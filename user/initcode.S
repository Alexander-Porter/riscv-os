# initcode.S: The first user-space program.

.section .text
.globl _start
_start:
    # The kernel needs to know where this code starts and ends.
    .globl _initcode_start
_initcode_start:
    # For now, just idle.
    # We will later have this execute the first real user program.
loop:
    wfi
    j loop

.globl _initcode_end
_initcode_end:
