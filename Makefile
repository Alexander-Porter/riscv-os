# Compiler and tools
CC = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
QEMU = qemu-system-riscv64
LD = riscv64-unknown-elf-ld

# Directories and files
KERNEL_ELF = kernel.elf
KERNEL_BIN = kernel.bin
LINKER_SCRIPT = kernel/kernel.ld

# Source files
SRC = \
	kernel/entry.S \
	kernel/start.c \
	kernel/uart.c \
	kernel/console.c \
	kernel/main.c \
	kernel/printf.c \
	kernel/kalloc.c \
	kernel/spinlock.c \
	kernel/proc.c \
	kernel/syscall.c \
	kernel/sysproc.c \
	kernel/vm.c \
	kernel/string.c \
	kernel/list.c \
	kernel/exec.c \
	kernel/user_programs.c \
	kernel/semaphore.c \
	kernel/swtch.S \
	kernel/trampoline.S \
	kernel/trap/kernelvec.S \
	kernel/trap/trap.c \
	kernel/trap/timer.c \
	kernel/trap/test.c


# Object files
OBJ = $(SRC:.c=.o)
OBJ := $(OBJ:.S=.o)

# 初始用户程序 initcode
INITCODE_OBJ = kernel/initcode_bin.o
INITCODE_OUT = user/initcode.out
INITCODE_BIN = user/initcode.bin

user/initcode.o: user/initcode.S
	$(CC) $(CFLAGS) -c -o $@ $<

$(INITCODE_OUT): user/initcode.o
	riscv64-unknown-elf-ld -N -e _start -Ttext 0 -o $@ $<

$(INITCODE_BIN): $(INITCODE_OUT)
	$(OBJCOPY) -O binary $< $@

$(INITCODE_OBJ): $(INITCODE_BIN)
	riscv64-unknown-elf-ld -r -b binary -o $@ $<


# 用户态通用对象与程序
USER_COMMON_OBJ = user/start.o user/usys.o user/lib.o user/printf.o
USER_PROGS = init  testsyscall2 testprocess testcow testsbrkbench
USER_PROG_OBJ = $(addprefix user/, $(addsuffix .o, $(USER_PROGS)))
USER_OUT = $(addprefix user/, $(addsuffix .out, $(USER_PROGS)))
USER_BIN = $(addprefix user/, $(addsuffix .bin, $(USER_PROGS)))
USER_OBJ_BIN = $(addprefix kernel/, $(addsuffix _bin.o, $(USER_PROGS)))
USER_CFLAGS = -Wall -Og -g -ffreestanding -nostdlib -fno-builtin -mcmodel=medany -fno-pie -no-pie -Iuser

user/%.out: user/%.o $(USER_COMMON_OBJ)
	riscv64-unknown-elf-ld -N -T user/user.ld -o $@ $^

user/%.bin: user/%.out
	$(OBJCOPY) -O binary $< $@

kernel/%_bin.o: user/%.bin
	riscv64-unknown-elf-ld -r -b binary -o $@ $<

user/%.o: user/%.c
	$(CC) $(USER_CFLAGS) -c -o $@ $<

user/%.o: user/%.S
	$(CC) $(USER_CFLAGS) -c -o $@ $<


user/usys.S: user/usys.pl kernel/syscall.h
	perl $< kernel/syscall.h > $@

# Compilation flags
CFLAGS = -Wall -Og -g -ffreestanding -nostdlib -mcmodel=medany
LDFLAGS = -T $(LINKER_SCRIPT) -nostdlib -nostartfiles

# Default target
all: $(KERNEL_BIN)

# Build kernel binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

EXTRA_USER_OBJ = $(INITCODE_OBJ) $(USER_OBJ_BIN)

$(KERNEL_ELF): $(OBJ) $(EXTRA_USER_OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean up
.PHONY: all clean qemu qemu-gdb

clean:
	rm -f $(KERNEL_ELF) $(KERNEL_BIN) $(OBJ) \
		$(INITCODE_OBJ) $(INITCODE_BIN) $(INITCODE_OUT) user/initcode.o \
		$(USER_COMMON_OBJ) $(USER_PROG_OBJ) $(USER_OUT) $(USER_BIN) $(USER_OBJ_BIN)

# Run QEMU (clean first)
qemu: clean $(KERNEL_BIN)
	$(QEMU) -machine virt -nographic -kernel $(KERNEL_ELF) -bios none 

# Run QEMU for GDB debugging (clean first)
qemu-gdb: clean $(KERNEL_ELF)
	@echo "Starting QEMU for GDB debugging. Connect GDB to localhost:1234"
	$(QEMU) -machine virt -nographic -kernel $(KERNEL_ELF) -s -S -bios none
