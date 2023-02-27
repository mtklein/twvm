#pragma once

struct Builder* builder(void);
struct Program* compile(struct Builder*);
void            execute(struct Program const*, int n, float const *uniform, float *varying[]);

int   splat  (struct Builder*, float);
int   uniform(struct Builder*, int ix);
int   load   (struct Builder*, int ix);
void  store  (struct Builder*, int ix, int val);

int fadd(struct Builder*, int,int);
int fsub(struct Builder*, int,int);
int fmul(struct Builder*, int,int);
int fdiv(struct Builder*, int,int);
int fmad(struct Builder*, int,int,int);

int feq(struct Builder*, int,int);
int flt(struct Builder*, int,int);
int fle(struct Builder*, int,int);
int fgt(struct Builder*, int,int);
int fge(struct Builder*, int,int);

int band(struct Builder*, int,int);
int bor (struct Builder*, int,int);
int bxor(struct Builder*, int,int);
int bsel(struct Builder*, int cond, int t, int f);

void mutate(struct Builder*, int* var, int val);
void jump  (struct Builder*, int dst, int cond);
