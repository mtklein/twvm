#include "expect.h"
#include "twvm.h"
#include <stdlib.h>

#define K 4
#define vector(T) T __attribute__((vector_size(sizeof(T) * K)))

typedef union {
    vector(float) f;
    vector(int)   i;
} V32;

typedef struct PInst {
    int (*fn)(struct PInst const *ip, V32 *v, int end, float const *uni, float *var[]);
    int   x,y,z;  // Relative to this instruction (almost always negative).
    union { int ix; float imm; };
} PInst;

typedef enum { MATH,CONST,UNIFORM,VARYING } Kind;

typedef struct BInst {
    int (*fn)(struct PInst const *ip, V32 *v, int end, float const *uni, float *var[]);
    int   x,y,z;  // Absolute, but 1-indexed so that 0 can mean N/A.
    union { int ix; float imm; };
    union { Kind kind; int id; };
    _Bool live, loop_dependent, unused[2];
} BInst;

typedef struct Entry {
    unsigned hash;
    int      id;
} Entry;

typedef struct Builder {
    BInst *inst;
    int    insts,unused;
    Entry *cse;
    int    cse_len,cse_cap;
} Builder;

Builder* builder(void) {
    Builder *b = calloc(1, sizeof *b);
    return b;
}
static void drop(Builder *b) {
    free(b->inst);
    free(b->cse);
    free(b);
}

#if defined(__clang__)
    #define next __attribute__((musttail)) return ip[1].fn(ip+1,v+1, end,uni,var)
#else
    #define next                           return ip[1].fn(ip+1,v+1, end,uni,var)
#endif

#define stage(name) int name##_(PInst const *ip, V32 *v, int end, float const *uni, float *var[])

static stage(done) {
    (void)ip;
    (void)v;
    (void)end;
    (void)uni;
    (void)var;
    return 0;
}

// Constant folding: math on constants produces a constant.
static int constant_fold(Builder *b, BInst inst) {
    while (inst.kind == MATH) {
        if (inst.x && b->inst[inst.x-1].kind != CONST) { break; }
        if (inst.y && b->inst[inst.y-1].kind != CONST) { break; }
        if (inst.z && b->inst[inst.z-1].kind != CONST) { break; }

        V32 v[4] = {0};
        if (inst.x) { v[0].f[0] = b->inst[inst.x-1].imm; }
        if (inst.y) { v[1].f[0] = b->inst[inst.y-1].imm; }
        if (inst.z) { v[2].f[0] = b->inst[inst.z-1].imm; }

        PInst ip[] = {
            {.fn=inst.fn, .x=-3, .y=-2, .z=-1, .ix=inst.ix},
            {.fn=done_},
        };
        ip->fn(ip,v+3,0,NULL,NULL);
        return splat(b, v[3].f[0]);
    }
    return 0;
}

// Common sub-expression elimination: have we seen this same instruction before?
// Note: in cse_* functions, we use id=0 to indicate an empty Entry,
//       and always keep at least one empty Entry (except of course when b->cse=NULL).
static int cse_lookup(Builder const *b, BInst inst, unsigned hash) {
    if (b->cse)
    for (unsigned mask=(unsigned)(b->cse_cap-1), i=hash&mask; b->cse[i].id; i = (i+1)&mask) {
        int const id = b->cse[i].id;
        if (b->cse[i].hash == hash && 0 == __builtin_memcmp(&inst, b->inst + id-1, sizeof inst)) {
            return id;
        }
    }
    return 0;
}
static void cse_just_insert(Entry *cse, int cap, unsigned hash, int id) {
    unsigned i, mask = (unsigned)(cap-1);
    for (i = hash&mask; cse[i].id; i = (i+1)&mask);
    cse[i] = (Entry){hash,id};
}
static void cse_insert(Builder *b, unsigned hash, int id) {
    if (b->cse_len/3 >= b->cse_cap/4) {
        int    cap = b->cse_cap ? 2*b->cse_cap : 2;
        Entry *cse = calloc((size_t)cap, sizeof *cse);
        for (int i = 0; i < b->cse_cap; i++) {
            if (b->cse[i].id) {
                cse_just_insert(cse,cap, b->cse[i].hash, b->cse[i].id);
            }
        }
        free(b->cse);
        b->cse     = cse;
        b->cse_cap = cap;
    }
    cse_just_insert(b->cse, b->cse_cap, hash, id);
    b->cse_len++;
}

// Just an arbitrary, easy-to-implement hash function.  Results won't be sensitive to this choice.
static unsigned fnv1a(void const *v, size_t len) {
    unsigned hash = 0x811c9dc5;
    for (unsigned char const *b=v, *end=b+len; b != end; b++) {
        hash ^= *b;
        __builtin_mul_overflow(hash, 0x01000193, &hash);
    }
    return hash;
}

static int push_(Builder *b, BInst const inst) {
    // The order we try common sub-expression elimination and constant folding doesn't much matter.
    unsigned const hash = fnv1a(&inst, sizeof inst);
    for (int id = cse_lookup   (b,inst,hash); id;) { return id; }
    for (int id = constant_fold(b,inst     ); id;) { return id; }

    if ((b->insts & (b->insts-1)) == 0) {
        b->inst = realloc(b->inst, (size_t)(b->insts ? b->insts * 2 : 1) * sizeof *b->inst);
    }
    b->inst[b->insts++] = inst;  // Builder IDs (here b->insts) are 1-indexed.
    if (inst.kind < VARYING && !inst.live) { cse_insert(b,hash,b->insts); }
    return b->insts;
}
#define push(b,...) push_(b, (BInst){__VA_ARGS__})

// When op(x,y)==op(y,x), using sort() instead of push() will canonicalize, allowing more CSE.
static int sort_(Builder *b, BInst inst) {
    if (inst.x > inst.y) {
        int tmp = inst.y;
        inst.y  = inst.x;
        inst.x  = tmp;
    }
    return push_(b,inst);
}
#define sort(b,...) sort_(b, (BInst){__VA_ARGS__})

static stage(splat) {
    v->f = ( (vector(float)){0} + 1 ) * ip->imm;
    next;
}
int splat(Builder *b, float imm) { return push(b, .fn=splat_, .imm=imm, .kind=CONST); }

static stage(uniform) {
    v->f = ( (vector(float)){0} + 1 ) * uni[ip->ix];
    next;
}
int uniform(Builder *b, int ix) { return push(b, .fn=uniform_, .ix=ix, .kind=UNIFORM); }

static stage(load) {
    float const *ptr = var[ip->ix];
    if (end & (K-1)) { __builtin_memcpy(v, ptr + end - 1,   sizeof(float)); }
    else             { __builtin_memcpy(v, ptr + end - K, K*sizeof(float)); }
    next;
}
int load(Builder *b, int ix) { return push(b, .fn=load_, .ix=ix, .kind=VARYING); }

static stage(store) {
    float *ptr = var[ip->ix];
    if (end & (K-1)) { __builtin_memcpy(ptr + end - 1, v+ip->x,   sizeof(float)); }
    else             { __builtin_memcpy(ptr + end - K, v+ip->x, K*sizeof(float)); }
    next;
}
void store(Builder *b, int ix, int x) { push(b, .fn=store_, .ix=ix, .x=x, .kind=VARYING, .live=1); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

static stage(fadd) { v->f = v[ip->x].f +  v[ip->y].f             ; next; }
static stage(fsub) { v->f = v[ip->x].f -  v[ip->y].f             ; next; }
static stage(fmul) { v->f = v[ip->x].f *  v[ip->y].f             ; next; }
static stage(fdiv) { v->f = v[ip->x].f /  v[ip->y].f             ; next; }
static stage(fmad) { v->f = v[ip->x].f *  v[ip->y].f + v[ip->z].f; next; }
static stage(feq ) { v->i = v[ip->x].f == v[ip->y].f             ; next; }
static stage(flt ) { v->i = v[ip->x].f <  v[ip->y].f             ; next; }
static stage(fle ) { v->i = v[ip->x].f <= v[ip->y].f             ; next; }
static stage(band) { v->i = v[ip->x].i &  v[ip->y].i             ; next; }
static stage(bor ) { v->i = v[ip->x].i |  v[ip->y].i             ; next; }
static stage(bxor) { v->i = v[ip->x].i ^  v[ip->y].i             ; next; }
static stage(bsel) {
    v->i = ( v[ip->x].i & v[ip->y].i)
         | (~v[ip->x].i & v[ip->z].i);
    next;
}

#pragma GCC diagnostic pop

int fadd(Builder *b, int x, int y       ) { return sort(b, .fn=fadd_, .x=x, .y=y      ); }
int fsub(Builder *b, int x, int y       ) { return push(b, .fn=fsub_, .x=x, .y=y      ); }
int fmul(Builder *b, int x, int y       ) { return sort(b, .fn=fmul_, .x=x, .y=y      ); }
int fdiv(Builder *b, int x, int y       ) { return push(b, .fn=fdiv_, .x=x, .y=y      ); }
int fmad(Builder *b, int x, int y, int z) { return sort(b, .fn=fmad_, .x=x, .y=y, .z=z); }
int feq (Builder *b, int x, int y       ) { return sort(b, .fn=feq_ , .x=x, .y=y      ); }
int flt (Builder *b, int x, int y       ) { return push(b, .fn=flt_ , .x=x, .y=y      ); }
int fle (Builder *b, int x, int y       ) { return push(b, .fn=fle_ , .x=x, .y=y      ); }
int band(Builder *b, int x, int y       ) { return sort(b, .fn=band_, .x=x, .y=y      ); }
int bor (Builder *b, int x, int y       ) { return sort(b, .fn=bor_ , .x=x, .y=y      ); }
int bxor(Builder *b, int x, int y       ) { return sort(b, .fn=bxor_, .x=x, .y=y      ); }
int bsel(Builder *b, int x, int y, int z) { return push(b, .fn=bsel_, .x=x, .y=y, .z=z); }

int fgt(Builder *b, int x, int y) { return flt(b,y,x); }
int fge(Builder *b, int x, int y) { return fle(b,y,x); }

static stage(mutate) {
    v[ip->x] = v[ip->y];
    next;
}
void mutate(Builder *b, int *var, int val) {
    push(b, .fn=mutate_, .x=*var, .y=val, .live=1);

    // Forget all CSE entries when anything mutates.  (TODO: kind of a big hammer)
    __builtin_bzero(b->cse, (size_t)b->cse_cap * sizeof *b->cse);
    b->cse_len = 0;
}

static stage(jump) {
    vector(int) const cond = v[ip->y].i;
    int any = 0;
    for (int i = 0; i < K; i++) {
        any |= cond[i];
    }
    if (any) {
        int const jmp = ip->x - 1;
        ip += jmp;
        v  += jmp;
    }
    next;
}
void jump(Builder *b, int dst, int cond) { push(b, .fn=jump_, .x=dst, .y=cond, .live=1); }

typedef struct Program {
    int   insts,loop;
    PInst inst[];
} Program;

#define  forward(elt,arr) for (__typeof__(arr) elt = arr; elt < arr + arr##s; elt++)
#define backward(elt,arr) for (__typeof__(arr) elt = arr + arr##s; elt --> arr;)

Program* compile(Builder *b) {
    push(b, .fn=done_, .kind=VARYING, .live=1);

    // Dead code elimination: mark inputs to live instructions as live.
    int live = 0;
    backward(inst, b->inst) {
        if (inst->live) {
            live++;
            if (inst->x) { b->inst[inst->x-1].live = 1; }
            if (inst->y) { b->inst[inst->y-1].live = 1; }
            if (inst->z) { b->inst[inst->z-1].live = 1; }
        }
    }

    Program *p = calloc(1, sizeof *p + (size_t)live * sizeof *b->inst);

    // Loop-invariant hoisting: constant and uniform instructions can run once,
    // but anything affected by a varying is loop-dependent.
    forward(inst, b->inst) {
        if (inst->kind == VARYING
                || (inst->x && b->inst[inst->x-1].loop_dependent)
                || (inst->y && b->inst[inst->y-1].loop_dependent)
                || (inst->z && b->inst[inst->z-1].loop_dependent)) {
            inst->loop_dependent = 1;
        }
    }

    for (int loop = 0; loop < 2; loop++) {
        if (loop) {
            p->loop = p->insts;
        }
        forward(inst, b->inst) {
            if (inst->live && inst->loop_dependent == loop) {
                p->inst[inst->id = p->insts++] = (PInst) {
                    .fn = inst->fn,
                    .x  = inst->x ? b->inst[inst->x-1].id - inst->id : 0,
                    .y  = inst->y ? b->inst[inst->y-1].id - inst->id : 0,
                    .z  = inst->z ? b->inst[inst->z-1].id - inst->id : 0,
                    .ix = inst->ix,
                };
            }
        }
    }
    drop(b);
    return p;
}

void execute(Program const *p, int n, float const *uniform, float *varying[]) {
    V32 *val = calloc((size_t)p->insts, sizeof *val);

    // This is loop-invariant hoisting: run first from the top, then any subsequent from p->loop.
    PInst const *ip = p->inst,  *loop = ip + p->loop;
    V32          *v = val    , *vloop =  v + p->loop;

    for (int i = 0; i < n/K*K; i += K) { ip->fn(ip,v,i+K,uniform,varying); ip = loop; v = vloop; }
    for (int i = n/K*K; i < n; i += 1) { ip->fn(ip,v,i+1,uniform,varying); ip = loop; v = vloop; }

    free(val);
}

static void test_constant_prop(void) {
    Builder *b = builder();
    int x = splat(b,2.0f),
        y = fmul (b,x,x);
    expect(b->inst[y-1].fn == splat_);
    drop(b);
}

static void test_dead_code_elimination(void) {
    Builder *b = builder();
    {
        int live = splat(b,2.0f),
            dead = splat(b,4.0f);
        (void)dead;
        store(b,0,live);
    }
    Program *p = compile(b);
    expect(p->insts == 3);
    free(p);
}

static void test_loop_hoisting(void) {
    Builder *b = builder();
    {
        int x = load(b,0),
            y = uniform(b,0),
            z = fadd(b,y,splat(b,1.0f)),
            w = fmul(b,x,z);
        store(b,0,w);
    }
    Program *p = compile(b);
    expect(p->insts == 7);
    expect(p->loop  == 3);
    expect(p->inst[0].fn == uniform_);
    expect(p->inst[1].fn == splat_);
    expect(p->inst[2].fn == fadd_);
    expect(p->inst[3].fn == load_);
    expect(p->inst[4].fn == fmul_);
    expect(p->inst[5].fn == store_);
    free(p);
}

void internal_tests(void);
void internal_tests(void) {
    test_constant_prop();
    test_dead_code_elimination();
    test_loop_hoisting();
}
