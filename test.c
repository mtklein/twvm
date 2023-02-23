#include "twvm.h"
#include <stdlib.h>
#include <stdio.h>

#define len(arr) (int)( sizeof(arr) / sizeof((arr)[0]) )

static void dump_(char const *func, struct Builder *b, int n, float const *uni, float *var[]) {
    printf("%s\t", func);

    float const *v0 = var[0];
    for (int i = 0; i < n; i++) {
        printf(" %g", (double)v0[i]);
    }

    struct Program *p = compile(b);
    execute(p, n, uni, var);

    printf("\t~~>\t");
    for (int i = 0; i < n; i++) {
        printf(" %g", (double)v0[i]);
    }
    printf("\n");

    free(p);
}
#define dump(b,n,uni,...) dump_(__func__, b,n,uni, (float*[]){__VA_ARGS__})

static void test_triple(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0),
            y = fmul(b, x, splat(b, 3.0f));
        store(b,0,y);
    }
    float v0[] = {1,2,3,4,5,6};
    dump(b,len(v0),NULL,v0);
}

static void test_mutate(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0);
        mutate(b, &x, fmul(b, x, splat(b, 3.0f)));
        store(b,0,x);
    }
    float v0[] = {1,2,3,4,5,6};
    dump(b,len(v0),NULL,v0);
}

static void test_feq(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0),
            y = load(b, 1);
        store(b,0, feq(b,x,y));
    }
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4};
    dump(b,len(v0),NULL,v0,v1);
}
static void test_flt(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0),
            y = load(b, 1);
        store(b,0, flt(b,x,y));
    }
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4};
    dump(b,len(v0),NULL,v0,v1);
}

int main(void) {
    test_triple();
    test_mutate();
    test_feq();
    test_flt();
    return 0;
}
