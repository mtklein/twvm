#pragma once

void expect_failed(char const *file, int line, char const *msg);
#define expect(x) if (!(x)) expect_failed(__FILE__,__LINE__,#x), __builtin_trap()
