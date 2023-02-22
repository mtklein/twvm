#include "twvm.h"
#include <stdio.h>
#include <stdlib.h>

int main(void) {
    for (char *err = unit_tests(); err;) {
        fprintf(stderr, "unit_tests() failed: %s\n", err);
        free(err);
        return 1;
    }
    return 0;
}
