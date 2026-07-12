#ifndef CONFIG_H
#define CONFIG_H

#define KiB(n) ((uint64_t)(n) << 10)  //1024
#define MiB(n) ((uint64_t)(n) << 20)  //1048576 
#define GiB(n) ((uint64_t)(n) << 30) //1073741824

#define MAX_FILE_NAME_LEN KiB(1)
// #define MAX_FILE_CHUNK KiB(4)
#define MAX_FILE_CHUNK 3

#endif
