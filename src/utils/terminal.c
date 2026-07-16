#define _POSIX_C_SOURCE 200809L
#include "file.h"
#include "terminal.h"
#include <stdio.h>
#include <errno.h>
#include <sys/param.h>

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
    terminal_info.curr_col = 0;
    terminal_info.curr_row = 0;

    //intialize arena
    terminal_info.scratch_arena = init_arena(KiB(8));

}

void restore_old_terminal()
{
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
    terminal_info.displayed_content = request_block(terminal_info.scratch_arena, terminal_info.terminal_size.ws_col * terminal_info.terminal_size.ws_row);
    
    //the extra slot acts as a safe boundary check to prevent out-of-bounds bugs
    terminal_info.displayed_cols = request_block(terminal_info.scratch_arena, terminal_info.terminal_size.ws_row+1); 

    //clear the screen, clear the scrollback history and move cursor to the top left position
    // printf("\e[2J\e[3J\e[H");
    printf("\e[2J\e[H");
    fflush(stdout); 

    int32_t node_index = file_info.curr_index;
    LeafNode_t* current_node = (LeafNode_t*) find_node_at_index(&node_index);

    char ch;
    uint8_t content_index = 0;
    uint8_t current_row = 0;

    for (int row = 0; row < terminal_info.terminal_size.ws_row; ++row)
    {
        for (int col = 0; col < terminal_info.terminal_size.ws_col; ++col)
        {
            if (!current_node)
                goto end;

            ch = current_node->content[node_index++];

            if (ch == '\n')
            {
                terminal_info.displayed_content[content_index++] = ch;
                break;
            }

            putchar(ch);
            terminal_info.displayed_content[content_index++] = ch;

            if (node_index >= current_node->base.content_size)
            {
                node_index = 0;
                current_node = current_node->next;
            }
        }

        putchar('\n');

        //if the current line was broken before reaching the \n was reached (becasue its bigger than the current horizontal size of the terminal) loop untill the next \n
        while (ch != '\n')
        {
            if (!current_node) 
                goto end;

            if (node_index >= current_node->base.content_size)
            {
                node_index = 0;
                current_node = current_node->next;
            }
            
            if (!current_node)
                goto end;

            ch = current_node->content[node_index++];
            terminal_info.displayed_content[content_index++] = ch;
        }

        terminal_info.displayed_cols[current_row] = content_index;
        current_row += 1;
        
    }

    end:
    terminal_info.displayed_content[content_index++] = '\n';
    terminal_info.displayed_cols[current_row] = content_index;

    //reset the cursor to the current position in the terminal
    printf("\e[%d;%dH", terminal_info.curr_row + 1, terminal_info.curr_col + 1);
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
                        if (terminal_info.displayed_content[MAX(terminal_info.displayed_cols[terminal_info.curr_row-1] + terminal_info.curr_col - 1, 0)] == '\n')
                        {
                            terminal_info.curr_row = MAX(terminal_info.curr_row - 1, 0);
                            terminal_info.curr_col = terminal_info.displayed_cols[terminal_info.curr_row] - terminal_info.displayed_cols[terminal_info.curr_row-1] - 1;
                            break;
                        }

                        if (terminal_info.curr_col <= 0) 
                        {
                            file_info.curr_index = MAX(file_info.curr_index - 1, 0);
                        }
                        else
                        {
                            terminal_info.curr_col = MAX(terminal_info.curr_col - 1, 0);
                        } 
                    
                        break;

                    //right arrow
                    case 67:
                        if (terminal_info.displayed_content[terminal_info.displayed_cols[terminal_info.curr_row-1] + terminal_info.curr_col] == '\n' )
                        {
                            if (!terminal_info.displayed_cols[terminal_info.curr_row+1])
                                break;

                            terminal_info.curr_row = MIN(terminal_info.curr_row + 1, terminal_info.terminal_size.ws_row);
                            terminal_info.curr_col = 0;
                            break;
                        }


                        if (terminal_info.curr_col >= terminal_info.terminal_size.ws_col) 
                        {
                            file_info.curr_index = MIN(file_info.curr_index + 1, terminal_info.terminal_size.ws_col);
                        }
                        else
                        {
                            terminal_info.curr_col = MIN(terminal_info.curr_col + 1, terminal_info.terminal_size.ws_col);
                        } 

                        break;
                    
                    //up arrow
                    case 65:
                        if (terminal_info.curr_row <= 0)
                        {
                            file_info.curr_index = MAX(file_info.curr_index - terminal_info.terminal_size.ws_col, file_info.curr_index);
                        }
                        else
                        {
                            terminal_info.curr_row = MAX(terminal_info.curr_row - 1, 0);
                        } 
                        break;
                        
                    //down arrow
                    case 66: 
                        if (terminal_info.curr_row >= terminal_info.terminal_size.ws_row)
                        {
                            file_info.curr_index = MIN(file_info.curr_index + terminal_info.terminal_size.ws_col, file_info.curr_index);
                        }
                        else
                        {
                            if (terminal_info.displayed_cols[terminal_info.curr_row + 1] != 0)
                            {
                                terminal_info.curr_row += 1;
                                terminal_info.curr_col = MIN(terminal_info.curr_col, terminal_info.displayed_cols[terminal_info.curr_row] - terminal_info.displayed_cols[terminal_info.curr_row -1] - 1);
                            }
                        } 
                        break;
                }
            }

        default:
            break;
    }
    
    }