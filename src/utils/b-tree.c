#include "b-tree.h"
#include <stdio.h>

struct InternalNode* root; 


bool insert_node(uint32_t index, uint8_t* content)
{
    if (root == NULL) return false;

    struct Node* current_node = root;
    uint32_t current_index = 0;

    while (true)
    {
        for (int i = 0; i < MAX_INTERNAL_NODE; ++i)
        {
            if (current_node->is_leaf) return false;
            
            struct InternalNode* current_node = (struct InternalNode*) current_node;
            if (current_index < current_index + current_node->child_size[i])
        }
    }
    

}