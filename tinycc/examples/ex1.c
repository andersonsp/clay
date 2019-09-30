#!/usr/local/bin/tcc -run
#include <tcclib.h>

int main()
{
    #if defined(__APPLE__)
    printf("Hello Apple\n");
    #else
    printf("Hello World\n");
    #endif
    return 0;
}
