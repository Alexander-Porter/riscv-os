# Compiler and tools
CC = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
QEMU = qemu-system-riscv64

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
	kernel/vm.c \
	kernel/string.c



# Object files
OBJ = $(SRC:.c=.o)
OBJ := $(OBJ:.S=.o)

# Compilation flags
CFLAGS = -Wall -O2 -ffreestanding -nostdlib -mcmodel=medany
LDFLAGS = -T $(LINKER_SCRIPT) -nostdlib -nostartfiles

# Default target
all: $(KERNEL_BIN)

# Build kernel binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(KERNEL_ELF): $(OBJ)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Compile source files
%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.S
	$(CC) $(CFLAGS) -c -o $@ $<

# Clean up
clean:
	rm -f $(KERNEL_ELF) $(KERNEL_BIN) $(OBJ)

# Run QEMU
qemu: $(KERNEL_BIN)
	$(QEMU) -machine virt -nographic -kernel $(KERNEL_BIN)
