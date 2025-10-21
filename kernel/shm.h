#ifndef __SHM_H
#define __SHM_H

#include "types.h"

struct proc;

void shm_system_init(void);
int shm_create(void);
int shm_map_for_proc(struct proc *p, int shmid, uint64 *out_va);
int shm_unmap_for_proc(struct proc *p, uint64 va);
void shm_cleanup_process(struct proc *p);
int shm_clone_mappings(struct proc *src, struct proc *dst);

// 仅调试使用：将共享内存编号转换为虚拟地址
uint64 shm_va_for_id(int shmid);
int shm_id_from_va(uint64 va);

#endif
