#pragma once

#include "virtio_pci.h"

#ifdef __cplusplus
extern "C" {
#endif

int virtio_gpu_get_display_info(virtio_pci_device_t *dev, uint32_t *width, uint32_t *height);
int virtio_gpu_resource_create_2d(virtio_pci_device_t *dev, uint32_t resource_id, uint32_t width, uint32_t height, uint32_t format);
int virtio_gpu_resource_attach_backing(virtio_pci_device_t *dev, uint32_t resource_id, uint64_t phys_addr, uint64_t size);
int virtio_gpu_set_scanout(virtio_pci_device_t *dev, uint32_t scanout_id, uint32_t resource_id, uint32_t width, uint32_t height);
int virtio_gpu_transfer_to_host_2d(virtio_pci_device_t *dev, uint32_t resource_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint64_t offset);
int virtio_gpu_resource_flush(virtio_pci_device_t *dev, uint32_t resource_id, uint32_t x, uint32_t y, uint32_t w, uint32_t h);
int virtio_gpu_update_cursor(virtio_pci_device_t *dev, uint32_t resource_id, uint32_t x, uint32_t y, uint32_t hot_x, uint32_t hot_y);

#ifdef __cplusplus
}
#endif

