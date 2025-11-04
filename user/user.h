#ifndef USER_USER_H
#define USER_USER_H

#include "../kernel/types.h"
#include <stdarg.h>
#include "../kernel/fcntl.h"
#include "../kernel/stat.h"

#define SBRK_ERROR ((char *)-1)

typedef unsigned long size_t;

typedef unsigned long uintptr_t;

// 系统调用封装
int fork(void);
void exit(int status) __attribute__((noreturn));
int wait(int *status);
int exec(const char *path, char *const argv[]);
int write(int fd, const void *buf, int n);
int read(int fd, void *buf, int n);
int open(const char *path, int omode);
int close(int fd);
int fstat(int fd, struct stat *st);
int mkdir(const char *path);
int chdir(const char *path);
int link(const char *old, const char *newp);
int unlink(const char *path);
int pipe(int fds[2]);
int getpid(void);
int sleep(int ticks);
int yield(void);
int setpriority(int prio);
int getpriority(void);
uint64 getrunticks(void);
uint64 rdtime(void);
uint64 uptime(void);
char *sbrk(int n);
int sem_create(int initial);
int sem_wait(int semid);
int sem_post(int semid);
int shm_create(void);
void *shm_get(int shmid);
int shm_unmap(void *addr);
int debugfs(int action);
int crash(void);

// 基础库函数
void *memset(void *dst, int c, size_t n);
void *memmove(void *dst, const void *src, size_t n);
void *memcpy(void *dst, const void *src, size_t n);
size_t strlen(const char *s);
int strcmp(const char *lhs, const char *rhs);
char *strcpy(char *dst, const char *src);
int strncmp(const char *lhs, const char *rhs, size_t n);
int atoi(const char *s);

// 输出相关
void printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void fprintf(int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
void vprintf(const char *fmt, va_list ap);

#endif
