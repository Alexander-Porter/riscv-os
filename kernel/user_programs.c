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
extern uchar _binary_user_testsbrkbench_bin_start[];
extern uchar _binary_user_testsbrkbench_bin_end[];
extern uchar _binary_user_testfsperf_bin_start[];
extern uchar _binary_user_testfsperf_bin_end[];
extern uchar _binary_user_testfsrecover_bin_start[];
extern uchar _binary_user_testfsrecover_bin_end[];

static const struct user_program user_program_table[] = {
#ifdef RECOVERY_INIT
    // 特殊构建：用恢复测试程序作为 init
    {"init", _binary_user_testfsrecover_bin_start, _binary_user_testfsrecover_bin_end},
#elif defined(PERF_INIT)
    // 性能测试构建：用性能测试作为 init
    {"init", _binary_user_testfsperf_bin_start, _binary_user_testfsperf_bin_end},
#else
    {"init", _binary_user_init_bin_start, _binary_user_init_bin_end},
#endif
    {"testsyscall2", _binary_user_testsyscall2_bin_start, _binary_user_testsyscall2_bin_end},
    {"testcow", _binary_user_testcow_bin_start, _binary_user_testcow_bin_end},
    {"testprocess", _binary_user_testprocess_bin_start, _binary_user_testprocess_bin_end},
    {"testsbrkbench", _binary_user_testsbrkbench_bin_start, _binary_user_testsbrkbench_bin_end},
    {"testfsperf", _binary_user_testfsperf_bin_start, _binary_user_testfsperf_bin_end},
    {"testfsrecover", _binary_user_testfsrecover_bin_start, _binary_user_testfsrecover_bin_end},
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
