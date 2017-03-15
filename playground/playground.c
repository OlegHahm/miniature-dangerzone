#include <stdio.h>
#include <sys/time.h>
#include "xtimer.h"

typedef struct {
    int a;
    int b;
} foo_t;

typedef struct {
    int a;
    int b;
    int c;
} bar_t;

void foobar(foo_t *f)
{
    printf("a is %i - b is %i\n", f->a, f->b);
}


int main(void)
{
    bar_t b = { 1, 2, 3};
    foobar(&b);
    return 0;
}
