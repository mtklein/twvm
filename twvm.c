#include "expect.h"
#include "hash.h"
#include "twvm.h"
#include <assert.h>
#include <stdlib.h>

#define K 4
#define vector(T) T __attribute__((vector_size(sizeof(T) * K)))

typedef union {
    vector(float) f;
    vector(int)   i;
} V32;

typedef struct PInst {
    int (*fn)(struct PInst const *ip, V32 *v, int end, void *ptr[]);
    int   x,y,z;  // Relative to this instruction (almost always negative).
    union { int ptr; float imm; };
} PInst;

typedef enum { CONSTANT,UNIFORM,VARYING } Shape;

typedef struct BInst {
    int (*fn)(struct PInst const *ip, V32 *v, int end, void *ptr[]);
    int   x,y,z;  // Absolute into b->inst, with id=0 predefined as a phony value (N/A).
    union { int ptr; float imm; };

    Shape shape   :  2;
    _Bool live    :  1;
    int   ptr_gen : 29;
    int   id;
} BInst;

typedef struct Builder {
    BInst       *inst;
    int         *ptr_gen;
    int          insts,ptrs;
    struct hash *cse;
} Builder;

Builder* builder(int ptrs) {
    Builder *b = calloc(1, sizeof *b);
    // A phony instruction at id=0 lets us assume that every BInst's inputs (x,y,z) always exist.
    b->inst    = calloc(1, sizeof *b->inst);
    b->insts   = 1;
    b->ptr_gen = calloc((size_t)ptrs, sizeof *b->ptr_gen);
    b->ptrs    = ptrs;
    return b;
}

#if defined(__clang__)
    #define next __attribute__((musttail)) return ip[1].fn(ip+1,v+1,end,ptr)
#else
    #define next                           return ip[1].fn(ip+1,v+1,end,ptr)
#endif

#define stage(name) int name##_(PInst const *ip, V32 *v, int end, void *ptr[])

static stage(done) {
    (void)ip;
    (void)v;
    (void)end;
    (void)ptr;
    return 0;
}

static int constant_fold(Builder *b, BInst inst) {
    if (inst.shape == CONSTANT && (inst.x || inst.y || inst.z)) {
        V32 v[4] = {
            {{b->inst[inst.x].imm}},
            {{b->inst[inst.y].imm}},
            {{b->inst[inst.z].imm}},
        };
        PInst ip[] = {
            {.fn=inst.fn, .x=-3, .y=-2, .z=-1, .ptr=inst.ptr},
            {.fn=done_},
        };
        ip->fn(ip,v+3,0,NULL);
        return splat(b, v[3].f[0]);
    }
    return 0;
}

typedef struct {
    Builder const *b;
    BInst   const *want;
    int            id, unused;
} MatchCtx;

static _Bool cse_match(int val, void *vctx) {
    MatchCtx *ctx = vctx;
    if (0 == __builtin_memcmp(ctx->want, ctx->b->inst+val, sizeof *ctx->want)) {
        ctx->id = val;
        return 1;
    }
    return 0;
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

static int push_(Builder *b, BInst inst) {
    assert(inst.x < b->insts);
    assert(inst.y < b->insts);
    assert(inst.z < b->insts);

    if (inst.shape < b->inst[inst.x].shape) { inst.shape = b->inst[inst.x].shape; }
    if (inst.shape < b->inst[inst.y].shape) { inst.shape = b->inst[inst.y].shape; }
    if (inst.shape < b->inst[inst.z].shape) { inst.shape = b->inst[inst.z].shape; }

    for (int id = constant_fold(b,inst); id;) {
        return id;
    }

    unsigned const hash = fnv1a(&inst, sizeof inst);
    for (MatchCtx ctx = {b,.want=&inst}; hash_lookup(b->cse, hash, cse_match, &ctx); ) {
        return ctx.id;
    }

    if ((b->insts & (b->insts-1)) == 0) {
        b->inst = realloc(b->inst, 2 * (size_t)b->insts * sizeof *b->inst);
    }
    int const id = b->insts++;
    b->inst[id] = inst;

    if (!inst.live) {
        b->cse = hash_insert(b->cse, hash, id);
    }
    return id;
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

static stage(thread_id) {
#if K == 4
    vector(float) iota = {0,1,2,3};
#endif
    if (end & (K-1)) { v->f = (float)end - 1 + iota; }
    else             { v->f = (float)end - K + iota; }
    next;
}
int thread_id(Builder *b) { return push(b, .fn=thread_id_, .shape=VARYING); }

static stage(splat) {
    v->f = ( (vector(float)){0} + 1 ) * ip->imm;
    next;
}
int splat(Builder *b, float imm) { return push(b, .fn=splat_, .imm=imm); }

static stage(load_uniform) {
    float const *p = ptr[ip->ptr],
                ix = v[ip->x].f[0];
    v->f = ( (vector(float)){0} + 1 ) * p[(int)ix];
    next;
}
static stage(load_contiguous) {
    float const *p = ptr[ip->ptr];
    if (end & (K-1)) { __builtin_memcpy(v, p + end - 1,   sizeof(float)); }
    else             { __builtin_memcpy(v, p + end - K, K*sizeof(float)); }
    next;
}
static stage(load_gather) {
    float const   *p = ptr[ip->ptr];
    vector(float) ix = v[ip->x].f;
    for (int i = 0; i < K; i++) {
        v->f[i] = p[(int)ix[i]];
    }
    next;
}
int load(Builder *b, int ptr, int ix) {
    assert(ptr < b->ptrs);
    int const ptr_gen = b->ptr_gen[ptr];
    if (b->inst[ix].shape <= UNIFORM) {
        return push(b, .fn=load_uniform_, .ptr=ptr, .x=ix, .shape=UNIFORM, .ptr_gen=ptr_gen);
    }
    if (b->inst[ix].fn == thread_id_) {
        return push(b, .fn=load_contiguous_, .ptr=ptr, .shape=VARYING, .ptr_gen=ptr_gen);
    }
    return push(b, .fn=load_gather_, .ptr=ptr, .x=ix, .shape=VARYING, .ptr_gen=ptr_gen);
}

static stage(store) {
    float *p = ptr[ip->ptr];
    if (end & (K-1)) { __builtin_memcpy(p + end - 1, v+ip->x,   sizeof(float)); }
    else             { __builtin_memcpy(p + end - K, v+ip->x, K*sizeof(float)); }
    next;
}
void store(Builder *b, int ptr, int x) {
    assert(ptr < b->ptrs);
    push(b, .fn=store_, .ptr=ptr, .x=x, .shape=VARYING, .live=1);
    b->ptr_gen[ptr]++;
}

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

int fadd(Builder *b, int x, int y) {
    if (b->inst[x].fn==fmul_) { return push(b, .fn=fmad_, .x=b->inst[x].x, .y=b->inst[x].y, .z=y); }
    if (b->inst[y].fn==fmul_) { return push(b, .fn=fmad_, .x=b->inst[y].x, .y=b->inst[y].y, .z=x); }
    return sort(b, .fn=fadd_, .x=x, .y=y);
}

int fsub(Builder *b, int x, int y       ) { return push(b, .fn=fsub_, .x=x, .y=y      ); }
int fmul(Builder *b, int x, int y       ) { return sort(b, .fn=fmul_, .x=x, .y=y      ); }
int fdiv(Builder *b, int x, int y       ) { return push(b, .fn=fdiv_, .x=x, .y=y      ); }
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
    free(b->cse);
    b->cse = NULL;
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
    push(b, .fn=done_, .shape=VARYING, .live=1);

    // Dead code elimination: the inputs of live instructions are live, and anything else is dead.
    // Marking dead instructions with fn=NULL handles the phony id=0 instruction naturally.
    int live = 0;
    backward(inst, b->inst) {
        if (inst->live) {
            b->inst[inst->x].live =
            b->inst[inst->y].live =
            b->inst[inst->z].live = 1;
        } else {
            inst->fn = NULL;
        }
        live += (inst->fn != NULL);
    }

    Program *p = calloc(1, sizeof *p + (size_t)live * sizeof *b->inst);

    for (int varying = 0; varying < 2; varying++) {
        if (varying) {
            p->loop = p->insts;
        }
        forward(inst, b->inst) {
            if (inst->fn && (inst->shape == VARYING) == varying) {
                inst->id = p->insts++;
                p->inst[inst->id] = (PInst) {
                    .fn  = inst->fn,
                    .x   = b->inst[inst->x].id - inst->id,
                    .y   = b->inst[inst->y].id - inst->id,
                    .z   = b->inst[inst->z].id - inst->id,
                    .ptr = inst->ptr,
                };
            }
        }
    }
    assert(p->insts == live);

    free(b->inst);
    free(b->ptr_gen);
    free(b->cse);
    free(b);
    return p;
}

void execute(Program const *p, int n, void *ptr[]) {
    V32 *val = calloc((size_t)p->insts, sizeof *val);

    PInst const *ip = p->inst,  *loop = ip + p->loop;
    V32          *v = val    , *vloop =  v + p->loop;

    for (int i = 0; i < n/K*K; i += K) { ip->fn(ip,v,i+K,ptr); ip = loop; v = vloop; }
    for (int i = n/K*K; i < n; i += 1) { ip->fn(ip,v,i+1,ptr); ip = loop; v = vloop; }

    free(val);
}

static void test_constant_prop(void) {
    Builder *b = builder(0);
    int x = splat(b,2.0f),
        y = fmul (b,x,x);
    expect(b->inst[y].fn == splat_);
    free(compile(b));
}

static void test_dead_code_elimination(void) {
    Builder *b = builder(1);
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
    Builder *b = builder(1);
    {
        int x = load(b,0,thread_id(b)),
            y = load(b,0,splat(b,0.0f)),
            z = fadd(b,y,splat(b,1.0f)),
            w = fmul(b,x,z);
        store(b,0,w);
    }
    Program *p = compile(b);
    expect(p->insts == 8);
    expect(p->loop  == 4);
    expect(p->inst[0].fn == splat_ && p->inst[0].imm == 0.0f);
    expect(p->inst[1].fn == load_uniform_);
    expect(p->inst[2].fn == splat_ && p->inst[2].imm == 1.0f);
    expect(p->inst[3].fn == fadd_);
    expect(p->inst[4].fn == load_contiguous_);
    expect(p->inst[5].fn == fmul_);
    expect(p->inst[6].fn == store_);
    free(p);
}

static void test_cse(void) {
    Builder *b = builder(1);
    {
        int x = load(b,0,thread_id(b)),
            y = fmul(b,x,x),
            z = fmul(b,x,x);
        expect(y == z);
    }
    free(compile(b));
}
static void test_more_cse(void) {
    Builder *b = builder(0);
    {
        int x = splat(b,2.0f),
            y = splat(b,2.0f);
        expect(x == y);
    }
    free(compile(b));
}
static void test_no_cse(void) {
    Builder *b = builder(1);
    {
        int x = load(b,0,thread_id(b)),
            y = fmul(b,x,x);
        mutate(b, &x, y);
        int z = fmul(b,x,x);
        expect(y != z);
    }
    free(compile(b));
}

static void test_cse_sort(void) {
    Builder *b = builder(1);
     {
        int x = load (b,0,thread_id(b)),
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
    Builder *b = builder(1);
     {
        int x = load (b,0,thread_id(b)),
            c = splat(b,2.0f),
            y = fdiv (b,x,c),
            k = splat(b,2.0f),
            z = fdiv (b,k,x);
        expect(c == k);
        expect(y != z);
    }
    free(compile(b));
}

static void test_load_cse(void) {
    Builder *b = builder(2);
    {
        int x = load(b,0, thread_id(b)),
            y = load(b,0, thread_id(b)),
            z = load(b,1, thread_id(b)),
            u = load(b,0, splat(b,0.0f)),
            v = load(b,0, splat(b,0.0f));
        expect(x == y);  // CSE should work for loading varyings
        expect(u == v);  // and also of course for uniforms
        expect(x != z);  // x and z are different varyings
        expect(x != u);  // x and u are different shapes

        store(b,0, fadd(b,x,y));

        int X = load(b,0, thread_id(b)),
            Z = load(b,1, thread_id(b)),
            U = load(b,0, splat(b,0.0f));
        expect(x != X);  // a store to ptr 0 invalidates loads from ptr 0
        expect(u != U);  // a store to ptr 0 also invalidates uniform loads from ptr 0
        expect(z == Z);  // a store to ptr 0 does not invalidate loads from ptr 1
    }
    free(compile(b));
}


void internal_tests(void);
void internal_tests(void) {
    test_constant_prop();
    test_dead_code_elimination();
    test_loop_hoisting();

    test_cse();
    test_more_cse();
    test_no_cse();

    test_cse_sort();
    test_cse_no_sort();

    test_load_cse();
}
