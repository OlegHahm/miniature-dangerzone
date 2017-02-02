#include <stdio.h>
#include "fmt.h"

int main(void)
{
    printf("Test - ");
    print_u64_dec(123456);
    printf(" - done\n");
    return 0;
}
