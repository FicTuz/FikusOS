#!/bin/sh
set -e

echo "[info] preparing to build..."
mkdir -p build

CC=clang
LD=ld.lld
OBJCOPY=llvm-objcopy

CFLAGS="--target=i686-elf -march=i386 -ffreestanding -m32 -fno-pic -fno-pie -fno-stack-protector -I. -Wall -Wextra -O2 -mno-sse -mno-sse2 -mno-mmx -mno-80387 -mno-red-zone"

echo "[info] compiling C sources..."
$CC $CFLAGS -c kernel.c            -o kernel.o
$CC $CFLAGS -c video/video.c       -o video/video.o
$CC $CFLAGS -c input/input.c       -o input/input.o
$CC $CFLAGS -c time/time.c         -o time/time.o
$CC $CFLAGS -c intr/intr.c         -o intr/intr.o
$CC $CFLAGS -c system/shell.c      -o system/shell.o
$CC $CFLAGS -c gui/gui.c           -o gui/gui.o
$CC $CFLAGS -c system/meminfo.c    -o system/meminfo.o
$CC $CFLAGS -c system/cpuinfo.c    -o system/cpuinfo.o
$CC $CFLAGS -c system/command.c    -o system/command.o
echo "[info] assembling intr/isr.asm..."
nasm -f elf32 intr/isr.asm -o intr/isr.o

echo "[info] linking kernel..."
$LD -m elf_i386 -e kmain --image-base=0 -Ttext 0x7E00 \
    -o kernel.elf \
    kernel.o \
    video/video.o \
    input/input.o \
    time/time.o \
    intr/intr.o \
    intr/isr.o \
    system/shell.o \
    system/command.o \
    gui/gui.o \
    system/meminfo.o \
    system/cpuinfo.o 

echo "[info] making kernel to binary..."
$OBJCOPY -O binary kernel.elf kernel.bin

echo "[info] building bootloader..."
nasm -f bin boot.asm -o boot.bin

echo "[info] building image..."
dd if=/dev/zero  of=./build/os.img bs=512 count=2880 2>/dev/null
dd if=boot.bin   of=./build/os.img conv=notrunc 2>/dev/null
dd if=kernel.bin of=./build/os.img bs=512 seek=1 conv=notrunc 2>/dev/null

cat boot.bin kernel.bin > ./build/os-without-compress.img

echo "[info] cleaning..."
rm -f -v ./*.o ./video/*.o ./input/*.o ./time/*.o ./intr/*.o

echo "[ok] done"
ls -la kernel.elf kernel.bin boot.bin ./build/*.img
