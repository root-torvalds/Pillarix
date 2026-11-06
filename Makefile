CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
ASM = nasm

CFLAGS = -ffreestanding -nostdlib -mno-red-zone -fno-builtin -Wall -O2 \
         -I. \
         -Isystem

CXXFLAGS = $(CFLAGS) -fno-exceptions -fno-rtti

ASMFLAGS = -f elf64

LDFLAGS = -Wl,--subsystem,10 \
          -Wl,-entry,efi_main \
          -Wl,--dll \
          -s

C_SRCS = boot/bootloader.c \
         system/kernel/kernel/kernel.c \
         system/graphic/screen.c \
         system/kernel/idt/idt.c \
         system/drivers/idt/keyboard.c \
         system/drivers/idt/mouse.c \
         system/kernel/kernel/lib.c \
         system/kernel/idt/ahci.c \
         system/graphic/print.c \
         system/kernel/alloc/allocate.c \
         system/drivers/fs/ext4.c \
         system/kernel/syscalls/syscalls.c \
         system/drivers/virtio/virtio_pci.c \
         system/drivers/virtio/virtio_gpu.c \
         system/drivers/virtio/virtio_gpu_proto.c \
         system/kernel/idt/acpi.c \
         system/kernel/idt/apic.c \
         system/drivers/idt/hda.c \
         system/graphic/display_manager.c \
         system/myc/compile/compiler.c \
         system/graphic/qt.c

CXX_SRCS = system/drivers/virtio/virtio_gpu_cmd.cpp
ASM_SRCS = system/kernel/idt/interrupts.asm

C_OBJS = $(addprefix build/, $(notdir $(C_SRCS:.c=.o)))
ASM_OBJS = $(addprefix build/, $(notdir $(ASM_SRCS:.asm=.o)))
CXX_OBJS = $(addprefix build/, $(notdir $(CXX_SRCS:.cpp=_cpp.o)))

OBJS = $(C_OBJS) $(CXX_OBJS) $(ASM_OBJS)

vpath %.c boot system system/kernel/kernel system/graphic system/kernel/idt system/drivers/idt system/kernel/alloc system/drivers/fs system/kernel/syscalls system/drivers/virtio system/myc/compile
vpath %.cpp system/drivers/idt system/drivers/virtio
vpath %.asm system/kernel/idt

all: clean build run

build: | build_dir $(OBJS)
	$(CXX) $(CXXFLAGS) $(LDFLAGS) -o BOOTX64.EFI $(OBJS)

	mkdir -p iso_root/EFI/BOOT
	cp BOOTX64.EFI iso_root/EFI/BOOT/BOOTX64.EFI
	echo "FS0:\\EFI\\BOOT\\BOOTX64.EFI" > iso_root/startup.nsh

	dd if=/dev/zero of=iso_root/efiboot.img bs=1M count=64
	mformat -i iso_root/efiboot.img -F -F -c 1 -v "EFI_BOOT" ::
	mmd -i iso_root/efiboot.img ::/EFI
	mmd -i iso_root/efiboot.img ::/EFI/BOOT
	mcopy -i iso_root/efiboot.img BOOTX64.EFI ::/EFI/BOOT/BOOTX64.EFI

	xorriso -as mkisofs -R -f -e efiboot.img -no-emul-boot -o analos.iso iso_root

build/%.o: %.c | build_dir
	$(CC) $(CFLAGS) -c $< -o $@

build/%_cpp.o: %.cpp | build_dir
	$(CXX) $(CXXFLAGS) -c $< -o $@

build/%.o: %.asm | build_dir
	$(ASM) $(ASMFLAGS) $< -o $@

build_dir:
	mkdir -p build

run:
	qemu-system-x86_64 \
		-machine q35 \
		-bios ./OVMF.fd \
		-m 512M \
		-vga virtio \
		-global virtio-gpu-pci.xres=1920 \
		-global virtio-gpu-pci.yres=1080 \
		-display sdl \
		-net none \
		-serial stdio \
		-cdrom analos.iso \
		-drive id=ahcidisk,file=disk.img,if=none,format=raw,cache=writethrough \
		-device ahci,id=ahci \
		-device ide-hd,drive=ahcidisk,bus=ahci.0 \
		-audiodev pipewire,id=snd0 \
		-device intel-hda \
		-device hda-output,audiodev=snd0 \
		-d int \
		-D qemu.log

clean:
	rm -f BOOTX64.EFI analos.iso
	rm -rf iso_root
	rm -rf build

.PHONY: all build run clean build_dir
