#pragma once

struct Builder* builder(void);
struct Program* compile(struct Builder*);
void            execute(struct Program const*, int n, float const *uniform, float *varying[]);

int   splat(struct Builder*, float);
int uniform(struct Builder*, int ix);
int    load(struct Builder*, int ix);
void  store(struct Builder*, int ix, int val);

int fadd(struct Builder*, int, int);
int fsub(struct Builder*, int, int);
int fmul(struct Builder*, int, int);
int fdiv(struct Builder*, int, int);

void mutate(struct Builder*, int*, int);

// jump
