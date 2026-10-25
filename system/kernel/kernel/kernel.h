#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include "../../../efi.h" // efi.h лежит в корне проекта

extern volatile int has_mouse_event;
extern volatile uint8_t last_scancode;

// Подключения из system/kernel/
#include "../idt/apic.h"
#include "../idt/idt.h"
#include "lib.h"
#include "../alloc/allocate.h"

// Подключения из system/drivers/
#include "../../../system/drivers/idt/hda.h"       // или относительный путь ниже
#include "../../drivers/fs/ahci.h"
#include "../../drivers/fs/ext4.h"
#include "../../drivers/idt/mouse.h"
#include "../../drivers/idt/keyboard.h"
#include "../../drivers/virtio/virtio_pci.h"
#include "../../drivers/virtio/virtio_gpu.h"
// #include "../../system/drivers/virtio/virtio_queue.h"
// #include "../../system/drivers/virtio/virtio_gpu_proto.h"

// Подключения из system/graphic/
#include "../../graphic/print.h"
#include "../../graphic/screen.h"
#include "../../graphic/display_manager.h"

#ifdef __cplusplus
}
#endif

#include "../../drivers/virtio/virtio_gpu_cmd.hpp"
