#include "twvm.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define len(v) (int)(sizeof(v) / sizeof((v)[0]))

// Snake, try to remember some of the basics of optimization:
//   [x]  1) vectorization
//   [ ]  2) strength reduction
//   [ ]  3) constant propagation
//   [ ]  4) common sub-expression elimination (and expression canonicalization)
//   [ ]  5) loop-invariant hoisting

#define vector __attribute__((vector_size(16)))

typedef union {
    int32_t  vector i;
    uint32_t vector u;
    float    vector f;
} Slot;

struct PInst;

typedef struct BInst {
    void (*fn)(Slot[], int id, struct PInst const*, void *ptr[]);
    int    x,y,z,imm;
} BInst;

typedef struct PInst {
    void (*fn)(Slot[], int id, struct PInst const*, void *ptr[]);
    int    x,y,z,imm;
} PInst;

typedef struct Builder {
    BInst *inst;
    int    insts,
           unused;
} Builder;

Builder* builder(void) {
    Builder *b = calloc(1, sizeof *b);
    return b;
}

static _Bool is_pow2_or_zero(int x) {
    return (x & (x-1)) == 0;
}
static void test_is_pow2_or_zero(void) {
    assert( is_pow2_or_zero(0));
    assert( is_pow2_or_zero(1));
    assert( is_pow2_or_zero(2));
    assert(!is_pow2_or_zero(3));
    assert( is_pow2_or_zero(4));
    assert(!is_pow2_or_zero(5));
    assert(!is_pow2_or_zero(6));
}

static Val push_(Builder *b, BInst inst) {
    if (is_pow2_or_zero(b->insts)) {
        b->inst = realloc(b->inst, (size_t)(b->insts ? b->insts * 2 : 4) * sizeof *b->inst);
    }
    b->inst[b->insts++] = inst;
    return (Val){b->insts};  // N.B. 1-indexed
}
#define push(b,f,...) push_(b, (BInst){.fn=f, __VA_ARGS__})

#define fn(name) \
    static void name##_(Slot v[], int id, PInst const *inst, void *ptr[])

fn(splat) {
    (void)ptr;
    v[id].i = (Slot){0}.i + inst->imm;
}
Val splat(Builder *b, int32_t imm) { return push(b, splat_, .imm=imm); }

fn(gather32) {
    int32_t const *p = ptr[inst->imm];
    int32_t vector ix = v[inst->x].i,
                    r = {0};
    #pragma GCC unroll
    for (int i = 0; i < len(ix); i++) {
        r[i] = p[ix[i]];
    }
    v[id].i = r;
}
Val load32(Builder *b, int ptr, Val ix) {
    // TODO: ix == uniform        ~~> broadcast load marked as kind=uniform
    // TODO: ix == uniform + iota ~~> contiguous load
    return push(b, gather32_, .imm=ptr, .x=ix.id);
}

fn(scatter32) {
    (void)id;
    int32_t *p = ptr[inst->imm];
    int32_t vector ix = v[inst->x].i,
                    y = v[inst->y].i,
                 mask = v[inst->z].i;
    #pragma GCC unroll
    for (int i = 0; i < len(ix); i++) {
        if (mask[i]) {
            p[ix[i]] = y[i];
        }
    }
}
void store32(Builder *b, int ptr, Val ix, Val y, Val mask) {
    // TODO: assert no index conflict?
    // TODO: mask == true                         ~~> unconditional scatter
    // TODO: mask == true and ix = uniform + iota ~~> contiguous store
    push(b, scatter32_, .imm=ptr, .x=ix.id, .y=y.id, .z=mask.id);
}


fn(   fadd) { (void)ptr; v[id].f =  v[inst->x].f +  v[inst->y].f; }
fn(   fsub) { (void)ptr; v[id].f =  v[inst->x].f -  v[inst->y].f; }
fn(   fmul) { (void)ptr; v[id].f =  v[inst->x].f *  v[inst->y].f; }
fn(   fdiv) { (void)ptr; v[id].f =  v[inst->x].f /  v[inst->y].f; }
fn(   iadd) { (void)ptr; v[id].i =  v[inst->x].i +  v[inst->y].i; }
fn(   isub) { (void)ptr; v[id].i =  v[inst->x].i -  v[inst->y].i; }
fn(   imul) { (void)ptr; v[id].i =  v[inst->x].i *  v[inst->y].i; }
fn(    shl) { (void)ptr; v[id].i =  v[inst->x].i << v[inst->y].i; }
fn(    shr) { (void)ptr; v[id].u =  v[inst->x].u >> v[inst->y].i; }
fn(    sra) { (void)ptr; v[id].i =  v[inst->x].i >> v[inst->y].i; }
fn(bit_and) { (void)ptr; v[id].i =  v[inst->x].i &  v[inst->y].i; }
fn(bit_or ) { (void)ptr; v[id].i =  v[inst->x].i |  v[inst->y].i; }
fn(bit_xor) { (void)ptr; v[id].i =  v[inst->x].i ^  v[inst->y].i; }
fn(bit_not) { (void)ptr; v[id].i = ~v[inst->x].i                ; }
fn(    ilt) { (void)ptr; v[id].i =  v[inst->x].i <  v[inst->y].i; }
fn(    ieq) { (void)ptr; v[id].i =  v[inst->x].i == v[inst->y].i; }
fn(    flt) { (void)ptr; v[id].i =  v[inst->x].f <  v[inst->y].f; }
fn(    fle) { (void)ptr; v[id].i =  v[inst->x].f <= v[inst->y].f; }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
fn(    feq) { (void)ptr; v[id].i =  v[inst->x].f == v[inst->y].f; }
#pragma GCC diagnostic pop

Val    fadd(Builder *b, Val x, Val y) { return push(b,    fadd_, .x=x.id, .y=y.id); }
Val    fsub(Builder *b, Val x, Val y) { return push(b,    fsub_, .x=x.id, .y=y.id); }
Val    fmul(Builder *b, Val x, Val y) { return push(b,    fmul_, .x=x.id, .y=y.id); }
Val    fdiv(Builder *b, Val x, Val y) { return push(b,    fdiv_, .x=x.id, .y=y.id); }
Val    iadd(Builder *b, Val x, Val y) { return push(b,    iadd_, .x=x.id, .y=y.id); }
Val    isub(Builder *b, Val x, Val y) { return push(b,    isub_, .x=x.id, .y=y.id); }
Val    imul(Builder *b, Val x, Val y) { return push(b,    imul_, .x=x.id, .y=y.id); }
Val     shl(Builder *b, Val x, Val y) { return push(b,     shl_, .x=x.id, .y=y.id); }
Val     shr(Builder *b, Val x, Val y) { return push(b,     shr_, .x=x.id, .y=y.id); }
Val     sra(Builder *b, Val x, Val y) { return push(b,     sra_, .x=x.id, .y=y.id); }
Val bit_and(Builder *b, Val x, Val y) { return push(b, bit_and_, .x=x.id, .y=y.id); }
Val bit_or (Builder *b, Val x, Val y) { return push(b, bit_or_ , .x=x.id, .y=y.id); }
Val bit_xor(Builder *b, Val x, Val y) { return push(b, bit_xor_, .x=x.id, .y=y.id); }
Val bit_not(Builder *b, Val x       ) { return push(b, bit_not_, .x=x.id         ); }
Val     ilt(Builder *b, Val x, Val y) { return push(b,     ilt_, .x=x.id, .y=y.id); }
Val     ieq(Builder *b, Val x, Val y) { return push(b,     ieq_, .x=x.id, .y=y.id); }
Val     flt(Builder *b, Val x, Val y) { return push(b,     flt_, .x=x.id, .y=y.id); }
Val     fle(Builder *b, Val x, Val y) { return push(b,     fle_, .x=x.id, .y=y.id); }
Val     feq(Builder *b, Val x, Val y) { return push(b,     feq_, .x=x.id, .y=y.id); }

typedef struct Program {
    int   vals,
          unused;
    PInst inst[];
} Program;

void assert_unit_tests(void) {
    test_is_pow2_or_zero();
}
