#define _POSIX_C_SOURCE 200809L
#include "file.h"
#include "terminal.h"
#include <stdio.h>
#include <errno.h>

terminal_info_t terminal_info;
volatile sig_atomic_t screen_resized = 0;

void handle_resize(int sig) 
{
    screen_resized = 1;
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

    //clear the screen, clear the scrollback history and move cursor to the top left position
    // printf("\e[2J\e[3J\e[H");
    printf("\e[2J\e[H");
    fflush(stdout); 

    int32_t node_index = file_info.curr_index;
    LeafNode_t* current_node = (LeafNode_t*) find_node_at_index(&node_index);

    char ch;

    for (int row = 0; row < terminal_info.terminal_size.ws_row; ++row)
    {
        for (int col = 0; col < terminal_info.terminal_size.ws_col; ++col)
        {
            if (!current_node)
                return;

            ch = current_node->content[node_index++];

            if (ch == '\n')
                break;

            putchar(ch);
            // fflush(stdout); //debug 

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
                return;

            if (node_index >= current_node->base.content_size)
            {
                node_index = 0;
                current_node = current_node->next;
            }
            
            if (!current_node)
                return;

            ch = current_node->content[node_index++];
        }
        
    }

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

                        break;

                    //right arrow
                    case 67:
                        break;
                    
                    //up arrow
                    case 65:
                        break;
                        
                    //down arrow
                    case 66: 
                        break;
                }
            }

        default:
            break;
    }
    
}