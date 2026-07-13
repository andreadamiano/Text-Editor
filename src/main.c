#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <unistd.h>
#include <signal.h>
#include <errno.h>
#include "utils/config.h"
#include "utils/arena.h"
#include "utils/b-tree.h"
#include "utils/file.h"


volatile sig_atomic_t screen_resized = 0; 

void handle_resize(int sig) 
{
    screen_resized = 1;
}


int main(int argc, char* argv[]) 
{
    if(argc < 2)
    {
        printf("Usage: %s <filename>\n", argv[0]);
        exit(0);
    }
    char ch;

    //register signal handler
    struct sigaction sa;
    sa.sa_handler = handle_resize;
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = 0; 
    sigaction(SIGWINCH, &sa, NULL);

    read_file_content(argv[1]);
    put_terminal_raw(); 
    render_file_content(); 

    while (1)
    {
        if (screen_resized) 
        {
            screen_resized = 0;
            ioctl(STDOUT_FILENO, TIOCGWINSZ, &info.terminal_size);
            render_file_content();
        }

        int ch = getchar();

        if (ch == EOF) {
            if (errno == EINTR) {
                clearerr(stdin); // Clear the interrupted state from stdin
                continue;        // Jump back to the top of the loop to process the resize!
            }
            break; // A real error or actual EOF happened, exit loop
        }

        if (ch == 'q') {
            break;
        }

    }

    restore_old_terminal(); 
    return 0;

}






