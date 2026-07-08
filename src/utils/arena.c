#include "arena.h"
#include <stdlib.h>

Arena* init_arena(size_t reserved_size)
{
    Arena* arena = (Arena*) malloc(sizeof(Arena));

    //align the requested size to the nearest page size multiple
    reserved_size = ((reserved_size + PAGE_SIZE - 1) / PAGE_SIZE) * PAGE_SIZE;

    //mmap lazily reserve a range in the virtual address space of the process 
    void* block = mmap(NULL, reserved_size, PROT_NONE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
    if (block == MAP_FAILED) 
    {
        free(arena);
        return NULL;
    }
    
    //initialize arena
    arena->base_ptr = (uint64_t*)block;
    arena->reserved_size = reserved_size;
    arena->commited_size = 0;
    arena->offset = 0;

    return arena;
}

void* request_block(Arena* arena, size_t size)
{
    if (!arena || !size)
        return NULL; 

    size_t new_offset = arena->offset + size; 

    //allign memory requests to 8 bytes 
    new_offset = (new_offset + 7) & ~7;

    if (new_offset > arena->reserved_size)
        return NULL;  

    //the os can commit only mulitples of page size bytes 
    //commmit n new pages only if the offset is bigger then the currently committed memory
    if (new_offset > arena->commited_size)
    {
        //allign new memory commit size
        size_t new_commited_size = (new_offset + PAGE_SIZE - 1) & ~(PAGE_SIZE - 1); //ceiling division

        //clamp new memory commit size to max reserved size 
        if (new_commited_size > arena->reserved_size) 
            new_commited_size = arena->reserved_size;

        size_t size_to_commit = new_commited_size - arena->commited_size;
        void* block_start_addr = arena->base_ptr + arena->commited_size;

        //mark the reserved space as writable and readable 
        if (mprotect(block_start_addr, size_to_commit, PROT_READ | PROT_WRITE) != 0) 
            return NULL;
    
        arena->commited_size = new_commited_size;
    }

    void* memory = arena->base_ptr + arena->offset; //return the immediate next available address inside the arena
    arena->offset = new_offset; //increment offset addng the size of the newly created block

    return memory; 
}


void arena_reset(Arena* arena)
{
    arena->offset = 0; //overwrite previously allocated memory
}

void free_arena(Arena* arena)
{
    munmap(arena->base_ptr, arena->commited_size);
    free(arena);
}