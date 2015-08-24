#include <stdio.h>
#include <inttypes.h>
#include "random.h"

#ifdef USE_MACRO
#define MIN(a, b) (((a) < (b)) ? (a) : (b))
#else
static inline unsigned MIN(unsigned a, unsigned b)
{
    return (a < b) ? a : b;
}
#endif

int main(void)
{
    genrand_init(44444);
    uint8_t a = (uint8_t) genrand_uint32();
    uint8_t b = (uint8_t) genrand_uint32();

    printf("minimum of %" PRIu8 " and %" PRIu8 ": %" PRIu8 "\n", a, b, (uint8_t) MIN(a, b));
    return 0;
}
