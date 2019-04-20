
/* this is a simple shared library for use with testloadso. */

#include <stdio.h>

extern __attribute__((visibility("default"))) int hello(const char *s);

int hello(const char *s)
{
    return puts(s);
}

