#ifndef __USER_PROGRAMS_H
#define __USER_PROGRAMS_H

#include "types.h"

struct user_program
{
    const char *name;      // 程序名称
    const uchar *start;    // 镜像起始地址
    const uchar *end;      // 镜像结束地址
};

const struct user_program *find_user_program(const char *name);

#endif
