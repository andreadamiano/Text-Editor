#ifndef TERMINAL_H
#define TERMINAL_H

#include <termios.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <stdint.h>
#include "arena.h"

typedef struct 
{
    //terminal cursor
    struct termios orig_termios;
    struct winsize terminal_size;
    uint8_t cursor_row;
    uint8_t cursor_col; 

    //content informations
    int32_t content_index;
    uint32_t content_size;
    uint8_t* displayed_content;
    int32_t* displayed_cols;
    int32_t row_offset;
    volatile sig_atomic_t screen_resized; //sig_atomic_t ensure that the varible is an integer type of a size that cna be accessed by the CPU in a single instruction

    //temporary arena, to store temporary informations overwritten every rendering cycle
    Arena_t* scratch_arena;

    //write and delete buffers
    uint8_t tmp_buffer[MAX_FILE_READ_CHUNK];
    int16_t tmp_buffer_screen_index;
    int16_t tmp_buffer_index;
    int16_t delete_buffer_screen_end_index;
    int16_t delete_buffer_len;

    //terminal lines
    int32_t line_size;
    int32_t next_line_size;
    int32_t prev_line_size;
    
}terminal_info_t;

extern terminal_info_t terminal_info;

void render_file_content(); 
void put_terminal_raw(); 
void restore_old_terminal(); 
void handle_resize(int sig);
void register_resize_handler(); 
void read_input(); 
int8_t write_to_tmp_buffer(uint32_t index, uint8_t ch); 
void add_to_delete_buffer(uint32_t index); 
void flush_tmp_buffer();
int get_utf8_char_length(uint8_t ch);

#endif