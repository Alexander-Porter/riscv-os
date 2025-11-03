# Compiler and tools
CC = riscv64-unknown-elf-gcc
OBJCOPY = riscv64-unknown-elf-objcopy
QEMU = qemu-system-riscv64
LD = riscv64-unknown-elf-ld
HOSTCC = gcc
HOSTCFLAGS = -Wall -Werror -std=gnu11

MKFS = tools/mkfs
FS_IMG = fs.img

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
	kernel/sleeplock.c \
	kernel/bio.c \
	kernel/proc.c \
	kernel/syscall.c \
	kernel/sysproc.c \
	kernel/sysfile.c \
	kernel/vm.c \
	kernel/string.c \
	kernel/list.c \
	kernel/exec.c \
	kernel/user_programs.c \
	kernel/semaphore.c \
	kernel/shm.c \
	kernel/file.c \
	kernel/fs.c \
	kernel/log.c \
	kernel/pipe.c \
	kernel/plic.c \
	kernel/virtio_disk.c \
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
USER_PROGS = init  testsyscall2 testprocess testcow testsbrkbench testfsperf testfsrecover testfsall
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
# 允许目标附加编译标志（例如选择 init 程序）
CFLAGS += $(EXTRA_CFLAGS)
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
		$(USER_COMMON_OBJ) $(USER_PROG_OBJ) $(USER_OUT) $(USER_BIN) $(USER_OBJ_BIN) \
		$(MKFS) $(FS_IMG)

# 方便地重置文件系统镜像：删除并重新生成空镜像
.PHONY: reset-fs
reset-fs:
	rm -f $(FS_IMG)
	$(MAKE) $(FS_IMG)

# Host mkfs 工具（直接使用 kernel/param.h 中的 FSSIZE 默认值）
$(MKFS): tools/mkfs.c tools/fs_format.h kernel/param.h kernel/stat.h kernel/types.h
	$(HOSTCC) $(HOSTCFLAGS) -I. -o $@ tools/mkfs.c

# 生成初始文件系统镜像（仅包含根目录）
$(FS_IMG): $(MKFS)
	./$(MKFS) $(FS_IMG)

QEMUFLAGS = -machine virt -bios none -kernel $(KERNEL_ELF) -nographic -m 128M
QEMUFLAGS += -global virtio-mmio.force-legacy=false
QEMUFLAGS += -drive file=$(FS_IMG),if=none,format=raw,id=hd0
QEMUFLAGS += -device virtio-blk-device,drive=hd0,bus=virtio-mmio-bus.0

# Run QEMU (preserve existing build and fs.img)
qemu: $(KERNEL_BIN) $(FS_IMG)
	-$(QEMU) $(QEMUFLAGS) || true

# Run QEMU for GDB debugging (preserve existing build and fs.img)
qemu-gdb: $(KERNEL_ELF) $(FS_IMG)
	@echo "Starting QEMU for GDB debugging. Connect GDB to localhost:1234"
	$(QEMU) $(QEMUFLAGS) -s -S

# Run QEMU without cleaning (preserve fs.img for multi-run tests)
.PHONY: qemu-noclean
qemu-noclean: $(KERNEL_ELF) $(FS_IMG)
	$(QEMU) $(QEMUFLAGS)

# Two-stage crash recovery test:
#  1) build with RECOVERY_INIT so init is testfsrecover; first run crashes
#  2) run again (without cleaning) to verify recovery and show PASS
.PHONY: crash-test crash-test-stage1 crash-test-stage2

# Stage 1：运行并触发崩溃，输出通过 tail 展示，避免在非交互 TTY 中丢失 QEMU 标准输出
crash-test-stage1:
	$(MAKE) EXTRA_CFLAGS="-DRECOVERY_INIT" all
	@# 为了保证 Stage 1 一定是“第一次运行”，强制重置干净的 fs.img
	$(MAKE) reset-fs
	@echo "[CrashTest] Stage 1: run and crash (timeout enforced)"
	rm -f .crash_stage1.log
	timeout 10s $(QEMU) $(QEMUFLAGS) -serial file:.crash_stage1.log -monitor none || true
	@echo "[CrashTest] Stage 1 output (tail):"
	@tail -n 200 .crash_stage1.log || true

# Stage 2：直接重启，不清理镜像；同样用 tail 展示
crash-test-stage2:
	@echo "[CrashTest] Stage 2: reboot without cleaning to verify recovery"
	rm -f .crash_stage2.log
	timeout 20s $(QEMU) $(QEMUFLAGS) -serial file:.crash_stage2.log -monitor none || true
	@echo "[CrashTest] Stage 2 output (tail):"
	@tail -n 200 .crash_stage2.log || true

# 组合目标：按顺序执行两个阶段
crash-test: crash-test-stage1 crash-test-stage2

# Performance-only run: boot directly into performance test as init
.PHONY: perf-test
perf-test:
	$(MAKE) clean
	$(MAKE) EXTRA_CFLAGS="-DPERF_INIT" all $(FS_IMG)
	rm -f .perf_run.log
	timeout 60s $(QEMU) $(QEMUFLAGS) -serial file:.perf_run.log -monitor none || true
	@sleep 1
	@echo "[PerfTest] Output (tail):"
	@tail -n 200 .perf_run.log || true
