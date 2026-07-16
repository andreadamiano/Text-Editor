#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include "utils/config.h"
#include "utils/arena.h"
#include "utils/b-tree.h"
#include "utils/file.h"
#include "utils/terminal.h"

volatile bool running = true;

int main(int argc, char* argv[]) 
{
    if(argc < 2)
    {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(0);
    }
    
    read_file_content(argv[1]);
    put_terminal_raw(); 
    register_resize_handler();
    render_file_content(); 

    while (running)
    {
        char ch;

        if (terminal_info.screen_resized) 
        {
            terminal_info.screen_resized = 0;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &terminal_info.terminal_size);
            render_file_content();
        }

        read_input();
        render_file_content();
    }

    restore_old_terminal(); 
    return 0;

}






