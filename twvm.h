#pragma once

struct Builder* builder(int ptrs);
struct Program* compile(struct Builder*);
void            execute(struct Program const*, int n, void *ptr[]);

int thread_id(struct Builder*);

int  splat(struct Builder*, float);
int  load (struct Builder*, int ptr, int ix);
void store(struct Builder*, int ptr, int ix, int val);

int fadd(struct Builder*, int,int);
int fsub(struct Builder*, int,int);
int fmul(struct Builder*, int,int);
int fdiv(struct Builder*, int,int);

int feq(struct Builder*, int,int);
int flt(struct Builder*, int,int);
int fle(struct Builder*, int,int);

int band(struct Builder*, int,int);
int bor (struct Builder*, int,int);
int bxor(struct Builder*, int,int);
int bsel(struct Builder*, int cond, int t, int f);

void mutate(struct Builder*, int* var, int val);
void loop  (struct Builder*, int cond);
