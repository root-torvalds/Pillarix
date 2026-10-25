CC = x86_64-w64-mingw32-gcc
CXX = x86_64-w64-mingw32-g++
LD = x86_64-w64-mingw32-ld
ASM = nasm

CFLAGS = -ffreestanding -nostdlib -mno-red-zone -fno-builtin -Wall -O2 -I. -Isystem
CXXFLAGS = -ffreestanding -nostdlib -mno-red-zone -fno-builtin -fno-exceptions -fno-rtti -Wall -O2 -I. -Isystem

all: clean build run

build: analos.iso

# ==========================================
# СБОРКА ПОЛЬЗОВАТЕЛЬСКОГО ПРИЛОЖЕНИЯ И ГЕНЕРАЦИЯ app_bin.h
# ==========================================
image/user/app: user/app_main.cpp user/user_syscall.h
	mkdir -p build
	mkdir -p image/user
	$(CXX) $(CXXFLAGS) -c user/app_main.cpp -o build/app_main.o
	$(LD) -nostdlib -e program_main build/app_main.o -o image/user/app
	xxd -i image/user/app > system/kernel/kernel/app_bin.h

# ==========================================
# СБОРКА ЯДРА И ДРАЙВЕРОВ
# ==========================================
build/pe_loader.o: system/kernel/kernel/pe_loader.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/kernel/pe_loader.c -o build/pe_loader.o

build/bootloader.o: boot/bootloader.c
	mkdir -p build
	$(CC) $(CFLAGS) -c boot/bootloader.c -o build/bootloader.o

build/kernel.o: system/kernel/kernel/kernel.c image/user/app
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/kernel/kernel.c -o build/kernel.o

build/lib.o: system/kernel/kernel/lib.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/kernel/lib.c -o build/lib.o

build/allocate.o: system/kernel/alloc/allocate.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/alloc/allocate.c -o build/allocate.o

build/memory.o: system/kernel/alloc/memory.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/alloc/memory.c -o build/memory.o

build/screen.o: system/graphic/screen.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/graphic/screen.c -o build/screen.o

build/print.o: system/graphic/print.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/graphic/print.c -o build/print.o

build/display_manager.o: system/graphic/display_manager.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/graphic/display_manager.c -o build/display_manager.o

build/syscalls.o: system/kernel/syscalls/syscalls.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/syscalls/syscalls.c -o build/syscalls.o

build/idt.o: system/kernel/idt/idt.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/idt/idt.c -o build/idt.o

build/uspace.o: system/kernel/kernel/uspace.asm
	mkdir -p build
	$(ASM) -f win64 system/kernel/kernel/uspace.asm -o build/uspace.o

build/interrupts.o: system/kernel/idt/interrupts.asm
	mkdir -p build
	$(ASM) -f win64 system/kernel/idt/interrupts.asm -o build/interrupts.o

build/apic.o: system/kernel/idt/apic.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/idt/apic.c -o build/apic.o

build/acpi.o: system/kernel/idt/acpi.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/kernel/idt/acpi.c -o build/acpi.o

build/keyboard.o: system/drivers/idt/keyboard.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/idt/keyboard.c -o build/keyboard.o

build/mouse.o: system/drivers/idt/mouse.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/idt/mouse.c -o build/mouse.o

build/mouse_cpp.o: system/drivers/idt/mouse.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c system/drivers/idt/mouse.cpp -o build/mouse_cpp.o

build/hda.o: system/drivers/idt/hda.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/idt/hda.c -o build/hda.o

build/ahci.o: system/drivers/fs/ahci.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/fs/ahci.c -o build/ahci.o

build/ext4.o: system/drivers/fs/ext4.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/fs/ext4.c -o build/ext4.o

build/virtio_pci.o: system/drivers/virtio/virtio_pci.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/virtio/virtio_pci.c -o build/virtio_pci.o

build/virtio_gpu.o: system/drivers/virtio/virtio_gpu.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/virtio/virtio_gpu.c -o build/virtio_gpu.o

build/virtio_gpu_proto.o: system/drivers/virtio/virtio_gpu_proto.c
	mkdir -p build
	$(CC) $(CFLAGS) -c system/drivers/virtio/virtio_gpu_proto.c -o build/virtio_gpu_proto.o

build/virtio_gpu_cmd_cpp.o: system/drivers/virtio/virtio_gpu_cmd.cpp
	mkdir -p build
	$(CXX) $(CXXFLAGS) -c system/drivers/virtio/virtio_gpu_cmd.cpp -o build/virtio_gpu_cmd_cpp.o

# ==========================================
# ЛИНКОВКА ЯДРА (BOOTX64.EFI)
# ==========================================
BOOTX64.EFI: build/bootloader.o build/kernel.o build/lib.o build/allocate.o build/memory.o build/screen.o build/print.o build/display_manager.o build/syscalls.o build/idt.o build/uspace.o build/pe_loader.o build/interrupts.o build/apic.o build/acpi.o build/keyboard.o build/mouse.o build/mouse_cpp.o build/hda.o build/ahci.o build/ext4.o build/virtio_pci.o build/virtio_gpu.o build/virtio_gpu_proto.o build/virtio_gpu_cmd_cpp.o
	$(LD) -nostdlib -shared --subsystem 10 $^ -o BOOTX64.EFI

# ==========================================
# СОЗДАНИЕ ISO
# ==========================================
analos.iso: BOOTX64.EFI image/user/app
	mkdir -p iso_root/EFI/BOOT
	cp BOOTX64.EFI iso_root/EFI/BOOT/
	xorriso -as mkisofs -R -f -e efiboot.img -no-emul-boot -o analos.iso iso_root

# ==========================================
# ЗАПУСК QEMU (1920x1080 в окне 1024x768 с масштабированием GTK)
# ==========================================
run: analos.iso
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
	rm -f BOOTX64.EFI analos.iso qemu.log
	rm -rf build
	rm -f image/user/app system/kernel/kernel/app_bin.h
