#ifndef B_TREE_H
#define B_TREE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"

#define MAX_INTERNAL_NODE MAX_FILE_CHUNK / 12

struct Node 
{
    struct InternalNode* parent;
    bool is_leaf;
};

struct InternalNode
{
    struct Node base;
    void* children[MAX_INTERNAL_NODE];  //can point to another internal node or a leaf node
    uint32_t child_size[MAX_INTERNAL_NODE];
    uint16_t child_count;
};

struct LeafNode
{
    struct Node base;
    uint8_t* content[MAX_FILE_CHUNK]; 
};

extern InternalNode* root; 

bool insert_node(uint32_t index, uint8_t* content);
bool insert_char_at_index(uint32_t index, uint8_t character);
bool insert_string_at_index(uint32_t index, uint8_t* string, uint32_t length);

#endif