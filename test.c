#include "twvm.h"
#include <stdio.h>
#include <stdlib.h>

int internal_tests(void);

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
        int x = load(b,0),
            y = fmad(b,x,x,splat(b,3.0f));
        store(b,0,y);
    }
    float v0[] = {1,2,3,4,5,6};
    dump("fmad",b,len(v0),NULL,v0);
}

static void test_binops(void) {
#define M(x) {x,#x}
    struct {
        int (*fn)(struct Builder*, int,int);
        char const *name;
    } op[] = {
        M(fadd), M(fsub), M(fmul), M(fdiv),
        M(feq), M(flt),M(fle), M(fgt),M(fge),
        M(band), M(bor), M(bxor),
    };
#undef M

    for (int i = 0; i < len(op); i++) {
        struct Builder *b = builder();
        {
            int x = load(b,0),
                y = load(b,1);
            store(b,0,op[i].fn(b,x,y));
        }
        float v0[] = {1,2,3,4,5,6},
              v1[] = {4,4,4,4,4,4};
        dump(op[i].name,b,len(v0),NULL,v0,v1);
    }
}

static void test_mutate(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0);
        mutate(b,&x,fmul(b,x,splat(b,3.0f)));
        store(b,0,x);
    }
    float v0[] = {1,2,3,4,5,6};
    dump("mutate",b,len(v0),NULL,v0);
}

static void test_jump(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0);
        {
            int cond = fgt (b,x,splat(b,0.0f)),
                newx = bsel(b,cond
                             ,fsub(b,x,splat(b,1.0f))
                             ,x);
            mutate(b,&x,newx);
            jump(b,cond,cond);
        }
        store(b,0,x);
    }
    float v0[] = {1,2,3,4,5,6};
    dump("jump",b,len(v0),NULL,v0);
}

static void test_dce(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = fmul(b,x,x),
            z = fadd(b,x,x),
            w = fsub(b,z,x);
        (void)y;
        store(b,0,w);
    }
    float v0[] = {1,2,3,4,5,6};
    dump("dce",b,len(v0),NULL,v0);
}

static void test_uni(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = uniform(b,0),
            z = fadd(b,y,splat(b,1.0f)),
            w = fmul(b,x,z);
        store(b,0,w);
    }
    float v0[] = {1,2,3,4,5,6};
    float uniform = 3.0f;
    dump("uni",b,len(v0),&uniform,v0);
}

int main(void) {
    for (int rc = internal_tests(); rc;) {
        return rc;
    }
    test_fmad();
    test_binops();
    test_mutate();
    test_jump();
    test_dce();
    test_uni();
    return 0;
}
