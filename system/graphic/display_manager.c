#include <stdint.h>
#include "display_manager.h"
#include "../drivers/virtio/virtio_pci.h"

#define DM_SCREEN_RESOURCE_ID                  1
#define DM_CURSOR_RESOURCE_ID                  2

#define DM_CMD_RESOURCE_CREATE_2D              0x0101
#define DM_CMD_RESOURCE_ATTACH_BACKING         0x0106
#define DM_CMD_TRANSFER_TO_HOST_2D             0x0105
#define DM_CMD_RESOURCE_FLUSH                  0x0104
#define DM_CMD_UPDATE_CURSOR                   0x0300
#define DM_CMD_MOVE_CURSOR                     0x0301
#define DM_RESP_OK_NODATA                      0x1100
#define DM_FLAG_FENCE                          1

extern virtio_pci_device_t my_gpu;
extern uint64_t kernel_virtual_to_physical(void *virtual_addr);
extern int virtio_dev_send_command(virtio_pci_device_t *vdev, uint16_t queue_index, void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len);
extern int virtio_dev_send_cursor_command(void *cmd_buf, uint32_t cmd_len, void *resp_buf, uint32_t resp_len);

__attribute__((aligned(4096))) static uint8_t hardware_cursor_buffer[64 * 64 * 4];
static uint64_t cursor_fence_counter = 1;

#pragma pack(push, 4)

struct dm_transfer_to_host_2d {
    uint32_t type; uint32_t flags; uint64_t fence_id; uint32_t ctx_id; uint32_t hdr_padding;
    uint32_t rx; uint32_t ry; uint32_t rwidth; uint32_t rheight;
    uint64_t offset; uint32_t resource_id; uint32_t padding;
};

struct dm_resource_flush {
    uint32_t type; uint32_t flags; uint64_t fence_id; uint32_t ctx_id; uint32_t hdr_padding;
    uint32_t rx; uint32_t ry; uint32_t rwidth; uint32_t rheight;
    uint32_t resource_id; uint32_t padding;
};

struct dm_update_cursor {
    uint32_t type; uint32_t flags; uint64_t fence_id; uint32_t ctx_id; uint32_t hdr_padding;
    uint32_t scanout_id; uint32_t x; uint32_t y; uint32_t pos_padding;
    uint32_t resource_id; uint32_t hot_x; uint32_t hot_y; uint32_t padding;
};

struct dm_cursor_attach_packet {
    struct virtio_gpu_resource_attach_backing attach;
    struct virtio_gpu_mem_entry entry;
};

#pragma pack(pop)

static struct dm_update_cursor global_dma_cursor_cmd;

int display_manager_init_hardware_cursor(void *cursor_rgba_buffer) {
    (void)cursor_rgba_buffer;

    static const char cursor16[16][16] = {
     {1,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0}, {1,1,0,0,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,1,0,0,0,0,0,0,0,0,0,0,0,0,0}, {1,2,2,1,0,0,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,1,0,0,0,0,0,0,0,0,0,0,0}, {1,2,2,2,2,1,0,0,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,1,0,0,0,0,0,0,0,0,0}, {1,2,2,2,2,2,2,1,0,0,0,0,0,0,0,0},
     {1,2,2,2,2,2,2,2,1,0,0,0,0,0,0,0}, {1,2,2,2,2,2,1,1,1,1,0,0,0,0,0,0},
     {1,2,2,1,2,2,1,0,0,0,0,0,0,0,0,0}, {1,2,1,0,1,2,2,1,0,0,0,0,0,0,0,0},
     {1,1,0,0,1,2,2,1,0,0,0,0,0,0,0,0}, {0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0},
     {0,0,0,0,0,1,2,2,1,0,0,0,0,0,0,0}, {0,0,0,0,0,0,1,1,0,0,0,0,0,0,0,0}
    };

    for (int i = 0; i < 64 * 64 * 4; i++) {
        hardware_cursor_buffer[i] = 0x00;
    }

    for (int y = 0; y < 16; y++) {
        for (int x = 0; x < 16; x++) {
            uint32_t byte_index = (y * 64 * 4) + (x * 4);
            char val = cursor16[y][x];
            if (val == 1) {
                hardware_cursor_buffer[byte_index + 0] = 0x00;
                hardware_cursor_buffer[byte_index + 1] = 0x00;
                hardware_cursor_buffer[byte_index + 2] = 0x00;
                hardware_cursor_buffer[byte_index + 3] = 0xFF;
            }
            else if (val == 2) {
                hardware_cursor_buffer[byte_index + 0] = 0xFF;
                hardware_cursor_buffer[byte_index + 1] = 0xFF;
                hardware_cursor_buffer[byte_index + 2] = 0xFF;
                hardware_cursor_buffer[byte_index + 3] = 0xFF;
            }
        }
    }

    struct virtio_gpu_ctrl_hdr resp_hdr;
    int status;

    unsigned char *raw_cmd = (unsigned char *)&global_dma_cursor_cmd;
    for (uint32_t i = 0; i < sizeof(global_dma_cursor_cmd); i++) raw_cmd[i] = 0;

    struct virtio_gpu_resource_create_2d cmd_c2d;
    unsigned char *raw_c2d = (unsigned char *)&cmd_c2d;
    for (uint32_t i = 0; i < sizeof(cmd_c2d); i++) raw_c2d[i] = 0;

    cmd_c2d.hdr.type = DM_CMD_RESOURCE_CREATE_2D;
    cmd_c2d.resource_id = DM_CURSOR_RESOURCE_ID;
    cmd_c2d.format = VIRTIO_GPU_FORMAT_B8G8R8A8_UNORM;
    cmd_c2d.width = 64;
    cmd_c2d.height = 64;

    status = virtio_dev_send_command(&my_gpu, 0, &cmd_c2d, sizeof(cmd_c2d), &resp_hdr, sizeof(resp_hdr));
    if (status != 0 || resp_hdr.type != DM_RESP_OK_NODATA) return -1;

    struct dm_cursor_attach_packet packet;
    unsigned char *raw_packet = (unsigned char *)&packet;
    for (uint32_t i = 0; i < sizeof(packet); i++) raw_packet[i] = 0;

    packet.attach.hdr.type = DM_CMD_RESOURCE_ATTACH_BACKING;
    packet.attach.resource_id = DM_CURSOR_RESOURCE_ID;
    packet.attach.nr_entries = 1;
    packet.entry.addr = kernel_virtual_to_physical(hardware_cursor_buffer);
    packet.entry.length = 64 * 64 * 4;

    status = virtio_dev_send_command(&my_gpu, 0, &packet, sizeof(packet), &resp_hdr, sizeof(resp_hdr));
    if (status != 0 || resp_hdr.type != DM_RESP_OK_NODATA) return -2;

    struct dm_transfer_to_host_2d cmd_trans;
    unsigned char *raw_trans = (unsigned char *)&cmd_trans;
    for (uint32_t i = 0; i < sizeof(cmd_trans); i++) raw_trans[i] = 0;

    cmd_trans.type = DM_CMD_TRANSFER_TO_HOST_2D;
    cmd_trans.rx = 0; cmd_trans.ry = 0; cmd_trans.rwidth = 64; cmd_trans.rheight = 64;
    cmd_trans.offset = 0;
    cmd_trans.resource_id = DM_CURSOR_RESOURCE_ID;

    status = virtio_dev_send_command(&my_gpu, 0, &cmd_trans, sizeof(cmd_trans), &resp_hdr, sizeof(resp_hdr));
    if (status != 0 || resp_hdr.type != DM_RESP_OK_NODATA) return -3;

    global_dma_cursor_cmd.type = DM_CMD_UPDATE_CURSOR;
    global_dma_cursor_cmd.flags = DM_FLAG_FENCE;
    global_dma_cursor_cmd.fence_id = cursor_fence_counter++;
    global_dma_cursor_cmd.scanout_id = 0;
    global_dma_cursor_cmd.x = 1920 / 2;
    global_dma_cursor_cmd.y = 1080 / 2;
    global_dma_cursor_cmd.resource_id = DM_CURSOR_RESOURCE_ID;
    global_dma_cursor_cmd.hot_x = 1;
    global_dma_cursor_cmd.hot_y = 1;

    status = virtio_dev_send_cursor_command(&global_dma_cursor_cmd, sizeof(global_dma_cursor_cmd), &resp_hdr, sizeof(resp_hdr));

    global_dma_cursor_cmd.type = DM_CMD_MOVE_CURSOR;
    global_dma_cursor_cmd.fence_id = cursor_fence_counter++;
    virtio_dev_send_cursor_command(&global_dma_cursor_cmd, sizeof(global_dma_cursor_cmd), &resp_hdr, sizeof(resp_hdr));

    return status;
}

int display_manager_move_cursor(uint32_t x, uint32_t y) {
    struct virtio_gpu_ctrl_hdr resp_hdr;

    global_dma_cursor_cmd.type = DM_CMD_MOVE_CURSOR;
    global_dma_cursor_cmd.flags = 0;
    global_dma_cursor_cmd.fence_id = 0;
    global_dma_cursor_cmd.x = x;
    global_dma_cursor_cmd.y = y;
    global_dma_cursor_cmd.hot_x = 1;
    global_dma_cursor_cmd.hot_y = 1;

    return virtio_dev_send_cursor_command(&global_dma_cursor_cmd, sizeof(global_dma_cursor_cmd), &resp_hdr, sizeof(resp_hdr));
}

int display_manager_update_region(uint32_t x, uint32_t y, uint32_t w, uint32_t h) {
    if (x >= 1920 || y >= 1080 || w == 0 || h == 0) return 0;
    if (x + w > 1920) w = 1920 - x;
    if (y + h > 1080) h = 1080 - y;

    struct dm_transfer_to_host_2d cmd_trans;
    struct dm_resource_flush cmd_flush;
    struct virtio_gpu_ctrl_hdr resp_hdr;
    int status;

    unsigned char *raw_trans = (unsigned char *)&cmd_trans;
    for (uint32_t i = 0; i < sizeof(cmd_trans); i++) raw_trans[i] = 0;

    cmd_trans.type = DM_CMD_TRANSFER_TO_HOST_2D;
    cmd_trans.rx = x; cmd_trans.ry = y; cmd_trans.rwidth = w; cmd_trans.rheight = h;
    cmd_trans.offset = (uint64_t)((y * 1920 + x) * 4);
    cmd_trans.resource_id = DM_SCREEN_RESOURCE_ID;

    status = virtio_dev_send_command(&my_gpu, 0, &cmd_trans, sizeof(cmd_trans), &resp_hdr, sizeof(resp_hdr));
    if (status != 0 || resp_hdr.type != DM_RESP_OK_NODATA) return -1;

    unsigned char *raw_flush = (unsigned char *)&cmd_flush;
    for (uint32_t i = 0; i < sizeof(cmd_flush); i++) raw_flush[i] = 0;

    cmd_flush.type = DM_CMD_RESOURCE_FLUSH;
    cmd_flush.rx = x; cmd_flush.ry = y; cmd_flush.rwidth = w; cmd_flush.rheight = h;
    cmd_flush.resource_id = DM_SCREEN_RESOURCE_ID;

    return virtio_dev_send_command(&my_gpu, 0, &cmd_flush, sizeof(cmd_flush), &resp_hdr, sizeof(resp_hdr));
}
