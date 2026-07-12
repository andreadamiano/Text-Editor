#ifndef B_TREE_H
#define B_TREE_H

#include <stdint.h>
#include <stdbool.h>
#include "config.h"
#include "arena.h"

typedef struct InternalNode InternalNode_t;

typedef struct 
{
    struct InternalNode* parent;
    bool is_leaf;
    size_t parent_index;
    uint32_t content_size;
}Node_t;

typedef struct 
{
    Node_t base;
    uint8_t content[MAX_FILE_CHUNK]; 
}LeafNode_t;

// #define MAX_INTERNAL_NODE ((MAX_FILE_CHUNK - sizeof(uint16_t)) / (sizeof(Node_t*) + sizeof(uint32_t)))
#define MAX_INTERNAL_NODE 12

struct InternalNode
{
    Node_t base;
    uint16_t child_count;
    uint32_t child_size[MAX_INTERNAL_NODE];
    Node_t* children[MAX_INTERNAL_NODE]; 
};

extern InternalNode_t* root; 
extern Arena_t* arena;

bool insert_char(uint32_t index, uint8_t character);
bool insert_string(uint32_t index, uint8_t* string, uint32_t length);
int32_t insert_string_into_node(LeafNode_t* node, uint32_t target_index, uint8_t* content, uint32_t content_size);
size_t insert_child_node(Node_t* insert_node, InternalNode_t* start_node, size_t parent_position);
InternalNode_t* split_internal_node(InternalNode_t* node, size_t parent_position);
void init_b_tree(); 
Node_t* find_node_at_index(uint32_t index);
void update_size_from_node(Node_t* node);

#endif