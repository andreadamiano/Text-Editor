#include "file.h"
#include "terminal.h"
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>
#include <string.h>

file_info_t file_info;

bool insert_char(uint32_t index, uint8_t character)
{

}

bool insert_string(uint32_t index, uint8_t* content, uint32_t content_size)
{
    Node_t* node = find_node_at_index(&index, true);
    
    if (!node)
    {
        return false;
    } 

    insert_string_into_node((LeafNode_t*) node, index, content, content_size);
}

bool delete_string(uint32_t end_index, uint32_t lenght)
{
    Node_t* node = find_node_at_index(&end_index, false);
    
    if (!node || !end_index)
    {
        return false;
    } 

    delete_string_from_node((LeafNode_t*) node, end_index-lenght, lenght);
    return true;
}

bool read_file_content(uint8_t* file_name)
{
    if (!file_info.content_root)
    {
        file_info.content_root = init_b_tree(); 
    }

    file_info.open_fd = open(file_name, O_RDWR | O_CREAT, 0644);

    size_t filename_len = strlen(file_name);
    file_info.open_filename = request_block(arena, filename_len);
    memcpy(file_info.open_filename, file_name, filename_len);
    uint32_t curr_index = 0; 
    uint8_t bytes_read; 

    while ((bytes_read = read(file_info.open_fd, file_info.read_buffer, MAX_FILE_READ_CHUNK)) > 0)
    {
        insert_string(curr_index, file_info.read_buffer, bytes_read);
        curr_index += bytes_read;
        file_info.file_size += bytes_read;
    }
    
}

bool write_to_disk()
{
    uint32_t index = 0;
    LeafNode_t* leaf_node = (LeafNode_t*) find_node_at_index(&index, false);

    if (!leaf_node)
    {
        return false;
    } 

    //open a temporary file where the new content will be dumped
    int tmp_fd; 
    char* tmp_filename;
    open_temporary_file(&tmp_fd, &tmp_filename);

    uint16_t bytes_written;
    while (leaf_node)
    {
        bytes_written = consume_str_from_node(&leaf_node, &index, MAX_FILE_WRITE_CHUNK, file_info.write_buffer);
        write(tmp_fd, file_info.write_buffer, bytes_written);
    }

    rename(tmp_filename, file_info.open_filename);
}

void open_temporary_file(int* tmp_fd, char** tmp_filename)
{
    *tmp_filename = request_block(terminal_info.scratch_arena, strlen(file_info.open_filename) + 6);

    //split the relative path with the file name
    char* slash = strrchr(file_info.open_filename, '/');

    if (!slash)
    {
        sprintf(*tmp_filename ,".%s.tmp", file_info.open_filename);
    }
    else 
    {
        size_t dirname_len = (slash - file_info.open_filename ) + 1;
        strncpy(*tmp_filename, file_info.open_filename, dirname_len);
        sprintf(*tmp_filename + dirname_len, ".%s.tmp", file_info.open_filename + dirname_len);
    }
    
    *tmp_fd = open(*tmp_filename, O_CREAT | O_WRONLY | O_TRUNC, 0644);


}