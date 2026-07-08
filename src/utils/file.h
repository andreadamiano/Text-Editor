#ifndef FILE_H
#define FILE_H

#include "config.h"
#include <stdint.h>
#include <emmintrin.h>

struct file_info
{
    char name[MAX_FILE_NAME_LEN];
    int open_fd;
    uint16_t curr_line; 
    char** content; 
    char* line_content;

    __m128 prova;
};

#endif