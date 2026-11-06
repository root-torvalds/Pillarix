#pragma once

#include <stdint.h>
#include <stddef.h>

#define DISPLAY_WIDTH                          1024
#define DISPLAY_HEIGHT                         768
#define GPU_RESOURCE_ID                        1

#define VIRTIO_PCI_VENDOR_ID                   0x1AF4
#define VIRTIO_PCI_DEVICE_ID_GPU               0x1050

#define VIRTIO_PCI_CAP_COMMON_CFG              1
#define VIRTIO_PCI_CAP_NOTIFY_CFG              2
#define VIRTIO_PCI_CAP_ISR_CFG                 3
#define VIRTIO_PCI_CAP_DEVICE_CFG              4
#define VIRTIO_PCI_CAP_PCI_CFG                 5

#define VIRTIO_STATUS_RESET                    0
#define VIRTIO_STATUS_ACKNOWLEDGE              1
#define VIRTIO_STATUS_DRIVER                   2
#define VIRTIO_STATUS_FEATURES_OK              8
#define VIRTIO_STATUS_DRIVER_OK                4
#define VIRTIO_STATUS_FAILED                   128

#define VIRTIO_F_VERSION_1                     32
#define VIRTIO_GPU_F_VIRGL                     0

#define VIRTQ_DESC_F_NEXT                      1
#define VIRTQ_DESC_F_WRITE                     2

#define VIRTIO_GPU_CMD_RESOURCE_CREATE_2D      0x0101
#define VIRTIO_GPU_CMD_RESOURCE_ATTACH_BACKING 0x0102
#define VIRTIO_GPU_CMD_SET_SCANOUT             0x0103
#define VIRTIO_GPU_CMD_RESOURCE_FLUSH          0x0104
#define VIRTIO_GPU_RESP_OK_NODATA              0x1100
#define VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM       1

#pragma pack(push, 1)

struct virtio_pci_cap {
    uint8_t  cap_vndr;
    uint8_t  cap_next;
    uint8_t  cap_len;
    uint8_t  cfg_type;
    uint8_t  bar;
    uint8_t  id;
    uint8_t  padding[2];
    uint32_t offset;
    uint32_t length;
};

struct virtio_pci_notify_cap {
    struct virtio_pci_cap cap;
    uint32_t notify_off_multiplier;
};

struct virtio_pci_common_cfg {
    uint32_t device_feature_select;
    uint32_t device_feature;
    uint32_t driver_feature_select;
    uint32_t driver_feature;
    uint16_t config_msix_vector;
    uint16_t num_queues;
    uint8_t  device_status;
    uint8_t  config_generation;
    uint16_t queue_select;
    uint16_t queue_size;
    uint16_t queue_msix_vector;
    uint16_t queue_enable;
    uint16_t queue_notify_off;
    uint64_t queue_desc;
    uint64_t queue_driver;
    uint64_t queue_device;
    uint16_t queue_notify_data;
    uint16_t queue_reset;
};

struct virtq_desc {
    uint64_t addr;
    uint32_t len;
    uint16_t flags;
    uint16_t next;
};

struct virtq_avail {
    uint16_t flags;
    uint16_t idx;
    uint16_t ring[];
};

struct virtq_used_elem {
    uint32_t id;
    uint32_t len;
};

struct virtq_used {
    uint16_t flags;
    uint16_t idx;
    struct virtq_used_elem ring[];
};

struct __attribute__((packed, aligned(4))) virtio_gpu_ctrl_hdr {
    uint32_t type;
    uint32_t flags;
    uint64_t fence_id;
    uint32_t ctx_id;
    uint32_t padding;
};

struct __attribute__((packed, aligned(4))) virtio_gpu_resource_create_2d {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t format;
    uint32_t width;
    uint32_t height;
};

struct __attribute__((packed, aligned(4))) virtio_gpu_mem_entry {
    uint64_t addr;
    uint32_t length;
    uint32_t padding;
};

struct __attribute__((packed, aligned(4))) virtio_gpu_resource_attach_backing {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t resource_id;
    uint32_t nr_entries;
};

struct __attribute__((packed, aligned(4))) virtio_gpu_set_scanout {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t r_x;
    uint32_t r_y;
    uint32_t r_width;
    uint32_t r_height;
    uint32_t scanout_id;
    uint32_t resource_id;
};

struct __attribute__((packed, aligned(4))) virtio_gpu_resource_flush {
    struct virtio_gpu_ctrl_hdr hdr;
    uint32_t r_x;
    uint32_t r_y;
    uint32_t r_width;
    uint32_t r_height;
    uint32_t resource_id;
    uint32_t padding;
};

#pragma pack(pop)

typedef struct {
    volatile struct virtq_desc  *desc;
    volatile struct virtq_avail *avail;
    volatile struct virtq_used  *used;
    uint16_t queue_size;
    uint16_t last_seen_used;
    uint16_t free_head;
} virtio_queue_t;

typedef struct {
    uint8_t bus;
    uint8_t slot;
    uint8_t func;
    
    volatile struct virtio_pci_common_cfg *common_cfg;
    volatile uint8_t                      *isr_cfg;
    volatile void                         *device_cfg;
    
    uint64_t notify_base_addr;
    uint32_t notify_multiplier;
    uint8_t  notify_bar;

    virtio_queue_t queues[2];
} virtio_pci_device_t;

#ifdef __cplusplus
extern "C" {
#endif

int virtio_pci_init_device(virtio_pci_device_t *vdev, uint8_t bus, uint8_t slot, uint8_t func);
int virtio_gpu_negotiate_features(virtio_pci_device_t *vdev);
int virtio_queue_setup(virtio_pci_device_t *vdev, uint16_t queue_index);
int virtio_dev_send_command(virtio_pci_device_t *vdev, uint16_t queue_index, void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len);

int  init_virtio_gpu(void);
int  virtio_gpu_setup_screen(void);
void virtio_gpu_redraw(void);

#ifdef __cplusplus
}
#endif
