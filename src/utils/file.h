#ifndef FILE_H
#define FILE_H

#include "config.h"
#include "b-tree.h"   
#include <stdint.h>
#include <emmintrin.h>

typedef struct
{
    char name[MAX_FILE_NAME_LEN];
    int open_fd;
    uint8_t  curr_index; 
    InternalNode_t* content_root; 
    uint8_t writing_buffer[MAX_FILE_CHUNK];
    uint8_t writing_buffer_index; 
    uint8_t read_buffer[MAX_FILE_CHUNK];
    uint32_t file_size;
}file_info_t;

extern file_info_t file_info;

bool read_file_content(uint8_t* file_name);  
bool insert_char(uint32_t index, uint8_t character);
bool insert_string(uint32_t index, uint8_t* string, uint32_t length);
bool delete_string(uint32_t start_index, uint32_t lenght);

#endif