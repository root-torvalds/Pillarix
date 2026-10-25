#include "virtio_pci.h"
#include "../../graphic/print.h"

#define DM_CURSOR_RESOURCE_ID                     3
#define DM_VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      0x0101
#define DM_VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0106
#define DM_VIRTIO_GPU_CMD_UPDATE_CURSOR           0x0300
#define DM_VIRTIO_GPU_RESP_OK_NODATA              0x1100

#define GPU_CONTROL_QUEUE_INDEX                   0
#define GPU_CURSOR_QUEUE_INDEX                    1
#define GPU_QUEUE_SIZE_DCA                        16

extern virtio_pci_device_t my_gpu;
extern uint64_t kernel_virtual_to_physical(void *virtual_addr);

#pragma pack(push, 4)
struct local_virtio_gpu_rect { uint32_t x; uint32_t y; uint32_t width; uint32_t height; };
struct local_virtio_gpu_ctrl_hdr { uint32_t type; uint32_t flags; uint64_t fence_id; uint32_t ctx_id; uint32_t padding; };
struct local_virtio_gpu_resp_display_info { struct local_virtio_gpu_ctrl_hdr hdr; struct { struct local_virtio_gpu_rect r; uint32_t enabled; uint32_t flags; } pmodes; };
struct local_virtio_gpu_transfer_to_host_2d { struct local_virtio_gpu_ctrl_hdr hdr; struct local_virtio_gpu_rect r; uint64_t offset; uint32_t resource_id; uint32_t padding; };
struct local_virtio_gpu_cursor_pos { uint32_t scanout_id; uint32_t x; uint32_t y; uint32_t padding; };
struct local_virtio_gpu_update_cursor { struct local_virtio_gpu_ctrl_hdr hdr; struct local_virtio_gpu_cursor_pos pos; uint32_t resource_id; uint32_t hot_x; uint32_t hot_y; uint32_t padding; };
#pragma pack(pop)

static uint16_t cursor_avail_idx_local = 0;

uint16_t virtio_pci_get_queue_notify_off(uint16_t queue_index) {
    volatile struct virtio_pci_common_cfg *cfg = (volatile struct virtio_pci_common_cfg *)my_gpu.common_cfg;
    if (!cfg) return 0;
    cfg->queue_select = queue_index;
    __asm__ volatile("mfence" : : : "memory");
    return cfg->queue_notify_off;
}

uint64_t virtio_pci_get_doorbell_address(uint16_t queue_index) {
    uint16_t notify_off = virtio_pci_get_queue_notify_off(queue_index);
    uint64_t physical_doorbell = my_gpu.notify_base_addr + ((uint64_t)notify_off * (uint64_t)my_gpu.notify_multiplier);
    return physical_doorbell;
}

int virtio_gpu_configure_cursor_hardware_queue(void) {
    return 0;
}

int virtio_dev_send_cursor_command(void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len) {
    (void)resp_buf;
    (void)resp_len;

    uint16_t queue_index = 1;
    virtio_queue_t *q = my_gpu.queues + queue_index;

    if (!q->desc || !q->avail || !q->used) {
        return -1001;
    }

    uint16_t idx_cmd = 0;

    q->desc[idx_cmd].addr  = kernel_virtual_to_physical(cmd_buf);
    q->desc[idx_cmd].len   = cmd_len;
    q->desc[idx_cmd].flags = 0;
    q->desc[idx_cmd].next  = 0;

    uint16_t avail_idx = q->avail->idx;
    uint16_t ring_pos = avail_idx & (q->queue_size - 1);
    q->avail->ring[ring_pos] = idx_cmd;

    __asm__ volatile("mfence" : : : "memory");
    q->avail->idx = avail_idx + 1;
    __asm__ volatile("mfence" : : : "memory");

    cursor_avail_idx_local++;

    my_gpu.common_cfg->queue_select = queue_index;
    __asm__ volatile("mfence" : : : "memory");

    uint16_t notify_off = my_gpu.common_cfg->queue_notify_off;
    uint64_t notify_addr = my_gpu.notify_base_addr + (notify_off * my_gpu.notify_multiplier);
    volatile uint16_t *doorbell = (volatile uint16_t *)notify_addr;

    *doorbell = queue_index;
    __asm__ volatile("mfence" : : : "memory");

    return 0;
}
