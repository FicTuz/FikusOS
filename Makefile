# =========================================================
#  FikusOS — Makefile
#  Минимально как Linux kernel: много вывода, много целей.
# =========================================================

# --- Компиляторы ---
CC          := clang
LD          := ld.lld
OBJCOPY     := llvm-objcopy
ASM         := nasm

# --- Флаги ---
ARCH        := i686-elf
MARCH       := i386

CFLAGS      := --target=$(ARCH) -march=$(MARCH) \
               -ffreestanding -m32 \
               -fno-pic -fno-pie -fno-stack-protector \
               -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-red-zone \
               -I. -Wall -Wextra -O2

LDFLAGS     := -m elf_i386 -e kmain --image-base=0 -Ttext 0x7E00

ASMFLAGS    := -f elf32
BOOTFLAGS   := -f bin

# --- Директории ---
BUILD_DIR   := build
KERNEL_ELF  := kernel.elf
KERNEL_BIN  := kernel.bin
BOOT_BIN    := boot.bin
OS_IMG      := $(BUILD_DIR)/os.img
OS_IMG_RAW  := $(BUILD_DIR)/os-without-compress.img

# --- Исходники ---
C_SOURCES   := \
    kernel.c \
    video/video.c \
    input/input.c \
    time/time.c \
    intr/intr.c \
    system/shell.c \
    system/command.c \
    system/cpuinfo.c \
    system/meminfo.c \
    fs/ramfs.c

ASM_SOURCES := intr/isr.asm
BOOT_SOURCE := boot.asm

# --- Объектные файлы ---
C_OBJECTS   := $(C_SOURCES:.c=.o)
ASM_OBJECTS := $(ASM_SOURCES:.asm=.o)
OBJECTS     := $(C_OBJECTS) $(ASM_OBJECTS)

# =========================================================
#  Цели по умолчанию
# =========================================================

.PHONY: all
all: $(OS_IMG)

# =========================================================
#  Сборка образа целиком
# =========================================================

$(OS_IMG): $(BOOT_BIN) $(KERNEL_BIN)
	@echo "  BUILD   $@"
	@mkdir -p $(BUILD_DIR)
	@dd if=/dev/zero of=$@ bs=512 count=2880 status=none
	@dd if=$(BOOT_BIN)   of=$@ conv=notrunc status=none
	@dd if=$(KERNEL_BIN) of=$@ bs=512 seek=1 conv=notrunc status=none
	@echo "  RAW     $(OS_IMG_RAW)"
	@cat $(BOOT_BIN) $(KERNEL_BIN) > $(OS_IMG_RAW)

# =========================================================
#  Ядро: link
# =========================================================

$(KERNEL_ELF): $(OBJECTS)
	@echo "  LD      $@"
	@echo "    objects: $(words $(OBJECTS)) files"
	@$(LD) $(LDFLAGS) -o $@ $(OBJECTS)
	@echo "  SIZE    $(KERNEL_ELF)"
	@llvm-size -A $@ 2>/dev/null | head -20 || true

$(KERNEL_BIN): $(KERNEL_ELF)
	@echo "  OBJCOPY $@"
	@$(OBJCOPY) -O binary $< $@
	@echo "  SIZE    $(KERNEL_BIN): $$(stat -c%s $@ 2>/dev/null || wc -c < $@) bytes"

# =========================================================
#  Bootloader
# =========================================================

$(BOOT_BIN): $(BOOT_SOURCE)
	@echo "  ASM     $@ <- $<"
	@$(ASM) $(BOOTFLAGS) $< -o $@
	@echo "  SIZE    $(BOOT_BIN): $$(stat -c%s $@ 2>/dev/null || wc -c < $@) bytes"

# =========================================================
#  Компиляция .c -> .o
# =========================================================

%.o: %.c
	@echo "  CC      $< -> $@"
	@$(CC) $(CFLAGS) -c $< -o $@

# =========================================================
#  Ассемблирование .asm -> .o
# =========================================================

%.o: %.asm
	@echo "  ASM     $< -> $@"
	@$(ASM) $(ASMFLAGS) $< -o $@

# =========================================================
#  Очистка
# =========================================================

.PHONY: clean
clean:
	@echo "  CLEAN   object files"
	@rm -f $(C_OBJECTS) $(ASM_OBJECTS)
	@echo "  CLEAN   kernel/boot binaries"
	@rm -f $(KERNEL_ELF) $(KERNEL_BIN) $(BOOT_BIN)
	@echo "  CLEAN   images"
	@rm -f $(OS_IMG) $(OS_IMG_RAW)
	@echo "  CLEAN   done"

.PHONY: distclean
distclean: clean
	@echo "  CLEAN   build dir"
	@rm -rf $(BUILD_DIR)

# =========================================================
#  Дополнительно — как в Linux
# =========================================================

.PHONY: size
size: $(KERNEL_ELF)
	@echo "=== Kernel size ==="
	@llvm-size $(KERNEL_ELF)

.PHONY: symbols
symbols: $(KERNEL_ELF)
	@echo "=== Kernel symbols ==="
	@llvm-nm $(KERNEL_ELF) | sort

.PHONY: sections
sections: $(KERNEL_ELF)
	@echo "=== Kernel sections ==="
	@llvm-readelf -S $(KERNEL_ELF)

.PHONY: headers
headers: $(KERNEL_ELF)
	@echo "=== Kernel ELF header ==="
	@llvm-readelf -h $(KERNEL_ELF)

.PHONY: disasm
disasm: $(KERNEL_ELF)
	@echo "=== Disassembly ==="
	@llvm-objdump -d $(KERNEL_ELF) > $(BUILD_DIR)/kernel.dis
	@echo "  saved to $(BUILD_DIR)/kernel.dis"
	@head -50 $(BUILD_DIR)/kernel.dis

.PHONY: qemu
qemu: $(OS_IMG)
	@echo "  QEMU    starting..."
	@qemu-system-i386 \
	    -drive format=raw,file=$(OS_IMG) \
	    -m 64 -cpu pentium3 \
	    -display curses

.PHONY: qemu-vnc
qemu-vnc: $(OS_IMG)
	@echo "  QEMU    VNC on 127.0.0.1:5900"
	@qemu-system-i386 \
	    -drive format=raw,file=$(OS_IMG) \
	    -m 64 -cpu pentium3

.PHONY: qemu-debug
qemu-debug: $(OS_IMG)
	@echo "  QEMU    debug mode (-d int,cpu_reset)"
	@qemu-system-i386 \
	    -drive format=raw,file=$(OS_IMG) \
	    -m 64 -cpu pentium3 \
	    -d int,cpu_reset -no-reboot 2>&1 | tail -50

.PHONY: run
run: qemu

.PHONY: rebuild
rebuild: clean all

.PHONY: help
help:
	@echo ""
	@echo "  FikusOS build system"
	@echo "  ────────────────────"
	@echo ""
	@echo "  make              — собрать всё (по умолчанию)"
	@echo "  make all          — то же самое"
	@echo "  make rebuild      — clean + all"
	@echo ""
	@echo "  make clean        — удалить .o, .elf, .bin, образ"
	@echo "  make distclean    — то же + удалить build/"
	@echo ""
	@echo "  make size         — размер секций ядра"
	@echo "  make symbols      — таблица символов ядра"
	@echo "  make sections     — секции ELF"
	@echo "  make headers      — заголовок ELF"
	@echo "  make disasm       — дизассемблер в build/kernel.dis"
	@echo ""
	@echo "  make qemu         — запустить в QEMU (curses)"
	@echo "  make qemu-vnc     — запустить в QEMU с VNC"
	@echo "  make qemu-debug   — запустить с логами исключений"
	@echo "  make run          — синоним qemu"
	@echo ""
