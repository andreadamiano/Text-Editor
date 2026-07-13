#include "file.h"
#include <fcntl.h>
#include <termios.h>
#include <stdio.h>

file_info_t info;

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
    if (!info.content_root)
    {
        info.content_root = init_b_tree(); 
    }

    info.open_fd = open(file_name, O_RDWR | O_APPEND | O_CREAT, 0644);
    uint32_t curr_index = 0; 
    uint8_t bytes_read; 

    while ((bytes_read = read(info.open_fd, info.read_buffer, MAX_FILE_CHUNK)) > 0)
    {
        insert_string(curr_index, info.read_buffer, bytes_read);
        curr_index += bytes_read;
    }
    


}

void put_terminal_raw()
{
    tcgetattr(STDIN_FILENO, &info.orig_termios); 
    struct termios new_termios = info.orig_termios;

    new_termios.c_lflag &= ~(ICANON | ECHO);
    new_termios.c_cc[VMIN] = 1;
    new_termios.c_cc[VTIME] = 0;
    tcsetattr(STDIN_FILENO, TCSANOW, &new_termios);

    //save current terminal dimensions
    ioctl(STDOUT_FILENO, TIOCGWINSZ, &info.terminal_size);

}

void restore_old_terminal()
{
    tcsetattr(STDIN_FILENO, TCSANOW, &info.orig_termios);
}

void render_file_content()
{
    //clear the screen and move cursor to the top left position
    printf("\e[2J\e[H");

    int32_t node_index = info.curr_index;
    LeafNode_t* current_node = (LeafNode_t*) find_node_at_index(&node_index);

    char ch;

    for (int row = 0; row < info.terminal_size.ws_row; ++row)
    {
        for (int col = 0; col < info.terminal_size.ws_col; ++col)
        {
            if (!current_node)
            {
                return;
            }

            ch = current_node->content[node_index++];

            if (ch == '\n')
                break;

            putchar(ch);
            fflush(stdout); //debug 

            if (node_index >= current_node->base.content_size)
            {
                node_index = 0;
                current_node = current_node->next;
            }
        }
        putchar('\n');

        while ((ch = current_node->content[node_index++]) != '\n')
        {
            if (node_index >= current_node->base.content_size)
            {
                node_index = 0;
                current_node = current_node->next;
            }
        }
        
    }
}