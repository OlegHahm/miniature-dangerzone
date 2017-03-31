#include <stdlib.h>
#include <stdio.h>

#define CHUNK_SIZE  (256)
#define CHUNK_NO    (1000)
#define FREE_INTERVAL (2)

void *ptr[CHUNK_NO];

int main(void)
{
    for (unsigned i = 0; i < CHUNK_NO; i++) {
        ptr[i] = malloc(CHUNK_SIZE);
        printf("Allocated %ith piece at %p\n", i, ptr[i]);
        if (ptr[i] == NULL) {
            printf("Error allocating memory for the %ith chunk\n", i);
            break;
        }
        if ((i % FREE_INTERVAL) == 0) {
            puts("freeing");
            free(ptr[i - 17]);
        }
    }
    puts("done");
    return 0;
}
