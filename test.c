#include "expect.h"
#include "twvm.h"
#include <stdio.h>
#include <stdlib.h>

void internal_tests(void);

static void test_nothing(void) {
    free(compile(builder(0)));
}

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

static void test_(struct Builder *b, float const want[], int n, void *ptr[]) {
    struct Program *p = compile(b);
    execute(p,n,ptr);
    free(p);
    float const *v0 = ptr[0];
    for (int i = 0; i < n; i++) {
        expect(equiv(v0[i], want[i]));
    }
}
#define test(b,want,...) test_(b,want,sizeof(want)/sizeof(0[want]), (void*[]){__VA_ARGS__})

static void test_fadd(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, fadd(b,x,y));
    }
    float v0[] = {1,2,3,4,5, 6},
          v1[] = {4,4,4,4,4, 4},
        want[] = {5,6,7,8,9,10};
    test(b,want,v0,v1);
}

static void test_fsub(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, fsub(b,x,y));
    }
    float v0[] = { 1, 2, 3, 4, 5, 6},
          v1[] = { 4, 4, 4, 4, 4, 4},
        want[] = {-3,-2,-1, 0, 1, 2};
    test(b,want,v0,v1);
}

static void test_fmul(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, fmul(b,x,y));
    }
    float v0[] = {1,2, 3, 4, 5, 6},
          v1[] = {4,4, 4, 4, 4, 4},
        want[] = {4,8,12,16,20,24};
    test(b,want,v0,v1);
}

static void test_fdiv(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, fdiv(b,x,y));
    }
    float v0[] = {   1,   2,    3,   4,    5,   6},
          v1[] = {   4,   4,    4,   4,    4,   4},
        want[] = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5};
    test(b,want,v0,v1);
}

static void test_fmad(void) {
    struct Builder *b = builder(1);
    {
        int x = load(b,0,thread_id(b)),
            y = fmul(b,x,x),
            z = fadd(b,y,splat(b,3.0f));
        store(b,0,z);
    }
    float v0[] = {1,2, 3, 4, 5, 6},
        want[] = {4,7,12,19,28,39};
    test(b,want,v0);
}


static void test_feq(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, feq(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {0,0,0,t,0,0};
    test(b,want,v0,v1);
}

static void test_flt(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, flt(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {t,t,t,0,0,0};
    test(b,want,v0,v1);
}

static void test_fle(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, fle(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {t,t,t,t,0,0};
    test(b,want,v0,v1);
}

static void test_fgt(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, fgt(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {0,0,0,0,t,t};
    test(b,want,v0,v1);
}

static void test_fge(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, fge(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {0,0,0,t,t,t};
    test(b,want,v0,v1);
}

static void test_band(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, band(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {0,0,t,t},
          v1[] = {0,t,0,t},
        want[] = {0,0,0,t};
    test(b,want,v0,v1);
}

static void test_bor(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, bor(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {0,0,t,t},
          v1[] = {0,t,0,t},
        want[] = {0,t,t,t};
    test(b,want,v0,v1);
}

static void test_bxor(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,thread_id(b));
        store(b,0, bxor(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {0,0,t,t},
          v1[] = {0,t,0,t},
        want[] = {0,t,t,0};
    test(b,want,v0,v1);
}

static void test_mutate(void) {
    struct Builder *b = builder(1);
    {
        int x = load(b,0,thread_id(b));
        mutate(b,&x,fmul(b,x,splat(b,3.0f)));
        store(b,0,x);
    }
    float v0[] = {1,2,3, 4, 5, 6},
        want[] = {3,6,9,12,15,18};
    test(b,want,v0);
}

static void test_jump(void) {
    struct Builder *b = builder(1);
    {
        int x = load(b,0,thread_id(b));
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
    float v0[] = {1,2,3,4,5,6},
        want[] = {0,0,0,0,0,0};
    test(b,want,v0);
}

static void test_dead_code(void) {
    struct Builder *b = builder(1);
    {
        int x = load(b,0,thread_id(b)),
            y = fmul(b,x,x),
            z = fadd(b,x,x),
            w = fsub(b,z,x);
        (void)y;
        store(b,0,w);
    }
    float v0[] = {1,2,3,4,5,6},
        want[] = {1,2,3,4,5,6};
    test(b,want,v0);
}

static void test_uniform_load(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,1,splat(b,0.0f)),
            z = fadd(b,y,splat(b,1.0f)),
            w = fmul(b,x,z);
        store(b,0,w);
    }
    float uni = 3.0f,
         v0[] = {1,2, 3, 4, 5, 6},
       want[] = {4,8,12,16,20,24};
    test(b,want,v0,&uni);
}

static void test_thread_id(void) {
    struct Builder *b = builder(1);
    {
        store(b,0, fmul(b, thread_id(b), splat(b,2.0f)));
    }
    float v0[] = {0,0,0,0,0, 0, 0},
        want[] = {0,2,4,6,8,10,12};
    test(b,want,v0);
}

static void test_complex_uniforms(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,1, splat(b,1.0f)),   // load uniform x =           uni[1] == 6.0f
            y = load(b,1, x),               // load uniform y = uni[x] == uni[6] == 9.0f
            z = load(b,0, thread_id(b));    // load varying z = v0
        store(b,0, fmul(b,y,z));            // store varying v0 = y*z == 9*z
    }
    float uni[] = {8,6,7,5,3,0,9},
           v0[] = {1, 2, 3, 4, 5, 6},
         want[] = {9,18,27,36,45,54};
    test(b,want,v0,uni);
}

static void test_gather(void) {
    struct Builder *b = builder(2);
    {
        int x = load(b,0,thread_id(b)),   // load varying x = v0[thread_id]
            y = load(b,1,x);              // load varying y = v1[x] == v1[v0[x]]
        store(b,0, y);
    }
    float v0[] = {1,2,3,4,5,6},
          v1[] = {0,-1,-2,-3,-4,-5,-6},
        want[] = {-1,-2,-3,-4,-5,-6};
    test(b,want,v0,v1);
}

int main(void) {
    internal_tests();

    test_nothing();

    test_fadd();
    test_fsub();
    test_fmul();
    test_fdiv();
    test_fmad();

    test_feq();
    test_flt();
    test_fle();
    test_fgt();
    test_fge();

    test_band();
    test_bor();
    test_bxor();

    test_mutate();
    test_jump();

    test_dead_code();
    test_uniform_load();
    test_thread_id();
    test_complex_uniforms();
    test_gather();
    return 0;
}
