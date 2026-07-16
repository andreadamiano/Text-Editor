#ifndef TERMINAL_H
#define TERMINAL_H

#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>

typedef struct 
{
    struct termios orig_termios;
    struct winsize terminal_size;
}terminal_info_t;

extern terminal_info_t terminal_info;
extern volatile sig_atomic_t screen_resized;  //sig_atomic_t ensure that the varible is an integer type of a size that cna be accessed by the CPU in a single instruction


void render_file_content(); 
void put_terminal_raw(); 
void restore_old_terminal(); 
void handle_resize(int sig);
void register_resize_handler(); 
void read_input(); 


#endif