#ifndef __EXEC_H
#define __EXEC_H

#include "types.h"

struct proc;

int do_exec(uint64 path_addr, uint64 argv_addr);
int do_exec_static(const char *name, char *argv[], int argc);
int exec_program_for_proc(struct proc *p, const char *name, char *argv[], int argc);

#endif
