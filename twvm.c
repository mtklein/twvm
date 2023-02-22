#include "twvm.h"
#include <stdlib.h>

#define vector __attribute__((vector_size(16)))

typedef struct Inst {
    void (*fn)(struct Inst const *ip, float vector *v, int end, float const *uni, float *var[]);
    float imm;
    int   x,y,unused;
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
    return b->insts;
}
#define push(b,...) push_(b, (Inst){__VA_ARGS__})

#define next ip[1].fn(ip+1,v+1, end,uni,var); return
#define stage(name) \
    static void name##_(Inst const *ip, float vector *v, int end, float const *uni, float *var[])

stage(done) {
    (void)ip;
    (void)v;
    (void)end;
    (void)uni;
    (void)var;
}

stage(splat) {
    *v = ((float vector){0} + 1.0f) * ip->imm;
    next;
}
int splat(Builder *b, float imm) { return push(b, .fn=splat_, .imm=imm); }

stage(uniform) {
    *v = ((float vector){0} + 1.0f) * uni[ip->x];
    next;
}
int uniform(Builder *b, int ix) { return push(b, .fn=uniform_, .x=ix); }


stage(load) {
    float const *ptr = var[ip->x];
    if (end & 3) { __builtin_memcpy(v, ptr + end - 1,  4); }
    else         { __builtin_memcpy(v, ptr + end - 4, 16); }
    next;
}
int load(Builder *b, int ptr) { return push(b, .fn=load_, .x=ptr); }

stage(store) {
    float *ptr = var[ip->x];
    if (end & 3) { __builtin_memcpy(ptr + end - 1, v+ip->y,  4); }
    else         { __builtin_memcpy(ptr + end - 4, v+ip->y, 16); }
    next;
}
void store(Builder *b, int ptr, int val) { push(b, .fn=store_, .x=ptr, .y=val); }

stage(fadd) { *v = v[ip->x] + v[ip->y]; next; }
stage(fsub) { *v = v[ip->x] - v[ip->y]; next; }
stage(fmul) { *v = v[ip->x] * v[ip->y]; next; }
stage(fdiv) { *v = v[ip->x] / v[ip->y]; next; }

int fadd(Builder *b, int x, int y) { return push(b, .fn=fadd_, .x=x, .y=y); }
int fsub(Builder *b, int x, int y) { return push(b, .fn=fsub_, .x=x, .y=y); }
int fmul(Builder *b, int x, int y) { return push(b, .fn=fmul_, .x=x, .y=y); }
int fdiv(Builder *b, int x, int y) { return push(b, .fn=fdiv_, .x=x, .y=y); }

stage(mutate) { v[ip->x] = v[ip->y]; next; }
void mutate(Builder *b, int *x, int y) { push(b, .fn=mutate_, .x=*x, .y=y); }

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
            .x   = inst.x ? inst.x-1 - i : 0,
            .y   = inst.y ? inst.y-1 - i : 0,
            .imm = inst.imm,
        };
    }
    free(b->inst);
    free(b);
    return p;
}

void execute(Program const *p, int n, float const *uniform, float *varying[]) {
    float vector *v = calloc((size_t)p->insts, sizeof *v);
    for (int i = 0; i < n/4*4; i += 4) { p->inst->fn(p->inst,v,i+4,uniform,varying); }
    for (int i = n/4*4; i < n; i += 1) { p->inst->fn(p->inst,v,i+1,uniform,varying); }
    free(v);
}
