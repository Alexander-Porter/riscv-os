#include "types.h"
#include "global_func.h"
#include "user_programs.h"

extern uchar _binary_user_init_bin_start[];
extern uchar _binary_user_init_bin_end[];
extern uchar _binary_user_testsyscall2_bin_start[];
extern uchar _binary_user_testsyscall2_bin_end[];
extern uchar _binary_user_testcow_bin_start[];
extern uchar _binary_user_testcow_bin_end[];
extern uchar _binary_user_testprocess_bin_start[];
extern uchar _binary_user_testprocess_bin_end[];

static const struct user_program user_program_table[] = {
    {"init", _binary_user_init_bin_start, _binary_user_init_bin_end},
    {"testsyscall2", _binary_user_testsyscall2_bin_start, _binary_user_testsyscall2_bin_end},
    {"testcow", _binary_user_testcow_bin_start, _binary_user_testcow_bin_end},
    {"testprocess", _binary_user_testprocess_bin_start, _binary_user_testprocess_bin_end},
};

const struct user_program *find_user_program(const char *name)
{
    for (uint i = 0; i < sizeof(user_program_table) / sizeof(user_program_table[0]); i++)
    {
        if (strcmp(user_program_table[i].name, name) == 0)
            return &user_program_table[i];
    }
    return 0;
}
