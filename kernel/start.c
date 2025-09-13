#include "global_func.h"

// entry.S needs one stack per CPU.
__attribute__((aligned(16))) char stack0[4096];

// entry.S jumps here in S-mode on stack0.
void start()
{
    // a C-language function needs a stack. a lot of C functions are called before paging is enabled.
    // so we need to set up a stack in entry.S.
    // after paging is enabled, we can set up a new stack for the kernel.
    main();
}
