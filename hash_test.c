#include "expect.h"
#include "hash.h"
#include <stdlib.h>

static _Bool match(int val, void *ctx) {
    int const *want = ctx;
    return val == *want;
}

static void test_basics(void) {
    struct hash *h = NULL;
    int want;

    want = 1;
    expect(!hash_lookup(h, 0x42, match, &want));
    h =     hash_insert(h, 0x42, want);
    expect( hash_lookup(h, 0x42, match, &want));
    want = 2;
    expect(!hash_lookup(h, 0x42, match, &want));

    free(h);
}

static void test_many(void) {
    int const K = 4096;
    struct hash *h = NULL;

    for (int i = 0; i < K; i++) {
        int want = 2*i;
        expect(!hash_lookup(h, (unsigned)i, match, &want));
        want++;
        expect(!hash_lookup(h, (unsigned)i, match, &want));

        h = hash_insert(h, (unsigned)i, 2*i);
    }

    for (int i = 0; i < K; i++) {
        int want = 2*i;
        expect( hash_lookup(h, (unsigned)i, match, &want));
        want++;
        expect(!hash_lookup(h, (unsigned)i, match, &want));
    }

    free(h);
}

int main(void) {
    test_basics();
    test_many();
    return 0;
}
