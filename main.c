#include <stdio.h>
#include "heap.h"
#include "custom_unistd.h"
#include "tested_declarations.h"
#include "rdebug.h"

int main() {
    heap_setup();
    heap_calloc(1,sizeof(char));
    heap_realloc(NULL,1);
    heap_get_largest_used_block_size();
    heap_clean();
    return 0;
}

