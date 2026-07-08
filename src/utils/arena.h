#include <stddef.h>
#include <stdint.h>
#include <sys/mman.h>
#include <unistd.h>

#define PAGE_SIZE (size_t)sysconf(_SC_PAGESIZE)
#define KiB(n) ((uint64_t)(n) << 10)  //1024
#define MiB(n) ((uint64_t)(n) << 20)  //1048576 
#define GiB(n) ((uint64_t)(n) << 30) //1073741824

typedef struct Arena
{
    uint64_t offset; //represent the offset from the base pointer to the end of the current allocated space
    size_t reserved_size; 
    size_t commited_size; 
    uint64_t* base_ptr; 
}Arena;


void* request_block(Arena* arena, size_t size); 
void free_arena(Arena* arena); 
Arena* init_arena(size_t reserved_size); 
void arena_reset(Arena* arena); 