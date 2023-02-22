#include "twvm.h"
#include <assert.h>
#include <stdlib.h>

#define len(arr) (int)( sizeof(arr) / sizeof((arr)[0]) )
#define assert_eq(x,y) assert(x<=y && y <= x)

static void test_nothing(void) {
    struct Builder *b = builder();
    struct Program *p = compile(b);
    execute(p, 0, NULL, NULL);
    free(p);
}

static void test_triple(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0),
            y = fmul(b, x, splat(b, 3.0f));
        store(b,1,y);
    }
    struct Program *p = compile(b);

    float s[] = {1.0f,2.0f,3.0f,4.0f,5.0f,6.0f},
          d[len(s)];
    execute(p, len(s), NULL, (float*[]){s,d});

    assert_eq(d[0],  3.0f);
    assert_eq(d[1],  6.0f);
    assert_eq(d[2],  9.0f);
    assert_eq(d[3], 12.0f);
    assert_eq(d[4], 15.0f);
    assert_eq(d[5], 18.0f);
    free(p);
}

static void test_mutate(void) {
    struct Builder *b = builder();
    {
        int x = load(b, 0);
        mutate(b, &x, fmul(b, x, splat(b, 3.0f)));
        store(b,0,x);
    }
    struct Program *p = compile(b);

    float f[] = {1.0f,2.0f,3.0f,4.0f,5.0f,6.0f};
    execute(p, len(f), NULL, (float*[]){f});

    assert_eq(f[0],  3.0f);
    assert_eq(f[1],  6.0f);
    assert_eq(f[2],  9.0f);
    assert_eq(f[3], 12.0f);
    assert_eq(f[4], 15.0f);
    assert_eq(f[5], 18.0f);
    free(p);
}

int main(void) {
    test_nothing();
    test_triple();
    test_mutate();
    return 0;
}
