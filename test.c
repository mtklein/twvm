#include "expect.h"
#include "twvm.h"
#include <stdio.h>
#include <stdlib.h>

void internal_tests(void);

#define len(x) (int)(sizeof(x) / sizeof(0[x]))

static _Bool equiv(float x, float y) {
    return (x <= y && y <= x)
        || (x != x && y != y);
}

static void test(struct Builder *b, int n, float const *uni, float *var[], float const want[]) {
    struct Program *p = compile(b);
    execute(p,n,uni,var);
    free(p);
    for (int i = 0; i < n; i++) {
        if (!equiv(var[0][i], want[i])) {
            dprintf(2, "got %g, want %g\n", (double)var[0][i], (double)want[i]);
            __builtin_trap();
        }
    }
}

static void test_fadd(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, fadd(b,x,y));
    }
    float v0[] = {1,2,3,4,5, 6},
          v1[] = {4,4,4,4,4, 4},
        want[] = {5,6,7,8,9,10};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_fsub(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, fsub(b,x,y));
    }
    float v0[] = { 1, 2, 3, 4, 5, 6},
          v1[] = { 4, 4, 4, 4, 4, 4},
        want[] = {-3,-2,-1, 0, 1, 2};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_fmul(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, fmul(b,x,y));
    }
    float v0[] = {1,2, 3, 4, 5, 6},
          v1[] = {4,4, 4, 4, 4, 4},
        want[] = {4,8,12,16,20,24};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_fdiv(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, fdiv(b,x,y));
    }
    float v0[] = {   1,   2,    3,   4,    5,   6},
          v1[] = {   4,   4,    4,   4,    4,   4},
        want[] = {0.25, 0.5, 0.75, 1.0, 1.25, 1.5};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_fmad(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = fmad(b,x,x,splat(b,3.0f));
        store(b,0,y);
    }
    float v0[] = {1,2, 3, 4, 5, 6},
        want[] = {4,7,12,19,28,39};
    test(b, len(v0), NULL, (float*[]){v0}, want);
}


static void test_feq(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, feq(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {0,0,0,t,0,0};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_flt(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, flt(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {t,t,t,0,0,0};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_fle(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, fle(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {t,t,t,t,0,0};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_fgt(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, fgt(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {0,0,0,0,t,t};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_fge(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, fge(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {1,2,3,4,5,6},
          v1[] = {4,4,4,4,4,4},
        want[] = {0,0,0,t,t,t};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_band(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, band(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {0,0,t,t},
          v1[] = {0,t,0,t},
        want[] = {0,0,0,t};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_bor(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, bor(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {0,0,t,t},
          v1[] = {0,t,0,t},
        want[] = {0,t,t,t};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_bxor(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = load(b,1);
        store(b,0, bxor(b,x,y));
    }
    float const t = 0.0f/0.0f;
    float v0[] = {0,0,t,t},
          v1[] = {0,t,0,t},
        want[] = {0,t,t,0};
    test(b, len(v0), NULL, (float*[]){v0,v1}, want);
}

static void test_mutate(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0);
        mutate(b,&x,fmul(b,x,splat(b,3.0f)));
        store(b,0,x);
    }
    float v0[] = {1,2,3, 4, 5, 6},
        want[] = {3,6,9,12,15,18};
    test(b, len(v0), NULL, (float*[]){v0}, want);
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
    float v0[] = {1,2,3,4,5,6},
        want[] = {0,0,0,0,0,0};
    test(b, len(v0), NULL, (float*[]){v0}, want);
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
    float v0[] = {1,2,3,4,5,6},
        want[] = {1,2,3,4,5,6};
    test(b, len(v0), NULL, (float*[]){v0}, want);
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
    float uni = 3.0f,
         v0[] = {1,2, 3, 4, 5, 6},
       want[] = {4,8,12,16,20,24};
    test(b, len(v0), &uni, (float*[]){v0}, want);
}

static void test_cse(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = fmul(b,x,x),
            z = fmul(b,x,x);
        expect(y == z);
    }
    free(compile(b));
}

static void test_more_cse(void) {
    struct Builder *b = builder();
    {
        int x = splat(b,2.0f),
            y = splat(b,2.0f);
        expect(x == y);
    }
    free(compile(b));
}

static void test_no_cse(void) {
    struct Builder *b = builder();
    {
        int x = load(b,0),
            y = fmul(b,x,x);
        mutate(b, &x, y);
        int z = fmul(b,x,x);
        expect(y != z);
    }
    free(compile(b));
}

static void test_cse_sort(void) {
    struct Builder *b = builder();
     {
        int x = load (b,0),
            c = splat(b,2.0f),
            y = fmul (b,x,c),
            k = splat(b,2.0f),
            z = fmul (b,k,x);
        expect(c == k);
        expect(y == z);
    }
    free(compile(b));
}

static void test_cse_no_sort(void) {
    struct Builder *b = builder();
     {
        int x = load (b,0),
            c = splat(b,2.0f),
            y = fdiv (b,x,c),
            k = splat(b,2.0f),
            z = fdiv (b,k,x);
        expect(c == k);
        expect(y != z);
    }
    free(compile(b));
}

int main(void) {
    internal_tests();

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

    test_dce();
    test_uni();
    test_cse();
    test_more_cse();
    test_no_cse();
    test_cse_sort();
    test_cse_no_sort();
    return 0;
}
