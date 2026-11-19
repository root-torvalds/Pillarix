#pragma once

#define KBD_BUFFER_SIZE 256

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../../../efi.h"

extern volatile int has_mouse_event;
extern volatile uint8_t last_scancode;

// Подключения из system/kernel/
#include "spinlock.h"
#include "smp.h"
#include "sched.h"
#include "../idt/acpi.h"
#include "../idt/apic.h"
#include "../idt/idt.h"
#include "lib.h"
#include "../alloc/allocate.h"
#include "../syscalls/gdt.h"
#include "../syscalls/syscall.h"

// Подключения из system/drivers/
#include "../../../system/drivers/idt/hda.h"
#include "../../drivers/fs/ahci.h"
#include "../../drivers/fs/ext4.h"
#include "../../drivers/idt/mouse.h"
#include "../../drivers/idt/keyboard.h"
#include "../../drivers/virtio/virtio_pci.h"
#include "../../drivers/virtio/virtio_gpu.h"

// Подключения из system/graphic/
#include "../../graphic/print.h"
#include "../../graphic/screen.h"
#include "../../graphic/display_manager.h"
#include "../../graphic/qt.h"

// Подключения из system/msh/
#include "../../msh/compile/compiler.h"

#ifdef __cplusplus
}
#endif

#include "../../drivers/virtio/virtio_gpu_cmd.hpp"
