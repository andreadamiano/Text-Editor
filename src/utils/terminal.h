#ifndef TERMINAL_H
#define TERMINAL_H

#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdint.h>
#include "arena.h"

typedef struct 
{
    struct termios orig_termios;
    struct winsize terminal_size;
    uint8_t cursor_row;
    uint8_t cursor_col; 
    uint8_t content_index;
    uint8_t content_size;
    uint8_t* displayed_content;
    uint8_t* displayed_cols;
    uint8_t row_offset;
    volatile sig_atomic_t screen_resized; //sig_atomic_t ensure that the varible is an integer type of a size that cna be accessed by the CPU in a single instruction
    Arena_t* scratch_arena;
}terminal_info_t;

extern terminal_info_t terminal_info;

void render_file_content(); 
void put_terminal_raw(); 
void restore_old_terminal(); 
void handle_resize(int sig);
void register_resize_handler(); 
void read_input(); 


#endif