#include "twvm.h"
#include <stdlib.h>
#include <stdio.h>

#define len(arr) (int)( sizeof(arr) / sizeof((arr)[0]) )


static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}
#define expect_equiv(x,y) \
    if (!equiv(x,y)) dprintf(2, "%s=%g != %s=%g\n", #x,(double)x,#y,(double)y), __builtin_trap()

static void verify_(struct Builder *b, int n,
                    float const want[], float const *uniform, float *varying[]) {
    struct Program *p = compile(b);
    execute(p, n, uniform, varying);

    float const *got = varying[0];
    for (int i = 0; i < n; i++) {
        expect_equiv(got[i], want[i]);
    }
    free(p);
}
#define verify(b,want,uni,...) verify_(b,len(want),want,uni, (float*[]){__VA_ARGS__})

static void test_triple(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0),
            y = fmul(b, x, splat(b, 3.0f));
        store(b,0,y);
    }

    float v0[] = {1,2,3, 4, 5, 6},
        want[] = {3,6,9,12,15,18};
    verify(b,want,NULL,v0);
}

static void test_mutate(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0);
        mutate(b, &x, fmul(b, x, splat(b, 3.0f)));
        store(b,0,x);
    }

    float v0[] = {1,2,3, 4, 5, 6},
        want[] = {3,6,9,12,15,18};
    verify(b,want,NULL,v0);
}

static void test_flt(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0),
            y = load(b, 1);
        store(b,0, flt(b,x,y));
    }

    float const t = (union {int bits; float f;}){~0}.f;

    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {t,t,t,0,0,0};
    verify(b,want,NULL,v0,v1);
}

int main(void) {
    test_triple();
    test_mutate();
    test_flt();
    return 0;
}
