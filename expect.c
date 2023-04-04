#include "expect.h"
#include <execinfo.h>
#include <stdio.h>

void expect_failed(char const *file, int line, char const *msg) {
    dprintf(2, "%s:%d expect(%s) failed\n", file,line,msg);
    void *buf[64];
    int n = backtrace(buf, sizeof buf / sizeof *buf);
    backtrace_symbols_fd(buf+1,n-1,2);
}
