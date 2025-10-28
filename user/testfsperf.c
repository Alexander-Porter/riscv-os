#include "user.h"

static char *utoa10(unsigned v, char *buf)
{
    char tmp[16]; int n = 0;
    if (v == 0) { buf[0] = '0'; buf[1] = 0; return buf; }
    while (v) { tmp[n++] = '0' + (v % 10); v /= 10; }
    for (int i = 0; i < n; i++) buf[i] = tmp[n-1-i];
    buf[n] = 0; return buf;
}

static void small_files_bench(void)
{
    const int files = 16;
    const char *name = "smf";
    uint64 t0 = rdtime();
    for (int i = 0; i < files; i++) {
        char fn[32]; char num[16];
        strcpy(fn, name);
        int k = (int)strlen(fn);
        fn[k++] = '_'; fn[k] = 0;
        utoa10((unsigned)i, num);
        int j = 0; while (num[j] && k < (int)sizeof(fn)-1) { fn[k++] = num[j++]; }
        fn[k] = 0;
        int fd = open(fn, O_CREATE | O_RDWR);
        if (fd < 0) { printf("perf: open %s failed\n", fn); exit(-1); }
        int v = i;
        if (write(fd, &v, sizeof(v)) != (int)sizeof(v)) { printf("perf: write failed\n"); exit(-1); }
        close(fd);
    }
    uint64 t1 = rdtime();
    printf("perf_small_files: %d files in %lu ticks\n", files, (unsigned long)(t1 - t0));

    // cleanup
    for (int i = 0; i < files; i++) {
        char fn[32]; char num[16];
        strcpy(fn, name);
        int k = (int)strlen(fn);
        fn[k++] = '_'; fn[k] = 0;
        utoa10((unsigned)i, num);
        int j = 0; while (num[j] && k < (int)sizeof(fn)-1) { fn[k++] = num[j++]; }
        fn[k] = 0;
        unlink(fn);
    }
}

static void large_file_bench(void)
{
    const int kb = 256; // 256KB (lighter workload to avoid buffer exhaustion)
    char buf[1024];
    printf("Preparing %dKB large file write benchmark...\n", kb);
    memset(buf, 0x5a, sizeof(buf));
    printf("Starting large file write benchmark...\n");
    uint64 t0 = rdtime();
    int fd = open("large_file", O_CREATE | O_RDWR);
    if (fd < 0) { printf("perf: open large_file failed\n"); exit(-1); }
    for (int i = 0; i < kb; i++) {
        if (write(fd, buf, sizeof(buf)) != (int)sizeof(buf)) { printf("perf: write large failed\n"); exit(-1); }
    }
    printf("Large file write benchmark completed, closing file...\n");
    close(fd);
    uint64 t1 = rdtime();
    printf("perf_large_file: %dKB in %lu ticks\n", kb, (unsigned long)(t1 - t0));
    unlink("large_file");
}

int main(void)
{
    small_files_bench();
    large_file_bench();
    printf("testfs_perf: DONE\n");
    // 仅当该程序作为 init(pid==1) 运行时才保持循环，避免默认测试被阻塞
    if (getpid() == 1) {
        for (;;)
            sleep(1000);
    }
    exit(0);
}
