#include "file.h"
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

file_info_t file_info;

bool insert_char(uint32_t index, uint8_t character)
{

}

bool insert_string(uint32_t index, uint8_t* content, uint32_t content_size)
{
    Node_t* node = find_node_at_index(&index);
    
    if (!node)
    {
        return false;
    } 

    insert_string_into_node((LeafNode_t*) node, index, content, content_size);
}

bool read_file_content(uint8_t* file_name)
{
    if (!file_info.content_root)
    {
        file_info.content_root = init_b_tree(); 
    }

    file_info.open_fd = open(file_name, O_RDWR | O_APPEND | O_CREAT, 0644);
    uint32_t curr_index = 0; 
    uint8_t bytes_read; 

    while ((bytes_read = read(file_info.open_fd, file_info.read_buffer, MAX_FILE_CHUNK)) > 0)
    {
        insert_string(curr_index, file_info.read_buffer, bytes_read);
        curr_index += bytes_read;
        file_info.file_size += bytes_read;
    }
    
}