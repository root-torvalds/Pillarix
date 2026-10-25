#include "../../kernel/kernel/kernel.h"
extern virtio_pci_device_t my_gpu;
extern int virtio_gpu_configure_cursor_hardware_queue(void);
#ifdef __cplusplus
extern "C" {
#endif

int init_virtio_gpu(void) {

    uint8_t bus = 0;
    uint8_t slot = 1;
    uint8_t func = 0;

    int res = virtio_pci_init_device(&my_gpu, bus, slot, func);
    if (res != 0) {
        return res;
    }

    res = virtio_gpu_negotiate_features(&my_gpu);
    if (res != 0) {
        return res;
    }

    res = virtio_queue_setup(&my_gpu, 0);
    if (res != 0) {
        return res;
    }

    res = virtio_queue_setup(&my_gpu, 1);
    if (res != 0) {
        return res;
    }

    (*my_gpu.common_cfg).device_status |= VIRTIO_STATUS_DRIVER_OK;
    return 0;
    int status = virtio_gpu_configure_cursor_hardware_queue();
    if (status == 0) {


        extern int display_manager_init_hardware_cursor(void *buf);
        display_manager_init_hardware_cursor(0);
    }
}

#ifdef __cplusplus
}
#endif
