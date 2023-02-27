#include "twvm.h"
#include <dlfcn.h>
#include <stdio.h>
#include <stdlib.h>

#define len(arr) (int)( sizeof(arr) / sizeof((arr)[0]) )

static void dump_(char const *func, struct Builder *b, int n, float const *uni, float *var[]) {
    float const *v0 = var[0];
    printf("%s\t", func);

    for (int i = 0; i < n; i++) {
        printf(" %g", (double)v0[i]);
    }
    printf("\t~~>\t");

    struct Program *p = compile(b);
    execute(p, n, uni, var);

    for (int i = 0; i < n; i++) {
        printf(" %g", (double)v0[i]);
    }
    printf("\n");

    free(p);
}
#define dump(func,b,n,uni,...) dump_(func, b,n,uni, (float*[]){__VA_ARGS__})

static void test_fmad(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0),
            y = fmad(b, x,x, splat(b,3.0f));
        store(b,0,y);
    }
    float v0[] = {1,2,3,4,5,6};
    dump("fmad",b,len(v0),NULL,v0);
}

static void test_binops(void) {
    int (*op[])(struct Builder*, int,int) = {
        fadd, fsub, fmul, fdiv,
        feq, flt,fle, fgt,fge,
        band, bor, bxor,
    };

    for (int i = 0; i < len(op); i++) {
        struct Builder *b = builder();
        {
            int x = load(b,0),
                y = load(b,1);
            store(b,0, op[i](b,x,y));
        }
        float v0[] = {1,2,3,4,5,6},
              v1[] = {4,4,4,4,4,4};
        Dl_info info;
        dladdr((void const*)op[i], &info);
        dump(info.dli_sname,b,len(v0),NULL,v0,v1);
    }
}

static void test_mutate(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0);
        mutate(b, &x, fmul(b, x, splat(b,3.0f)));
        store(b,0,x);
    }
    float v0[] = {1,2,3,4,5,6};
    dump("mutate",b,len(v0),NULL,v0);
}

static void test_jump(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0);
        {
            int cond = fgt (b, x, splat(b,0.0f)),
                newx = bsel(b, cond
                             , fsub(b, x, splat(b,1.0f))
                             , x);
            mutate(b, &x, newx);
            jump(b, cond, cond);
        }

        store(b,0,x);
    }
    float v0[] = {1,2,3,4,5,6};
    dump("jump",b,len(v0),NULL,v0);
}

int main(void) {
    test_fmad();
    test_binops();
    test_mutate();
    test_jump();
    return 0;
}
