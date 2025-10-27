#include "../kernel/syscall.h"

	.section .text

	.globl fork
fork:
	li a7, SYS_fork
	ecall
	ret

	.globl exit
exit:
	li a7, SYS_exit
	ecall
	ret

	.globl wait
wait:
	li a7, SYS_wait
	ecall
	ret

	.globl read
read:
	li a7, SYS_read
	ecall
	ret

	.globl kill
kill:
	li a7, SYS_kill
	ecall
	ret

	.globl exec
exec:
	li a7, SYS_exec
	ecall
	ret

	.globl getpid
getpid:
	li a7, SYS_getpid
	ecall
	ret

	.globl sbrk
sbrk:
	li a7, SYS_sbrk
	ecall
	ret

	.globl sleep
sleep:
	li a7, SYS_sleep
	ecall
	ret

	.globl uptime
uptime:
	li a7, SYS_uptime
	ecall
	ret

	.globl open
open:
	li a7, SYS_open
	ecall
	ret

	.globl write
write:
	li a7, SYS_write
	ecall
	ret

	.globl close
close:
	li a7, SYS_close
	ecall
	ret

	.globl yield
yield:
	li a7, SYS_yield
	ecall
	ret

	.globl setpriority
setpriority:
	li a7, SYS_setpriority
	ecall
	ret

	.globl getpriority
getpriority:
	li a7, SYS_getpriority
	ecall
	ret

	.globl getrunticks
getrunticks:
	li a7, SYS_getrunticks
	ecall
	ret

	.globl sem_create
sem_create:
	li a7, SYS_sem_create
	ecall
	ret

	.globl sem_wait
sem_wait:
	li a7, SYS_sem_wait
	ecall
	ret

	.globl sem_post
sem_post:
	li a7, SYS_sem_post
	ecall
	ret

	.globl rdtime
rdtime:
	li a7, SYS_rdtime
	ecall
	ret

	.globl shm_create
shm_create:
	li a7, SYS_shm_create
	ecall
	ret

	.globl shm_get
shm_get:
	li a7, SYS_shm_get
	ecall
	ret

	.globl shm_unmap
shm_unmap:
	li a7, SYS_shm_unmap
	ecall
	ret

	.globl dup
dup:
	li a7, SYS_dup
	ecall
	ret

	.globl pipe
pipe:
	li a7, SYS_pipe
	ecall
	ret

	.globl fstat
fstat:
	li a7, SYS_fstat
	ecall
	ret

	.globl chdir
chdir:
	li a7, SYS_chdir
	ecall
	ret

	.globl mkdir
mkdir:
	li a7, SYS_mkdir
	ecall
	ret

	.globl unlink
unlink:
	li a7, SYS_unlink
	ecall
	ret

	.globl link
link:
	li a7, SYS_link
	ecall
	ret

	.globl debugfs
debugfs:
	li a7, SYS_debugfs
	ecall
	ret

	.globl crash
crash:
	li a7, SYS_crash
	ecall
	ret

