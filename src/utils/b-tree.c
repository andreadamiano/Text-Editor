#include "b-tree.h"
#include <stdio.h>
#include <memory.h>
#include <sys/param.h>

InternalNode_t* root; 
Arena_t* arena;


int32_t insert_string_into_node(LeafNode_t* leaf_node, uint32_t target_index, uint8_t* content, uint32_t content_size)
{
    uint32_t total_content_size = leaf_node->base.content_size + content_size;
    int32_t inserted_content = 0;

    if (total_content_size <= MAX_FILE_CHUNK)
    {
        memmove(leaf_node->content + target_index + content_size, leaf_node->content + target_index, MAX(leaf_node->base.content_size - target_index, 0)); //shift the content before inserting 
        memcpy(leaf_node->content + target_index, content, content_size); //insert content  
        leaf_node->base.content_size += content_size;
        inserted_content = content_size;
        update_size_from_node((Node_t*) leaf_node, inserted_content); 
    }
    else
    {
        LeafNode_t* new_leaf = (LeafNode_t*) request_block(arena, sizeof(LeafNode_t));

        //the idea is to compute the new total size after insertion and split that in half between 2 nodes
        uint16_t first_half_content_size = total_content_size / 2;
        uint16_t second_half_content_size = total_content_size - first_half_content_size;
    
        if (total_content_size < 2 * MAX_FILE_CHUNK || target_index < MAX_FILE_CHUNK )
        {
            uint8_t tmp_buffer[2 * MAX_FILE_CHUNK];

            memcpy(tmp_buffer, leaf_node->content, target_index); //copy the first half
            memcpy(tmp_buffer + target_index, content, content_size); //copy the new content
            memcpy(tmp_buffer + target_index + content_size, leaf_node->content + target_index, leaf_node->base.content_size - target_index); //copy the remaining half

            //copy half the content in the current node 
            int16_t changed_size = first_half_content_size - leaf_node->base.content_size;
            memcpy(leaf_node->content, tmp_buffer, first_half_content_size);
            leaf_node->base.content_size = first_half_content_size;
            update_size_from_node((Node_t*) leaf_node, changed_size); 

            //copy the other half in the new leaf node
            memcpy(new_leaf->content, tmp_buffer + first_half_content_size, second_half_content_size);
            new_leaf->base.content_size = second_half_content_size;
            new_leaf->base.is_leaf = true;
        }
        else
        {
            //copy content inside the new leaf
            memcpy(new_leaf->content, content, content_size);
            new_leaf->base.content_size = content_size;
            new_leaf->base.is_leaf = true;
        }

        inserted_content = insert_child_node((Node_t*) new_leaf, leaf_node->base.parent, leaf_node->base.parent_index + 1);
    }

    return inserted_content;
}



InternalNode_t* split_internal_node(InternalNode_t* node, size_t parent_position)
{
    InternalNode_t* right_sibling = (InternalNode_t*) request_block(arena, sizeof(InternalNode_t));

    //split intermediate node
    uint8_t first_half_nodes = MAX_CHILD_NODE / 2;
    uint8_t second_half_nodes = MAX_CHILD_NODE - first_half_nodes;
    memcpy(right_sibling->children, node->children + first_half_nodes, second_half_nodes * sizeof(Node_t*));
    memcpy(right_sibling->child_size, node->child_size + first_half_nodes, second_half_nodes * sizeof(int32_t));
    memset(node->children + first_half_nodes, 0, second_half_nodes * sizeof(Node_t*));
    memset(node->child_size + first_half_nodes, 0, second_half_nodes * sizeof(int32_t));
    node->child_count = first_half_nodes;
    right_sibling->child_count = second_half_nodes;

    //update moved children to point to the right sibiling
    for (int i =  0; i < right_sibling->child_count; ++i)
    {
        right_sibling->children[i]->parent = right_sibling;
        right_sibling->children[i]->parent_index = i;
    }

    //recompute the sizes of the splitted nodes
    // TODO: vectorize this operations
    size_t size_left = 0, size_right = 0;
    for (int i = 0; i < node->child_count; i++) size_left += node->child_size[i];
    for (int i = 0; i < right_sibling->child_count; i++) size_right += right_sibling->child_size[i];
    node->base.content_size = size_left;
    right_sibling->base.content_size = size_right;
    
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
        //update the size of the parents
        node->base.parent->child_size[parent_position] = size_left;
        node->base.parent->base.content_size -= size_right;
        update_size_from_node((Node_t*) node->base.parent, -size_right);
    }

    //insert right node
    insert_child_node((Node_t*) right_sibling, node->base.parent, node->base.parent_index + 1);

    return right_sibling;
}

/*
Insert a new node into a parent node with a specified position in the child's array
*/
size_t insert_child_node(Node_t* insert_node, InternalNode_t* parent_node, size_t parent_position)
{
    if (parent_node->child_count >= MAX_CHILD_NODE)
    {
        InternalNode_t* right_neighbor = split_internal_node(parent_node, parent_node->base.parent_index);

        if (parent_position > MAX_CHILD_NODE / 2)
        {
            parent_node = right_neighbor;
            parent_position -= MAX_CHILD_NODE / 2;
        }
    }

    //shift all pointers before inserting the new leaf node
    memmove(parent_node->children + parent_position + 1, parent_node->children + parent_position, MAX((parent_node->child_count - parent_position), 0) * sizeof(LeafNode_t*));
    memmove(parent_node->child_size + parent_position + 1, parent_node->child_size + parent_position, MAX((parent_node->child_count - parent_position), 0) * sizeof(uint32_t));
    parent_node->child_size[parent_position] = 0; //reset the size before updating it
    parent_node->children[parent_position] = insert_node;
    parent_node->child_count += 1;
    insert_node->parent_index = parent_position;
    insert_node->parent = parent_node;

    //update parent indices
    for (int i = parent_position + 1; i < parent_node->child_count; ++i)
    {
        parent_node->children[i]->parent_index = i; 
    }

    //bubble up updating all the parent untill the root is reached
    update_size_from_node(insert_node, insert_node->content_size);

    if (insert_node->is_leaf)
    {
        link_leaf(insert_node); 
    }

    return insert_node->content_size;

}

InternalNode_t* init_b_tree()
{
    arena = init_arena(MiB(10));
    root = (InternalNode_t*) request_block(arena, sizeof(InternalNode_t));
    return root;
}

Node_t* find_node_at_index(uint32_t* index, bool insert)
{
    Node_t* current_node = (Node_t*) root;
    Node_t* prev_node;
    int i;

    start:
    while (current_node  && ! current_node->is_leaf )
    {
        for (i = 0; i < MAX_CHILD_NODE; ++i)
        {
            // if (*index <=  ((InternalNode_t*) current_node)->child_size[i] || ((InternalNode_t*) current_node)->child_size[i] == 0)
            if (!((InternalNode_t*) current_node)->children[i] || (*index <=  ((InternalNode_t*) current_node)->child_size[i] && (((InternalNode_t*) current_node)->children[i]->content_size || insert)) )
            {
                prev_node = current_node;
                current_node = ((InternalNode_t*) current_node)->children[i];
                goto start; 
            }

            *index -= ((InternalNode_t*) current_node)->child_size[i];
        }
        
        return NULL;

    }

    if (! current_node)
    {
        current_node = (Node_t*) request_block(arena, sizeof(LeafNode_t));
        current_node->parent = (struct InternalNode*) prev_node;
        current_node->is_leaf = true;
        current_node->parent_index = i;
        current_node->parent->child_count += 1;
        current_node->parent->children[i] = current_node;

        //create a linked list of leaves (for fast rendering)
        link_leaf(current_node);
            
    }

    return current_node;
    
}

inline void update_size_from_node(Node_t* node, int32_t inserted_len)
{
    if (!inserted_len) return;

    Node_t* current_node = node;
    while (current_node && current_node != (Node_t*) root)
    {
        current_node->parent->child_size[current_node->parent_index] += inserted_len;
        current_node->parent->base.content_size += inserted_len;

        //bubble up
        current_node = (Node_t*) current_node->parent;
    }
    return;
}

void link_leaf(Node_t* current_node)
{
    if (current_node->parent_index > 0) //connect left child
    {
        ((LeafNode_t*) current_node)->prev = (LeafNode_t*) current_node->parent->children[current_node->parent_index - 1];
        
    }
    else if (current_node->parent->base.parent_index > 0) //connect left cousin
    {
        InternalNode_t* grandparent = current_node->parent->base.parent;
        InternalNode_t* left_uncle = (InternalNode_t*) grandparent->children[current_node->parent->base.parent_index - 1];

        if (left_uncle && left_uncle->child_count > 0)
        {
            ((LeafNode_t*) current_node)->prev = (LeafNode_t*) left_uncle->children[left_uncle->child_count - 1];
        }
    }

    if (((LeafNode_t*) current_node)->prev) //if prev node was found connect it with current node
    {
        ((LeafNode_t*) current_node)->prev->next = (LeafNode_t*) current_node;
    }

    if (current_node->parent_index + 1 < current_node->parent->child_count) //connect right child
    {
        ((LeafNode_t*) current_node)->next = (LeafNode_t*) current_node->parent->children[current_node->parent_index + 1];
    }
    else if (current_node->parent->base.parent && current_node->parent->base.parent_index + 1 < current_node->parent->base.parent->child_count) //connect right cousin
    {
        InternalNode_t* grand_parent = current_node->parent->base.parent;
        InternalNode_t* right_uncle = (InternalNode_t*) grand_parent->children[current_node->parent->base.parent_index + 1];
        
        if (right_uncle && right_uncle->child_count > 0)
        {
            ((LeafNode_t*) current_node)->next = (LeafNode_t*) right_uncle->children[0];
        }
    }

    if (((LeafNode_t*) current_node)->next) //if next node was found connect it back with current node 
    {
        ((LeafNode_t*) current_node)->next->prev = (LeafNode_t*) current_node;
    }
}

uint8_t consume_char_from_node(LeafNode_t** node, int32_t* node_index)
{
    while (*node && (*node_index >= (*node)->base.content_size || !(*node)->base.content_size))
    {
        *node_index = 0;
        *node = (*node)->next;
    }

    if(!(*node))
        return '\0';

    return (*node)->content[(*node_index)++];
}

void delete_string_from_node(LeafNode_t* node, uint32_t target_index, uint32_t delete_size)
{
    uint32_t deleted_size =0;

    while ((delete_size - deleted_size) && node)
    {
        deleted_size = MIN(delete_size, node->base.content_size);
        node->base.content_size = MAX(node->base.content_size - delete_size, 0);

        //shift content to replace the deleted content
        memmove(node->content + target_index, node->content + MIN(target_index + delete_size, MAX_FILE_CHUNK -1), node->base.content_size);

        update_size_from_node((Node_t*) node, -deleted_size);

        node = node->next;
        delete_size -= deleted_size;
        deleted_size = 0;
    }

}