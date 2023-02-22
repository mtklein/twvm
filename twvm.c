#include "twvm.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

#define len(v) (int)(sizeof(v) / sizeof((v)[0]))

// Snake, try to remember some of the basics of optimization:
//   [x]  1) vectorization
//   [ ]  2) strength reduction
//   [ ]  3) constant propagation
//   [ ]  4) common sub-expression elimination
//   [x]  5) expression canonicalization
//   [ ]  6) loop-invariant hoisting

#define vector __attribute__((vector_size(16)))

typedef union {
    int32_t  vector i;
    uint32_t vector u;
    float    vector f;
} Slot;

typedef struct BInst {
    void (*fn)(Slot[], int id, int x, int y, int z, int imm, void *ptr[]);
    enum {CONSTANT,UNIFORM,VARYING,SIDE_EFFECT} kind;
    int                                         x,y,z,imm,padding;
} BInst;

typedef struct PInst {
    void (*fn)(Slot[], int id, int x, int y, int z, int imm, void *ptr[]);
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
    // Promote inst.kind up to the strongest of its intrinsic kind (if any) and its inputs' kinds,
    // e.g. CONSTANT + UNIFORM = UNIFORM, UNIFORM * VARYING == VARYING, etc.
    if (inst.x && inst.kind < b->inst[inst.x-1].kind) { inst.kind = b->inst[inst.x-1].kind; }
    if (inst.y && inst.kind < b->inst[inst.y-1].kind) { inst.kind = b->inst[inst.y-1].kind; }
    if (inst.z && inst.kind < b->inst[inst.z-1].kind) { inst.kind = b->inst[inst.z-1].kind; }

    if (is_pow2_or_zero(b->insts)) {
        b->inst = realloc(b->inst, (size_t)(b->insts ? b->insts * 2 : 4) * sizeof *b->inst);
    }
    b->inst[b->insts++] = inst;
    return (Val){b->insts};  // N.B. IDs in BInst and Val are 1-indexed to let 0 signal N/A.
}
static Val sort_(Builder *b, BInst inst) {
    if (inst.y < inst.x) {
        int  y = inst.y;
        inst.y = inst.x;
        inst.x = y;
    }
    return push_(b,inst);
}
#define push(b,f,...) push_(b, (BInst){.fn=f, __VA_ARGS__})
#define sort(b,f,...) sort_(b, (BInst){.fn=f, __VA_ARGS__})

static void test_sort(void) {
    Builder b = {0};
    Val x = splat(&b, 47),
        y = splat(&b, 42);
    fadd(&b, y,x);  assert(b.inst[2].x == 1 && b.inst[2].y == 2);
    fsub(&b, y,x);  assert(b.inst[3].x == 2 && b.inst[3].y == 1);
    free(b.inst);
}

#define fn(name) \
    static void name##_(Slot v[], int id, int x, int y, int z, int imm, void *ptr[])

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-parameter"

fn(splat) {
    v[id].i = (Slot){0}.i + imm;
}
fn(gather32) {
    int32_t const *p = ptr[imm];
    int32_t vector ix = v[x].i,
                  val = {0};
    #pragma GCC unroll
    for (int i = 0; i < len(ix); i++) {
        val[i] = p[ix[i]];
    }
    v[id].i = val;
}
fn(scatter32) {
    int32_t *p = ptr[imm];
    int32_t vector ix = v[x].i,
                  val = v[y].i,
                 mask = v[z].i;
    #pragma GCC unroll
    for (int i = 0; i < len(ix); i++) {
        if (mask[i]) {
            p[ix[i]] = val[i];
        }
    }
}

fn(   fadd) { v[id].f =  v[x].f +  v[y].f; }
fn(   fsub) { v[id].f =  v[x].f -  v[y].f; }
fn(   fmul) { v[id].f =  v[x].f *  v[y].f; }
fn(   fdiv) { v[id].f =  v[x].f /  v[y].f; }
fn(   iadd) { v[id].i =  v[x].i +  v[y].i; }
fn(   isub) { v[id].i =  v[x].i -  v[y].i; }
fn(   imul) { v[id].i =  v[x].i *  v[y].i; }
fn(    shl) { v[id].i =  v[x].i << v[y].i; }
fn(    shr) { v[id].u =  v[x].u >> v[y].i; }
fn(    sra) { v[id].i =  v[x].i >> v[y].i; }
fn(bit_and) { v[id].i =  v[x].i &  v[y].i; }
fn(bit_or ) { v[id].i =  v[x].i |  v[y].i; }
fn(bit_xor) { v[id].i =  v[x].i ^  v[y].i; }
fn(bit_not) { v[id].i = ~v[x].i          ; }
fn(    ilt) { v[id].i =  v[x].i <  v[y].i; }
fn(    ieq) { v[id].i =  v[x].i == v[y].i; }
fn(    flt) { v[id].i =  v[x].f <  v[y].f; }
fn(    fle) { v[id].i =  v[x].f <= v[y].f; }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
fn(    feq) { v[id].i =  v[x].f == v[y].f; }
#pragma GCC diagnostic pop
fn(    sel) { v[id].i = (v[x].i & v[y].i) | (~v[x].i & v[z].i); }

#pragma GCC diagnostic pop  // Restore -Wunused

Val splat(Builder *b, int32_t imm) { return push(b, splat_, .imm=imm); }
Val load32(Builder *b, int ptr, Val ix) {
    // TODO: ix == uniform        ~~> broadcast load marked as kind=uniform
    // TODO: ix == uniform + iota ~~> contiguous load
    return push(b, gather32_, .kind=VARYING, .imm=ptr, .x=ix.id);
}
void store32(Builder *b, int ptr, Val ix, Val y, Val mask) {
    // TODO: assert no index conflict?
    // TODO: mask == true                         ~~> unconditional scatter
    // TODO: mask == true and ix = uniform + iota ~~> contiguous store
    push(b, scatter32_, .kind=SIDE_EFFECT, .imm=ptr, .x=ix.id, .y=y.id, .z=mask.id);
}

Val    fadd(Builder *b, Val x, Val y       ) { return sort(b,   fadd_, .x=x.id, .y=y.id); }
Val    fsub(Builder *b, Val x, Val y       ) { return push(b,   fsub_, .x=x.id, .y=y.id); }
Val    fmul(Builder *b, Val x, Val y       ) { return sort(b,   fmul_, .x=x.id, .y=y.id); }
Val    fdiv(Builder *b, Val x, Val y       ) { return push(b,   fdiv_, .x=x.id, .y=y.id); }
Val    iadd(Builder *b, Val x, Val y       ) { return sort(b,   iadd_, .x=x.id, .y=y.id); }
Val    isub(Builder *b, Val x, Val y       ) { return push(b,   isub_, .x=x.id, .y=y.id); }
Val    imul(Builder *b, Val x, Val y       ) { return sort(b,   imul_, .x=x.id, .y=y.id); }
Val     shl(Builder *b, Val x, Val y       ) { return push(b,    shl_, .x=x.id, .y=y.id); }
Val     shr(Builder *b, Val x, Val y       ) { return push(b,    shr_, .x=x.id, .y=y.id); }
Val     sra(Builder *b, Val x, Val y       ) { return push(b,    sra_, .x=x.id, .y=y.id); }
Val bit_and(Builder *b, Val x, Val y       ) { return sort(b,bit_and_, .x=x.id, .y=y.id); }
Val bit_or (Builder *b, Val x, Val y       ) { return sort(b,bit_or_ , .x=x.id, .y=y.id); }
Val bit_xor(Builder *b, Val x, Val y       ) { return sort(b,bit_xor_, .x=x.id, .y=y.id); }
Val bit_not(Builder *b, Val x              ) { return push(b,bit_not_, .x=x.id         ); }
Val     ilt(Builder *b, Val x, Val y       ) { return push(b,    ilt_, .x=x.id, .y=y.id); }
Val     ieq(Builder *b, Val x, Val y       ) { return sort(b,    ieq_, .x=x.id, .y=y.id); }
Val     flt(Builder *b, Val x, Val y       ) { return push(b,    flt_, .x=x.id, .y=y.id); }
Val     fle(Builder *b, Val x, Val y       ) { return push(b,    fle_, .x=x.id, .y=y.id); }
Val     feq(Builder *b, Val x, Val y       ) { return sort(b,    feq_, .x=x.id, .y=y.id); }
Val     sel(Builder *b, Val x, Val y, Val z) { return push(b,    sel_, .x=x.id, .y=y.id, .z=z.id); }

typedef struct Program {
    int   vals,
          unused;
    PInst inst[];
} Program;

void assert_unit_tests(void);
void assert_unit_tests(void) {
    test_is_pow2_or_zero();
    test_sort();
}
