#define _POSIX_C_SOURCE 200809L
#include "file.h"
#include "terminal.h"
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>
#include <memory.h>

terminal_info_t terminal_info;

void handle_resize(int sig) 
{
    terminal_info.screen_resized = 1;
}

void register_resize_handler()
{
    struct sigaction sa;
    sa.sa_handler = handle_resize;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGWINCH, &sa, NULL);
}

void put_terminal_raw()
{
    printf("\e[?1049h\e[H");
    fflush(stdout);

    tcgetattr(STDIN_FILENO, &terminal_info.orig_termios); 
    struct termios new_termios = terminal_info.orig_termios;

    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    //save current terminal dimensions
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal_info.terminal_size);

    //initialize terminal position
    terminal_info.cursor_col = 0;
    terminal_info.cursor_row = 0;
    terminal_info.row_offset = 0;
    terminal_info.content_index = 0;
    terminal_info.tmp_buffer_index = 0;
    terminal_info.tmp_buffer_screen_index = -1;

    //intialize arena
    terminal_info.scratch_arena = init_arena(KiB(24));

    //put terminal in full buffered mode (to avoid flushing when printing a newline)
    setvbuf(stdout, NULL, _IOFBF, BUFSIZ);

}

void restore_old_terminal()
{
    //restore standard line buffering mode
    setvbuf(stdout, NULL, _IOLBF, BUFSIZ);

    //restore original terminal settings 
    tcsetattr(STDIN_FILENO, TCSANOW, &terminal_info.orig_termios);

    printf("\e[?1049l");
    fflush(stdout);
}

void render_file_content()
{
    //do not render for extremely small terminal window sizes 
    if (terminal_info.terminal_size.ws_row < 1 || terminal_info.terminal_size.ws_col < 1)
    {
        return;
    }

    arena_reset(terminal_info.scratch_arena);
    terminal_info.displayed_content = request_block(terminal_info.scratch_arena, (terminal_info.terminal_size.ws_col * terminal_info.terminal_size.ws_row) * 4); //ensure UTF-8 safety
    
    //the extra slot acts as a safe boundary check to prevent out-of-bounds bugs
    terminal_info.displayed_cols = request_block(terminal_info.scratch_arena, terminal_info.terminal_size.ws_row+1); 

    //clear the screen, clear the scrollback history and move cursor to the top left position
    // printf("\e[2J\e[3J\e[H");
    printf("\e[2J\e[H");

    int32_t node_index = file_info.curr_index;
    LeafNode_t* current_node = (LeafNode_t*) find_node_at_index(&node_index, false);

    char ch;
    uint8_t content_index = 0;
    uint8_t current_row = 0;

    for (int row = 0; row < terminal_info.terminal_size.ws_row; ++row)
    {   

        for(int offset = 0; offset < terminal_info.row_offset; ++offset)
        {
            if ((ch = consume_char_from_node(&current_node, &node_index)) == '\0')
                goto end;

            terminal_info.displayed_content[content_index++] = ch;
            
            if (ch == '\n')
            {
                putchar('\n');
                goto end_loop;
            }
        }

        int col = 0;        
        while (col < terminal_info.terminal_size.ws_col)
        {
            //display tmp buffer
            if (content_index == terminal_info.tmp_buffer_screen_index)
            {
                for (int i = 0; i < terminal_info.tmp_buffer_index; ++i)
                {
                    putchar(terminal_info.tmp_buffer[i]);
                    terminal_info.displayed_content[content_index++] = terminal_info.tmp_buffer[i];

                    if ((terminal_info.tmp_buffer[i] & 0xC0) != 0x80) 
                    {
                        col++;
                    }
                }
            }

            if ((ch = consume_char_from_node(&current_node, &node_index)) == '\0')
                goto end;

            if (ch == '\n')
            {
                terminal_info.displayed_content[content_index++] = ch;
                break;
            }
            
            putchar(ch);
            terminal_info.displayed_content[content_index++] = ch;

            //if it is a UTF-8 continuation byte, do not increment the column counter
            if ((ch & 0xC0) != 0x80) 
            {
                col++;
            } 
        }

        putchar('\n');

        //if the current line was broken before reaching the \n was reached (becasue its bigger than the current horizontal size of the terminal) loop untill the next \n
        while (ch != '\n')
        {
            if ((ch = consume_char_from_node(&current_node, &node_index)) == '\0')
                goto end;
            
            terminal_info.displayed_content[content_index++] = ch;
        }

        end_loop:
        terminal_info.displayed_cols[current_row] = content_index-1;
        current_row += 1;
    }

    end:
    terminal_info.displayed_content[content_index++] = '\n';
    terminal_info.displayed_cols[current_row] = content_index;
    terminal_info.content_size = content_index-1;

    //reset the cursor to the current position in the terminal
    printf("\e[%d;%dH", terminal_info.cursor_row + 1, terminal_info.cursor_col + 1);
    fflush(stdout); 
}

void read_input()
{
    int ch = getchar();

    if (ch == EOF) {
        if (errno == EINTR) {
            clearerr(stdin); 
            return;        
        }
        running = false;
        return;
    }

    switch (ch)
    {
        case 'q':
            running = false;
            break;

        case '\e':
            char ch1 = getchar();
            char ch2 = getchar();

            if (ch1 == '[')
            {
                switch (ch2) 
                {   
                    //left arrow
                    case 68: 
                    {
                        if (terminal_info.content_index <= 0)
                            break;

                        uint8_t prev_line_size = terminal_info.displayed_cols[terminal_info.cursor_row-1] - terminal_info.displayed_cols[terminal_info.cursor_row-2];
                        int char_len = 0;
                        char current_char;
                        
                        while (terminal_info.content_index > 0)
                        {
                            current_char = terminal_info.displayed_content[--terminal_info.content_index];
                            ++char_len;

                            if ((current_char & 0xC0) != 0x80) 
                            {
                                break;
                            }
                        }

                        if (current_char == '\n')
                        {
                            terminal_info.cursor_row = MAX(terminal_info.cursor_row - 1, 0);
                            terminal_info.cursor_col = MIN(prev_line_size, terminal_info.terminal_size.ws_col);
                            terminal_info.row_offset = MAX(prev_line_size - terminal_info.terminal_size.ws_col,0);
                            terminal_info.content_index = MAX(terminal_info.content_index - 1, 0);
                            break;
                        }

                        // terminal_info.content_index = MAX(terminal_info.content_index - 1, 0);

                        if (terminal_info.cursor_col <= 0) 
                        {
                            terminal_info.row_offset = MAX(terminal_info.row_offset - 1, 0);
                        }
                        else
                        {
                            terminal_info.cursor_col = MAX(terminal_info.cursor_col - 1, 0);
                        } 
                    
                        break;
                    }
                    //right arrow
                    case 67:
                        if (terminal_info.content_index >= terminal_info.content_size)
                            break;

                        uint8_t line_size = terminal_info.displayed_cols[terminal_info.cursor_row] - terminal_info.displayed_cols[terminal_info.cursor_row-1];
                        char current_char = terminal_info.displayed_content[terminal_info.content_index];
                        int char_len = get_utf8_char_length(current_char);

                        if (current_char == '\n' )
                        {
                            terminal_info.row_offset = 0;
                            terminal_info.cursor_row +=  1;
                            terminal_info.cursor_col = 0;
                            terminal_info.content_index = MIN(terminal_info.content_index + char_len, terminal_info.content_size);
                            break;
                        }

                        terminal_info.content_index = MIN(terminal_info.content_index + char_len, terminal_info.content_size);

                        if (terminal_info.cursor_col >= terminal_info.terminal_size.ws_col - 1) 
                        {
                            terminal_info.row_offset = terminal_info.row_offset + 1;
                        }
                        else
                        {
                            terminal_info.cursor_col = MIN(terminal_info.cursor_col + 1, terminal_info.terminal_size.ws_col);
                        } 

                        break;
                    
                    //up arrow
                    case 65:

                        if (terminal_info.cursor_row <= 0)
                        {
                            file_info.curr_index = MAX(file_info.curr_index - terminal_info.terminal_size.ws_col, file_info.curr_index);
                        }
                        else
                        {
                            uint8_t line_size = terminal_info.displayed_cols[terminal_info.cursor_row] - terminal_info.displayed_cols[terminal_info.cursor_row-1];
                            uint8_t prev_line_size = terminal_info.displayed_cols[terminal_info.cursor_row-1] - terminal_info.displayed_cols[terminal_info.cursor_row-2];

                            terminal_info.cursor_row = MAX(terminal_info.cursor_row - 1, 0);
                            terminal_info.cursor_col = MIN(terminal_info.cursor_col, prev_line_size);
                            terminal_info.content_index = terminal_info.displayed_cols[terminal_info.cursor_row];
                            terminal_info.row_offset = MAX(prev_line_size - terminal_info.terminal_size.ws_col ,0);
                        } 
                        break;
                        
                    //down arrow
                    case 66: 

                        if (terminal_info.cursor_row >= terminal_info.terminal_size.ws_row)
                        {
                            file_info.curr_index = MIN(file_info.curr_index + terminal_info.terminal_size.ws_col, file_info.curr_index);
                        }
                        else
                        {
            
                            if (terminal_info.displayed_cols[terminal_info.cursor_row + 1] != 0)
                            {
                                uint8_t line_size = terminal_info.displayed_cols[terminal_info.cursor_row] - terminal_info.displayed_cols[terminal_info.cursor_row-1];
                                uint8_t next_line_size = terminal_info.displayed_cols[terminal_info.cursor_row+1] - terminal_info.displayed_cols[terminal_info.cursor_row];

                                terminal_info.cursor_row += 1;
                                terminal_info.cursor_col = MIN(terminal_info.cursor_col, next_line_size);
                                terminal_info.content_index += MIN(line_size + 1, line_size - terminal_info.content_index + terminal_info.cursor_col + 1);
                                terminal_info.row_offset = MAX(terminal_info.content_index - terminal_info.displayed_cols[terminal_info.cursor_row-1] - terminal_info.terminal_size.ws_col ,0);
                            }
                        } 
                        break;
                }
            }
            break;
        
        //delete button
        case 127:
            add_to_delete_buffer(terminal_info.content_index);
            break;

        //ctrl+s
        case 19:
            flush_tmp_buffer();
            write_to_disk();
            break;

        default:
            write_to_tmp_buffer(terminal_info.content_index, ch);
            break;
    }
    
}

int8_t write_to_tmp_buffer(uint8_t index, uint8_t ch)
{
    if (terminal_info.tmp_buffer_screen_index == -1)
    {
        terminal_info.tmp_buffer_screen_index = index;
        terminal_info.tmp_buffer_index = 0;
        terminal_info.tmp_buffer[terminal_info.tmp_buffer_index++] = ch;
    }
    else
    {
        if (index > terminal_info.tmp_buffer_index + terminal_info.tmp_buffer_screen_index || terminal_info.tmp_buffer_index + 1 > MAX_FILE_READ_CHUNK)
        {
            insert_string(terminal_info.tmp_buffer_screen_index + file_info.curr_index, terminal_info.tmp_buffer, terminal_info.tmp_buffer_index);
            terminal_info.tmp_buffer_screen_index = index;
            terminal_info.tmp_buffer_index = 0;
            terminal_info.tmp_buffer[terminal_info.tmp_buffer_index++] = ch;
        }
        else
        {
            uint8_t tmp_buffer_index = index - terminal_info.tmp_buffer_screen_index;
            memmove(terminal_info.tmp_buffer + tmp_buffer_index + 1, terminal_info.tmp_buffer + tmp_buffer_index, MIN(MAX_FILE_READ_CHUNK - 1 - tmp_buffer_index, terminal_info.tmp_buffer_index - tmp_buffer_index));
            terminal_info.tmp_buffer[tmp_buffer_index] = ch;
            ++terminal_info.tmp_buffer_index;
        }
    }
    uint8_t line_size = terminal_info.displayed_cols[terminal_info.cursor_row] - terminal_info.displayed_cols[terminal_info.cursor_row-1];
    terminal_info.content_index += 1;
    
    if ((ch & 0xC0) != 0x80) 
    {
        terminal_info.cursor_col = MIN(terminal_info.cursor_col + 1, terminal_info.terminal_size.ws_col);
        terminal_info.row_offset = MAX(line_size + 1 - terminal_info.terminal_size.ws_col, 0);
    }
}

void add_to_delete_buffer(uint8_t index)
{
    char current_char;
    int char_len = 0;
    while (terminal_info.content_index > 0)
    {
        current_char = terminal_info.displayed_content[--terminal_info.content_index];
        ++char_len;

        if ((current_char & 0xC0) != 0x80) 
        {
            break;
        }
    }

    if (terminal_info.tmp_buffer_index && (index - 1 >= terminal_info.tmp_buffer_screen_index && index - 1 <= terminal_info.tmp_buffer_screen_index + terminal_info.tmp_buffer_index))
    {   
        int remove_len = MIN(char_len, terminal_info.tmp_buffer_index);
        uint8_t tmp_buffer_index = index - terminal_info.tmp_buffer_screen_index;
        memmove(terminal_info.tmp_buffer + tmp_buffer_index - remove_len, terminal_info.tmp_buffer + tmp_buffer_index, MAX(0, terminal_info.tmp_buffer_index - tmp_buffer_index));
        terminal_info.tmp_buffer_index = MAX(terminal_info.tmp_buffer_index - remove_len, 0);
        char_len -= remove_len;
        index -= remove_len;
    }
    
    if (char_len)
    {
        delete_string(index, char_len);
    }
    
    uint8_t line_size = terminal_info.displayed_cols[terminal_info.cursor_row] - terminal_info.displayed_cols[terminal_info.cursor_row-1] - char_len;
    terminal_info.cursor_col = MAX(terminal_info.cursor_col - 1, 0);
    // terminal_info.content_index = MAX(terminal_info.content_index -1, 0); 
    terminal_info.row_offset = MAX(line_size + 1 - terminal_info.terminal_size.ws_col, 0);

    if (!terminal_info.tmp_buffer_index)
    {
        terminal_info.tmp_buffer_screen_index  = -1;
    }
}

void flush_tmp_buffer()
{
    insert_string(terminal_info.tmp_buffer_screen_index + file_info.curr_index, terminal_info.tmp_buffer, terminal_info.tmp_buffer_index); //flush to the b-tree
    terminal_info.tmp_buffer_screen_index = -1;
    terminal_info.tmp_buffer_index = 0;
}

int get_utf8_char_length(uint8_t ch) 
{
    if ((ch & 0x80) == 0x00) 
    {      
        return 1;                   
    } 
    else if ((ch & 0xE0) == 0xC0) 
    { 
        return 2;                   
    } 
    else if ((ch & 0xF0) == 0xE0) 
    { 
        return 3;                   
    } 
    else if ((ch & 0xF8) == 0xF0) 
    {
        return 4;                 
    }
    
    return 1; 
}