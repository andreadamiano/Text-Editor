#include <stdio.h>
#include "utils/config.h"
#include "utils/arena.h"
#include "utils/b-tree.h"

char* file; 

int main(int argc, char* argv[]) 
{
    // if(argc == 2)
    // {

    // }
    init_b_tree(); 

    char* string1 = "cia";
    char* string2 = "com";
    char* string3 = "va?";
    insert_string(0, string1, 3);
    insert_string(0, string2, 3);
    insert_string(0, string3, 3);

}