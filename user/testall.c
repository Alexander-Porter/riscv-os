// 仅保留：进程间通信（IPC）三种方式的验证
// - 信号量 + 共享内存：生产者-消费者（有界环形缓冲区）
// - 管道：客户端-服务器（逐字节发送，模拟传输延迟）

#include "user.h"

#define RING_CAP 4
#define MAX_STR 32
#define PRODUCE_N 12

struct ringbuf {
    char data[RING_CAP][MAX_STR];
    int head; // 写入位置（生产者推进）
    int tail; // 读取位置（消费者推进）
};

// 使用三个信号量：empty(可用空槽)、full(可用数据)、mtx(互斥)
static inline unsigned rnd_next(unsigned *state) {
    // 32-bit LCG
    *state = (*state) * 1664525u + 1013904223u;
    return *state;
}

static void test_ipc_prodcons_shm_sem(void)
{
    printf("[pc] begin (shm+sem)\n");

    // 1) 创建共享内存并初始化环形缓冲
    int shmid = shm_create();
    if (shmid < 0) { printf("[pc] shm_create failed\n"); exit(-1); }
    struct ringbuf *rb = (struct ringbuf *)shm_get(shmid);
    if ((long)rb == -1) { printf("[pc] shm_get failed\n"); exit(-1); }
    rb->head = rb->tail = 0;

    // 2) 创建信号量
    int sem_empty = sem_create(RING_CAP);
    int sem_full  = sem_create(0);
    int sem_mtx   = sem_create(1);
    if (sem_empty < 0 || sem_full < 0 || sem_mtx < 0) {
        printf("[pc] sem_create failed\n");
        exit(-1);
    }
    printf("[pc] sem ids: empty=%d full=%d mtx=%d\n", sem_empty, sem_full, sem_mtx);
    // 信号量已创建：empty(初始4)、full(初始0)、mtx(初始1)

    // 3) fork 生产者
    int pid_prod = fork();
    if (pid_prod < 0) { printf("[pc] fork producer failed\n"); exit(-1); }
    if (pid_prod == 0) {
    // 生产者子进程
        struct ringbuf *prb = (struct ringbuf *)shm_get(shmid);
        if ((long)prb == -1) {
            printf("[pc] producer shm_get failed\n");
            exit(-1);
        }
    printf("[pc] producer: start pid=%d\n", getpid());
    // 候选商品名
        const char *cats[] = {
            "large-cute-cat","small-cute-cat","large-annoying-cat","tiny-sleepy-cat",
            "angry-red-cat","smart-gray-cat","noisy-black-cat","quiet-white-cat"
        };
    unsigned rnd_state = 123456789u; // 固定种子，避免 COW 触发在 .bss
        for (int i = 0; i < PRODUCE_N; i++) {
            sem_wait(sem_empty);
            sem_wait(sem_mtx);
            const char *s = cats[rnd_next(&rnd_state) % (sizeof(cats)/sizeof(cats[0]))];
            strcpy(prb->data[prb->head], s);
            prb->head = (prb->head + 1) % RING_CAP;
            sem_post(sem_mtx);
            sem_post(sem_full);
            printf("producer: step=%d value=%s\n", i, s);
            // 模拟生产开销与节奏
            if ((i & 1) == 0) sleep(10);
        }
        // 发送结束标志字符串
    sem_wait(sem_empty);
    sem_wait(sem_mtx);
        strcpy(prb->data[prb->head], "###END###");
        prb->head = (prb->head + 1) % RING_CAP;
        sem_post(sem_mtx);
        sem_post(sem_full);
        exit(0);
    }

    // 4) fork 消费者
    int pid_cons = fork();
    if (pid_cons < 0) { printf("[pc] fork consumer failed\n"); exit(-1); }
    if (pid_cons == 0) {
    // 消费者子进程
        struct ringbuf *crb = (struct ringbuf *)shm_get(shmid);
        if ((long)crb == -1) {
            printf("[pc] consumer shm_get failed\n");
            exit(-1);
        }
        printf("[pc] consumer: start pid=%d\n", getpid());
        int cnt = 0;
        char val[MAX_STR];
        for (;;) {
            sem_wait(sem_full);
            sem_wait(sem_mtx);
            strcpy(val, crb->data[crb->tail]);
            crb->tail = (crb->tail + 1) % RING_CAP;
            sem_post(sem_mtx);
            sem_post(sem_empty);
            if (strcmp(val, "###END###") == 0) break;
            printf("consumer: step=%d pid=%d value=%s\n", cnt, getpid(), val);
            cnt++;
            // 模拟消费开销
            if ((cnt & 3) == 0) sleep(20);
        }
        printf("[pc] consumed %d items\n", cnt);
        exit(cnt == PRODUCE_N ? 0 : -1);
    }

    // 5) 回收两个子进程
    // 等待子进程结束
    int st1 = 0, st2 = 0;
    if (wait(&st1) < 0 || wait(&st2) < 0) { printf("[pc] wait failed\n"); exit(-1); }
    if (st1 != 0 || st2 != 0) { printf("[pc] children status: %d %d\n", st1, st2); exit(-1); }
    shm_unmap(rb);
    printf("[pc] end\n");
}

// 管道：client 从管道读，server 逐字节写入并 sleep 模拟传输延迟
static void test_ipc_pipe_client_server(void)
{
    printf("[pipe] begin (client-server, delayed)\n");
    int fds[2];
    if (pipe(fds) < 0) { printf("[pipe] pipe failed\n"); exit(-1); }

    // server: writer with delay
    int pid_srv = fork();
    if (pid_srv < 0) { printf("[pipe] fork server failed\n"); exit(-1); }
    if (pid_srv == 0) {
        close(fds[0]); // close read end
        const char *msgs[] = {"hello", "riscv", "pipe-delay", "bye"};
        char ch;
        for (int m = 0; m < 4; m++) {
            const char *s = msgs[m];
            printf("[pipe][server] send: %s\n", s);
            for (int i = 0; s[i]; i++) {
                ch = s[i];
                write(fds[1], &ch, 1);
                // 逐字节延迟，模拟链路抖动
                sleep(10);
            }
            ch = '\n';
            write(fds[1], &ch, 1);
            // 消息间额外延迟
            sleep(2);
        }
        close(fds[1]);
        exit(0);
    }

    // client: reader/validator
    int pid_cli = fork();
    if (pid_cli < 0) { printf("[pipe] fork client failed\n"); exit(-1); }
    if (pid_cli == 0) {
        close(fds[1]); // close write end
        char line[32]; int p = 0; int nmsg = 0;
        for (;;) {
            char c;
            int r = read(fds[0], &c, 1);
            if (r == 0) break; // EOF
            if (r < 0) { printf("[pipe] read error\n"); exit(-1); }
            if (c == '\n') {
                line[p] = 0; // end string
                // 基本校验：按预期顺序
                const char *expect[] = {"hello", "riscv", "pipe-delay", "bye"};
                if (nmsg >= 4 || strcmp(line, expect[nmsg]) != 0) {
                    printf("[pipe] recv mismatch idx=%d got=%s\n", nmsg, line);
                    exit(-1);
                }
                printf("[pipe][client] recv: %s\n", line);
                nmsg++; p = 0;
            } else if (p + 1 < (int)sizeof(line)) {
                line[p++] = c;
            } else {
                printf("[pipe] line too long\n");
                exit(-1);
            }
        }
        close(fds[0]);
        exit(nmsg == 4 ? 0 : -1);
    }

    // parent closes both ends and waits
    close(fds[0]); close(fds[1]);
    int st1 = 0, st2 = 0;
    if (wait(&st1) < 0 || wait(&st2) < 0) { printf("[pipe] wait failed\n"); exit(-1); }
    if (st1 != 0 || st2 != 0) { printf("[pipe] children status: %d %d\n", st1, st2); exit(-1); }
    printf("[pipe] end\n");
}

int main(void)
{
    test_ipc_prodcons_shm_sem();
    test_ipc_pipe_client_server();
    printf("testall: PASS\n");
    if (getpid() == 1) {
        // 若作为 init 运行，为了在时限内自动退出 QEMU
        crash();
    } else {
        exit(0);
    }
}
