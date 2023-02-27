#include "twvm.h"
#include <stdlib.h>

#define K 4
#define vector(T) T __attribute__((vector_size(sizeof(T) * K)))
typedef vector(float) F32;

typedef struct Inst {
    int (*fn)(struct Inst const *ip, F32 *v, int end, float const *uni, float *var[]);
    int   x,y,ix;
    float imm;
} Inst;

typedef struct Builder {
    Inst *inst;
    int   insts,unused;
} Builder;

Builder* builder(void) {
    Builder *b = calloc(1, sizeof *b);
    return b;
}

static int push_(Builder *b, Inst inst) {
    if ((b->insts & (b->insts-1)) == 0) {
        b->inst = realloc(b->inst, (size_t)(b->insts ? b->insts * 2 : 1) * sizeof *b->inst);
    }
    b->inst[b->insts++] = inst;
    return b->insts;  // Builder / Val IDs are 1-indexed so 0 can indicate N/A.
}
#define push(b,...) push_(b, (Inst){__VA_ARGS__})

#define next __attribute__((musttail)) return ip[1].fn(ip+1,v+1, end,uni,var)
#define stage(name) \
    static int name##_(Inst const *ip, F32 *v, int end, float const *uni, float *var[])

stage(done) {
    (void)ip;
    (void)v;
    (void)end;
    (void)uni;
    (void)var;
    return 0;
}

stage(splat) {
    *v = ( (F32){0} + 1.0f ) * ip->imm;
    next;
}
int splat(Builder *b, float imm) { return push(b, .fn=splat_, .imm=imm); }

stage(uniform) {
    *v = ( (F32){0} + 1.0f ) * uni[ip->ix];
    next;
}
int uniform(Builder *b, int ix) { return push(b, .fn=uniform_, .ix=ix); }


stage(load) {
    float const *ptr = var[ip->ix];
    if (end & (K-1)) { __builtin_memcpy(v, ptr + end - 1,   sizeof(float)); }
    else             { __builtin_memcpy(v, ptr + end - K, K*sizeof(float)); }
    next;
}
int load(Builder *b, int ix) { return push(b, .fn=load_, .ix=ix); }

stage(store) {
    float *ptr = var[ip->ix];
    if (end & (K-1)) { __builtin_memcpy(ptr + end - 1, v+ip->y,   sizeof(float)); }
    else             { __builtin_memcpy(ptr + end - K, v+ip->y, K*sizeof(float)); }
    next;
}
void store(Builder *b, int ix, int val) { push(b, .fn=store_, .ix=ix, .y=val); }

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wfloat-equal"

stage(fadd) { *v =        v[ip->x] +  v[ip->y]  ; next; }
stage(fsub) { *v =        v[ip->x] -  v[ip->y]  ; next; }
stage(fmul) { *v =        v[ip->x] *  v[ip->y]  ; next; }
stage(fdiv) { *v =        v[ip->x] /  v[ip->y]  ; next; }
stage(feq ) { *v = (F32)( v[ip->x] == v[ip->y] ); next; }
stage(flt ) { *v = (F32)( v[ip->x] <  v[ip->y] ); next; }

#pragma GCC diagnostic pop

int fadd(Builder *b, int x, int y) { return push(b, .fn=fadd_, .x=x, .y=y); }
int fsub(Builder *b, int x, int y) { return push(b, .fn=fsub_, .x=x, .y=y); }
int fmul(Builder *b, int x, int y) { return push(b, .fn=fmul_, .x=x, .y=y); }
int fdiv(Builder *b, int x, int y) { return push(b, .fn=fdiv_, .x=x, .y=y); }
int feq (Builder *b, int x, int y) { return push(b, .fn=feq_ , .x=x, .y=y); }
int flt (Builder *b, int x, int y) { return push(b, .fn=flt_ , .x=x, .y=y); }

stage(mutate) { v[ip->x] = v[ip->y]; next; }
void mutate(Builder *b, int *x, int y) { push(b, .fn=mutate_, .x=*x, .y=y); }

stage(jump) {
    vector(int) const cond = (vector(int))v[ip->y];
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
void jump(Builder *b, int dst, int cond) { push(b, .fn=jump_, .x=dst, .y=cond); }

typedef struct Program {
    int  insts,unused;
    Inst inst[];
} Program;

Program* compile(Builder *b) {
    push(b, .fn=done_);

    Program *p = calloc(1, sizeof *p + (size_t)b->insts * sizeof *b->inst);
    for (int i = 0; i < b->insts; i++) {
        Inst inst = b->inst[i];
        p->inst[p->insts++] = (Inst) {
            .fn  = inst.fn,
            .x   = inst.x-1 - i,  // -1 for 1-indexed -> 0-indexed, -i for 0-indexed -> relative.
            .y   = inst.y-1 - i,
            .imm = inst.imm,
            .ix  = inst.ix,
        };
    }
    free(b->inst);
    free(b);
    return p;
}

void execute(Program const *p, int n, float const *uniform, float *varying[]) {
    F32 *v = calloc((size_t)p->insts, sizeof *v);
    for (int i = 0; i < n/K*K; i += K) { p->inst->fn(p->inst,v,i+K,uniform,varying); }
    for (int i = n/K*K; i < n; i += 1) { p->inst->fn(p->inst,v,i+1,uniform,varying); }
    free(v);
}
