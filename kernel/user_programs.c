#include "types.h"
#include "global_func.h"
#include "user_programs.h"

extern uchar _binary_user_init_bin_start[];
extern uchar _binary_user_init_bin_end[];
extern uchar _binary_user_testfsrecover_bin_start[];
extern uchar _binary_user_testfsrecover_bin_end[];
extern uchar _binary_user_testfsall_bin_start[];
extern uchar _binary_user_testfsall_bin_end[];
extern uchar _binary_user_testall_bin_start[];
extern uchar _binary_user_testall_bin_end[];
extern uchar _binary_user_noop_bin_start[];
extern uchar _binary_user_noop_bin_end[];
extern uchar _binary_user_testcowbench_bin_start[];
extern uchar _binary_user_testcowbench_bin_end[];
extern uchar _binary_user_forkexec_child_bin_start[];
extern uchar _binary_user_forkexec_child_bin_end[];

static const struct user_program user_program_table[] = {
#ifdef RECOVERY_INIT
    // 特殊构建：用恢复测试程序作为 init
    {"init", _binary_user_testfsrecover_bin_start, _binary_user_testfsrecover_bin_end},
#elif defined(TESTALL_INIT)
    // 特殊构建：用附加功能综合测试作为 init（便于单次 QEMU 在超时内结束）
    {"init", _binary_user_testall_bin_start, _binary_user_testall_bin_end},
#elif defined(BENCH_INIT)
    // 性能基准：直接进入 fork-exec 基准程序
    {"init", _binary_user_testcowbench_bin_start, _binary_user_testcowbench_bin_end},
#else
    {"init", _binary_user_init_bin_start, _binary_user_init_bin_end},
#endif
    {"testfsrecover", _binary_user_testfsrecover_bin_start, _binary_user_testfsrecover_bin_end},
    {"testfsall", _binary_user_testfsall_bin_start, _binary_user_testfsall_bin_end},
    {"testall", _binary_user_testall_bin_start, _binary_user_testall_bin_end},
    {"noop", _binary_user_noop_bin_start, _binary_user_noop_bin_end},
    {"testcowbench", _binary_user_testcowbench_bin_start, _binary_user_testcowbench_bin_end},
    {"forkexec_child", _binary_user_forkexec_child_bin_start, _binary_user_forkexec_child_bin_end},
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
