# Compiler and tools
CC = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
QEMU = qemu-system-riscv64

# Directories and files
KERNEL_ELF = kernel.elf
KERNEL_BIN = kernel.bin
ENTRY = kernel/entry.o
START = kernel/start.o
UART = kernel/uart.c
LINKER_SCRIPT = kernel/kernel.ld

# Compilation flags
CFLAGS = -Wall -O2 -ffreestanding -nostdlib -mcmodel=medany
LDFLAGS = -T $(LINKER_SCRIPT) -nostdlib -nostartfiles

# Default target
all: $(KERNEL_BIN)

# Build kernel binary
$(KERNEL_BIN): $(KERNEL_ELF)
	$(OBJCOPY) -O binary $< $@

$(KERNEL_ELF): $(ENTRY) $(START) $(UART)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

# Clean up
clean:
	rm -f $(KERNEL_ELF) $(KERNEL_BIN)

# Run QEMU
qemu: $(KERNEL_BIN)
	$(QEMU) -machine virt -nographic -kernel $(KERNEL_BIN)
