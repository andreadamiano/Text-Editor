#include "b-tree.h"
#include <stdio.h>
#include <memory.h>
#include <sys/param.h>

InternalNode_t* root; 
Arena_t* arena;


bool insert_string(uint32_t index, uint8_t* content, uint32_t content_size)
{
    Node_t* node = find_node_at_index(index);
    
    if (!node)
    {
        return false;
    } 

    insert_string_into_node((LeafNode_t*) node, index, content, content_size);
}

int32_t insert_string_into_node(LeafNode_t* leaf_node, uint32_t target_index, uint8_t* content, uint32_t content_size)
{
    uint32_t total_content_size = leaf_node->base.content_size + content_size;
    int32_t inserted_content;

    if (total_content_size <= MAX_FILE_CHUNK)
    {
        memmove(leaf_node->content + target_index + content_size, leaf_node->content + target_index, leaf_node->base.content_size - target_index); //shift the content before inserting 
        memcpy(leaf_node->content + target_index, content, content_size); //insert content
        leaf_node->base.content_size += content_size;
        inserted_content = content_size;
        update_size_from_node((Node_t*) leaf_node); 
    }
    else
    {
        uint16_t first_half_content_size = total_content_size / 2;
        uint16_t second_half_content_size = total_content_size - first_half_content_size;

        //the idea is to compute the new total size after insertion and split that in half between 2 nodes
        LeafNode_t* new_leaf = (LeafNode_t*) request_block(arena, sizeof(LeafNode_t));
        uint8_t tmp_buffer[2 * MAX_FILE_CHUNK];

        memcpy(tmp_buffer, leaf_node->content, target_index); //copy the first half
        memcpy(tmp_buffer + target_index, content, content_size); //copy the new content
        memcpy(tmp_buffer + target_index + content_size, leaf_node->content + target_index, leaf_node->base.content_size - target_index); //copy the remaining half

        //copy half the content in the current node 
        int16_t changed_size = first_half_content_size - leaf_node->base.content_size;
        memcpy(leaf_node->content, tmp_buffer, first_half_content_size);
        leaf_node->base.content_size = first_half_content_size;
        leaf_node->base.parent->base.content_size += changed_size;
        leaf_node->base.parent->child_size[leaf_node->base.parent_index] = changed_size; 
        leaf_node->base.parent->children[leaf_node->base.parent_index] = (Node_t*) leaf_node; 
        update_size_from_node((Node_t*) leaf_node->base.parent); 

        //copy the other half in the new leaf node
        memcpy(new_leaf->content, tmp_buffer, second_half_content_size);
        new_leaf->base.content_size = second_half_content_size;
        new_leaf->base.is_leaf = true;

        inserted_content = insert_child_node((Node_t*) new_leaf, leaf_node->base.parent, leaf_node->base.parent_index + 1);
    }

    return inserted_content;
}



InternalNode_t* split_internal_node(InternalNode_t* node, size_t parent_position)
{
    InternalNode_t* right_sibling = (InternalNode_t*) request_block(arena, sizeof(InternalNode_t));

    //split intermediate node
    uint8_t first_half_nodes = MAX_INTERNAL_NODE / 2;
    uint8_t second_half_nodes = MAX_INTERNAL_NODE - first_half_nodes;
    memcpy(right_sibling->children, node->children + first_half_nodes, second_half_nodes * sizeof(Node_t*));
    memset(node->children + first_half_nodes, 0, second_half_nodes * sizeof(Node_t*));
    node->child_count = first_half_nodes;
    right_sibling->child_count = second_half_nodes;

    //recompute the sizes of the splitted nodes
    // TODO: vectorize this operations
    size_t size_left = 0, size_right = 0;
    for (int i = 0; i < node->child_count; i++) size_left += node->child_size[i];
    for (int i = 0; i < right_sibling->child_count; i++) size_right += right_sibling->child_size[i];
    
    //append the splitted node to the parent node
    if (node == root)
    {
        InternalNode_t* new_root = (InternalNode_t*) request_block(arena, sizeof(InternalNode_t));
        node->base.parent = new_root;
        root = new_root;

        //insert left node
        insert_child_node((Node_t*) node, root, 0);
    }
    else
    {
        //update the parent
        node->base.parent->child_size[parent_position] = size_left;
        node->base.parent->base.content_size -= second_half_nodes;
        update_size_from_node((Node_t*) node->base.parent);
    }

    //insert right node
    insert_child_node((Node_t*) node, node->base.parent, node->base.parent_index + 1);
    return right_sibling;
}

/*
Inser a new node into a parent node with a specified position in the child's array
*/
size_t insert_child_node(Node_t* insert_node, InternalNode_t* parent_node, size_t parent_position)
{
    if (parent_node->child_count >= MAX_INTERNAL_NODE)
    {
        InternalNode_t* right_neighbor = split_internal_node(parent_node, parent_node->base.parent_index);

        if (parent_position >= MAX_INTERNAL_NODE / 2)
        {
            parent_node = right_neighbor;
            parent_position -= MAX_INTERNAL_NODE / 2;
        }
    }

    //shift all pointers before inserting the new leaf node
    memmove(parent_node->children + parent_position, parent_node->children + parent_position + 1, MAX((parent_node->child_count - parent_position), 0) * sizeof(LeafNode_t*));
    parent_node->children[parent_position] = insert_node;
    parent_node->child_count += 1;
    insert_node->parent_index = parent_position;
    parent_node->child_size[parent_position] = 0; //initialize the new size 

    //bubble up updating all the parent untill the root is reached
    update_size_from_node(insert_node);

    return insert_node->content_size;

}

void init_b_tree()
{
    arena = init_arena(MiB(10));
    root = (InternalNode_t*) request_block(arena, sizeof(Arena_t));
    return;
}

Node_t* find_node_at_index(uint32_t index)
{
    Node_t* current_node = (Node_t*) root;
    Node_t* prev_node;
    uint32_t current_index = 0;
    int i;

    start:
    while (current_node  && ! current_node->is_leaf )
    {
        for (i = 0; i < MAX_INTERNAL_NODE; ++i)
        {
            if (current_index <= current_index + ((InternalNode_t*) current_node)->child_size[i])
            {
                prev_node = current_node;
                current_node = ((InternalNode_t*) current_node)->children[i];
                goto start; 
            }

            current_index += ((InternalNode_t*) current_node)->child_size[i];
        }

        return NULL;
    }

    if (! current_node)
    {
        current_node = (Node_t*) request_block(arena, sizeof(Arena_t));
        current_node->parent = (InternalNode_t*) prev_node;
        current_node->is_leaf = true;
        current_node->parent_index = i;
        current_node->parent->child_count += 1;
        current_node->parent->children[i] = current_node;
    }

    return current_node;
    
}

inline void update_size_from_node(Node_t* node)
{
    Node_t* current_node = node;
    while (current_node != (Node_t*) root)
    {
        current_node->parent->child_size[current_node->parent_index] += current_node->content_size;
        current_node->parent->base.content_size += current_node->content_size;

        //bubble up
        current_node = (Node_t*) current_node->parent;
    }
    return;
}