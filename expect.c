#include "expect.h"
#include <stdio.h>

#if __has_include(<execinfo.h>)
    #include <execinfo.h>
#endif

void expect_failed(char const *file, int line, char const *msg) {
    dprintf(2, "%s:%d expect(%s) failed\n", file,line,msg);
#if defined(_EXECINFO_H_)
    void *buf[64];
    int n = backtrace(buf, sizeof buf / sizeof *buf);
    backtrace_symbols_fd(buf,n,2);
#endif
}
