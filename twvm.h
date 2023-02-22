#pragma once
#include <stdint.h>

// Life cycle:
//   1)    create a Builder
//   2...) call its methods to define the program,
//   3)    pass ownership of Builder to compile(),
//   4...) execute() as you like with late-bound uniform array and varying pointers,
//   5)    free(Program*).

struct Builder* builder(void);
struct Program* compile(struct Builder*);
void            execute(struct Program const*, int32_t const *uniform, void *varying[]);

// Val represents a 32-bit value.
typedef struct { int id; } Val;

// Create from immediate bit pattern, load a uniform, or load a varying, and store a varying().
Val    splat(struct Builder*, int32_t bits);
Val  uniform(struct Builder*, int uni);
Val   load32(struct Builder*, int ptr, Val ix);
void store32(struct Builder*, int ptr, Val ix, Val x, Val mask);

// Float and integer math.
Val fadd(struct Builder*, Val, Val);
Val fsub(struct Builder*, Val, Val);
Val fmul(struct Builder*, Val, Val);
Val fdiv(struct Builder*, Val, Val);

Val iadd(struct Builder*, Val, Val);
Val isub(struct Builder*, Val, Val);
Val imul(struct Builder*, Val, Val);

Val shl(struct Builder*, Val, int bits);
Val shr(struct Builder*, Val, int bits);
Val sra(struct Builder*, Val, int bits);

Val bit_and(struct Builder*, Val, Val);
Val bit_or (struct Builder*, Val, Val);
Val bit_xor(struct Builder*, Val, Val);

Val ieq(struct Builder*, Val, Val);
Val ilt(struct Builder*, Val, Val);
Val feq(struct Builder*, Val, Val);
Val fne(struct Builder*, Val, Val);
Val flt(struct Builder*, Val, Val);
Val fle(struct Builder*, Val, Val);

// Control flow.
typedef struct { int id; } Label;
void   label(Label*);
void jump_if(Label, Val);
void  mutate(Val*, Val);

char* unit_tests(void);
