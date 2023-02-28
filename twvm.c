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
    int   x,y,z;
    union { int ix; float imm; };
} PInst;

typedef enum { MATH,CONST,UNIFORM,VARYING } Kind;

typedef struct BInst {
    int (*fn)(struct PInst const *ip, V32 *v, int end, float const *uni, float *var[]);
    int   x,y,z;
    union { int ix; float imm; };
    union { Kind kind; int id; };
    _Bool live, unused[3];
} BInst;


typedef struct Builder {
    BInst *inst;
    int    insts,unused;
} Builder;

Builder* builder(void) {
    Builder *b = calloc(1, sizeof *b);
    return b;
}
static void drop(Builder *b) {
    free(b->inst);
    free(b);
}

#define next __attribute__((musttail)) return ip[1].fn(ip+1,v+1, end,uni,var)
#define stage(name) \
    static int name##_(PInst const *ip, V32 *v, int end, float const *uni, float *var[])
stage(done) {
    (void)ip;
    (void)v;
    (void)end;
    (void)uni;
    (void)var;
    return 0;
}

static int push_(Builder *b, BInst inst) {
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

    if (inst.x && inst.kind < b->inst[inst.x-1].kind) { inst.kind = b->inst[inst.x-1].kind; }
    if (inst.y && inst.kind < b->inst[inst.y-1].kind) { inst.kind = b->inst[inst.y-1].kind; }
    if (inst.z && inst.kind < b->inst[inst.z-1].kind) { inst.kind = b->inst[inst.z-1].kind; }

    if ((b->insts & (b->insts-1)) == 0) {
        b->inst = realloc(b->inst, (size_t)(b->insts ? b->insts * 2 : 1) * sizeof *b->inst);
    }
    b->inst[b->insts++] = inst;
    return b->insts;  // Builder IDs are 1-indexed so 0 can indicate N/A.
}
#define push(b,...) push_(b, (BInst){__VA_ARGS__})

stage(splat) {
    v->f = ( (vector(float)){0} + 1 ) * ip->imm;
    next;
}
int splat(Builder *b, float imm) { return push(b, .fn=splat_, .imm=imm, .kind=CONST); }

stage(uniform) {
    v->f = ( (vector(float)){0} + 1 ) * uni[ip->ix];
    next;
}
int uniform(Builder *b, int ix) { return push(b, .fn=uniform_, .ix=ix, .kind=UNIFORM); }

stage(load) {
    float const *ptr = var[ip->ix];
    if (end & (K-1)) { __builtin_memcpy(v, ptr + end - 1,   sizeof(float)); }
    else             { __builtin_memcpy(v, ptr + end - K, K*sizeof(float)); }
    next;
}
int load(Builder *b, int ix) { return push(b, .fn=load_, .ix=ix, .kind=VARYING); }

stage(store) {
    float *ptr = var[ip->ix];
    if (end & (K-1)) { __builtin_memcpy(ptr + end - 1, v+ip->x,   sizeof(float)); }
    else             { __builtin_memcpy(ptr + end - K, v+ip->x, K*sizeof(float)); }
    next;
}
void store(Builder *b, int ix, int x) { push(b, .fn=store_, .ix=ix, .x=x, .kind=VARYING, .live=1); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

stage(fadd) { v->f = v[ip->x].f +  v[ip->y].f             ; next; }
stage(fsub) { v->f = v[ip->x].f -  v[ip->y].f             ; next; }
stage(fmul) { v->f = v[ip->x].f *  v[ip->y].f             ; next; }
stage(fdiv) { v->f = v[ip->x].f /  v[ip->y].f             ; next; }
stage(fmad) { v->f = v[ip->x].f *  v[ip->y].f + v[ip->z].f; next; }
stage(feq ) { v->i = v[ip->x].f == v[ip->y].f             ; next; }
stage(flt ) { v->i = v[ip->x].f <  v[ip->y].f             ; next; }
stage(fle ) { v->i = v[ip->x].f <= v[ip->y].f             ; next; }
stage(band) { v->i = v[ip->x].i &  v[ip->y].i             ; next; }
stage(bor ) { v->i = v[ip->x].i |  v[ip->y].i             ; next; }
stage(bxor) { v->i = v[ip->x].i ^  v[ip->y].i             ; next; }
stage(bsel) {
    v->i = ( v[ip->x].i & v[ip->y].i)
         | (~v[ip->x].i & v[ip->z].i);
    next;
}

#pragma GCC diagnostic pop

int fadd(Builder *b, int x, int y       ) { return push(b, .fn=fadd_, .x=x, .y=y      ); }
int fsub(Builder *b, int x, int y       ) { return push(b, .fn=fsub_, .x=x, .y=y      ); }
int fmul(Builder *b, int x, int y       ) { return push(b, .fn=fmul_, .x=x, .y=y      ); }
int fdiv(Builder *b, int x, int y       ) { return push(b, .fn=fdiv_, .x=x, .y=y      ); }
int fmad(Builder *b, int x, int y, int z) { return push(b, .fn=fmad_, .x=x, .y=y, .z=z); }
int feq (Builder *b, int x, int y       ) { return push(b, .fn=feq_ , .x=x, .y=y      ); }
int flt (Builder *b, int x, int y       ) { return push(b, .fn=flt_ , .x=x, .y=y      ); }
int fle (Builder *b, int x, int y       ) { return push(b, .fn=fle_ , .x=x, .y=y      ); }
int band(Builder *b, int x, int y       ) { return push(b, .fn=band_, .x=x, .y=y      ); }
int bor (Builder *b, int x, int y       ) { return push(b, .fn=bor_ , .x=x, .y=y      ); }
int bxor(Builder *b, int x, int y       ) { return push(b, .fn=bxor_, .x=x, .y=y      ); }
int bsel(Builder *b, int x, int y, int z) { return push(b, .fn=bsel_, .x=x, .y=y, .z=z); }

int fgt(Builder *b, int x, int y) { return flt(b,y,x); }
int fge(Builder *b, int x, int y) { return fle(b,y,x); }

stage(mutate) {
    v[ip->x] = v[ip->y];
    next;
}
void mutate(Builder *b, int *var, int val) { push(b, .fn=mutate_, .x=*var, .y=val, .live=1); }

stage(jump) {
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

Program* compile(Builder *b) {
    push(b, .fn=done_, .kind=VARYING, .live=1);

    int live = 0;
    for (BInst *inst = b->inst + b->insts; inst --> b->inst;) {
        if (inst->live) {
            live++;
            if (inst->x) { b->inst[inst->x-1].live = 1; }
            if (inst->y) { b->inst[inst->y-1].live = 1; }
            if (inst->z) { b->inst[inst->z-1].live = 1; }
        }
    }

    Program *p = calloc(1, sizeof *p + (size_t)live * sizeof *b->inst);

    for (int loop = 0; loop < 2; loop++) {
        if (loop) {
            p->loop = p->insts;
        }
        for (BInst *inst = b->inst; inst < b->inst + b->insts; inst++) {
            if (inst->live && (inst->kind == VARYING) == loop) {
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
    V32 *slots = calloc((size_t)p->insts, sizeof *slots);

    PInst const *ip = p->inst,  *loop = ip + p->loop;
    V32          *v = slots  , *vloop =  v + p->loop;

    for (int i = 0; i < n/K*K; i += K) { ip->fn(ip,v,i+K,uniform,varying); ip = loop; v = vloop; }
    for (int i = n/K*K; i < n; i += 1) { ip->fn(ip,v,i+1,uniform,varying); ip = loop; v = vloop; }

    free(slots);
}

int internal_tests(void);
int internal_tests(void) {
    int rc = 0;
    {  // rc=1 constant propagation
        Builder *b = builder();
        int x = splat(b,2.0f),
            y = fmul (b,x,x);
        if (b->inst[y-1].fn != splat_) {
            rc = 1;
        }
        drop(b);
    }
    {  // rc=2 dead-code elimination
        Builder *b = builder();
        {
            int live = splat(b,2.0f),
                dead = splat(b,4.0f);
            (void)dead;
            store(b,0,live);
        }
        Program *p = compile(b);
        if (p->insts != 3) {
            rc = 2;
        }
        free(p);
    }
    {  // rc=3 loop hoisting
        struct Builder *b = builder();
        {
            int x = load(b,0),
                y = uniform(b,0),
                z = fadd(b,y,splat(b,1.0f)),
                w = fmul(b,x,z);
            store(b,0,w);
        }
        Program *p = compile(b);
        if (0 || p->insts != 7
              || p->loop  != 3
              || p->inst[0].fn != uniform_
              || p->inst[1].fn != splat_
              || p->inst[2].fn != fadd_
              || p->inst[3].fn != load_
              || p->inst[4].fn != fmul_
              || p->inst[5].fn != store_) {
            rc = 3;
        }
        free(p);
    }
    return rc;
}
