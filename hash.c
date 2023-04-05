#include "hash.h"
#include <assert.h>
#include <stdlib.h>

struct hash {
    unsigned                           len,mask;
    struct { unsigned hash; int val; } entry[];
};

static void just_insert(struct hash *h, unsigned hash, int val) {
    assert(hash && h->len+1 <= h->mask);  // At least two empty slots remain.
    unsigned i;
    for (i = hash & h->mask; h->entry[i].hash; i = (i+1) & h->mask) /*look for empty slot*/;
    h->entry[i].hash = hash;
    h->entry[i].val  = val;
    h->len++;
    assert(h->len <= h->mask);            // At least one empty slot remains.
}

struct hash* hash_insert(struct hash *h, unsigned hash, int val) {
    unsigned const len = h ? h->len    : 0,
                   cap = h ? h->mask+1 : 0;
    if (hash == 0) {
        hash = 1;
    }
    if (len/3 >= cap/4) {
        unsigned const new_cap = cap ? 2*cap : 2;
        struct hash *grown = calloc(1, sizeof *grown + new_cap * sizeof *grown->entry);
        grown->mask = new_cap-1;
        for (unsigned i = 0; i < cap; i++) {
            if (h->entry[i].hash) {
                just_insert(grown, h->entry[i].hash, h->entry[i].val);
            }
        }
        free(h);
        h = grown;
    }
    just_insert(h, hash, val);
    return h;
}

_Bool hash_lookup(struct hash const *h, unsigned hash, _Bool(*match)(int, void*), void *ctx) {
    if (hash == 0) {
        hash = 1;
    }
    if (h) {
        for (unsigned i = hash & h->mask; h->entry[i].hash; i = (i+1) & h->mask) {
            if (h->entry[i].hash == hash && match(h->entry[i].val, ctx)) {
                return 1;
            }
        }
    }
    return 0;
}
