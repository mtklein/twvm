#include "twvm.h"
#include <assert.h>
#include <stddef.h>
#include <stdlib.h>

// Snake, try to remember some of the basics of optimization:
//   [x]  1) vectorization
//   [ ]  2) strength reduction
//   [ ]  3) constant propagation
//   [ ]  4) common sub-expression elimination (and expression canonicalization)
//   [ ]  5) loop-invariant hoisting

typedef union {
    int32_t  __attribute__((vector_size(16))) i;
    uint32_t __attribute__((vector_size(16))) u;
    float    __attribute__((vector_size(16))) f;
} Slot;

struct PInst;

typedef struct BInst {
    void (*fn)(Slot[], int id, struct PInst const*, int32_t const *uni, void *var[]);
    int    x,y,z,imm;
} BInst;

typedef struct PInst {
    void (*fn)(Slot[], int id, struct PInst const*, int32_t const *uni, void *var[]);
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
    static void name##_(Slot v[], int id, PInst const *inst, int32_t const *uni, void *var[])

fn(  splat) { (void)uni; (void)var; v[id].i = (Slot){0}.i +     inst->imm ; }
fn(uniform) {            (void)var; v[id].i = (Slot){0}.i + uni[inst->imm]; }

Val   splat(Builder *b, int32_t imm) { return push(b,   splat_, .imm=imm); }
Val uniform(Builder *b, int     uni) { return push(b, uniform_, .imm=uni); }

// TODO: can we detect ix == uniform + iota and do a contiguous load?
// TODO: does it make sense to merge uniform ptrs and varying ptrs
//       and just detect a uniform ix as a signal to load a uniform?
fn(gather32) {
    (void)uni;
    int32_t const *ptr = var[inst->imm];
    Slot ix = v[inst->x];
    v[id] = (Slot){{
        ptr[ix.i[0]],
        ptr[ix.i[1]],
        ptr[ix.i[2]],
        ptr[ix.i[3]],
    }};
}
Val load32(Builder *b, int ptr, Val ix) { return push(b, gather32_, .imm=ptr, .x=ix.id); }


fn(   fadd) { (void)uni; (void)var; v[id].f =  v[inst->x].f +  v[inst->y].f; }
fn(   fsub) { (void)uni; (void)var; v[id].f =  v[inst->x].f -  v[inst->y].f; }
fn(   fmul) { (void)uni; (void)var; v[id].f =  v[inst->x].f *  v[inst->y].f; }
fn(   fdiv) { (void)uni; (void)var; v[id].f =  v[inst->x].f /  v[inst->y].f; }
fn(   iadd) { (void)uni; (void)var; v[id].i =  v[inst->x].i +  v[inst->y].i; }
fn(   isub) { (void)uni; (void)var; v[id].i =  v[inst->x].i -  v[inst->y].i; }
fn(   imul) { (void)uni; (void)var; v[id].i =  v[inst->x].i *  v[inst->y].i; }
fn(    shl) { (void)uni; (void)var; v[id].i =  v[inst->x].i << v[inst->y].i; }
fn(    shr) { (void)uni; (void)var; v[id].u =  v[inst->x].u >> v[inst->y].i; }
fn(    sra) { (void)uni; (void)var; v[id].i =  v[inst->x].i >> v[inst->y].i; }
fn(bit_and) { (void)uni; (void)var; v[id].i =  v[inst->x].i &  v[inst->y].i; }
fn(bit_or ) { (void)uni; (void)var; v[id].i =  v[inst->x].i |  v[inst->y].i; }
fn(bit_xor) { (void)uni; (void)var; v[id].i =  v[inst->x].i ^  v[inst->y].i; }
fn(bit_not) { (void)uni; (void)var; v[id].i = ~v[inst->x].i                ; }
fn(    ilt) { (void)uni; (void)var; v[id].i =  v[inst->x].i <  v[inst->y].i; }
fn(    ieq) { (void)uni; (void)var; v[id].i =  v[inst->x].i == v[inst->y].i; }
fn(    flt) { (void)uni; (void)var; v[id].i =  v[inst->x].f <  v[inst->y].f; }
fn(    fle) { (void)uni; (void)var; v[id].i =  v[inst->x].f <= v[inst->y].f; }
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"
fn(    feq) { (void)uni; (void)var; v[id].i =  v[inst->x].f == v[inst->y].f; }
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
