# initcode.S: The first user-space program.

.section .text
.globl _start
_start:
    # The kernel needs to know where this code starts and ends.
    .globl _initcode_start
_initcode_start:
    # For now, just idle.
    # We will later have this execute the first real user program.
    # Output a string using UART
    la a0, message       # Load address of the message into a0
    li a1, 0x10000000    # UART base address
1:
    lbu a2, (a0)         # Load the next byte of the message
    beqz a2, 2f          # If null terminator, exit loop
    sb a2, 0(a1)         # Store byte to UART
    addi a0, a0, 1       # Increment message pointer
    j 1b                 # Repeat
2:
    # End of UART output

    .section .rodata
message:
    .asciz "Hello, RISC-V!\n"
loop:
    wfi
    j loop

.globl _initcode_end
_initcode_end:
