#include "../../kernel/kernel/kernel.h"
virtio_pci_device_t my_gpu;

extern void* memset(void* ptr, int value, unsigned long num);

// 1. НИЗКОУРОВНЕВЫЙ ДОСТУП К ПОРТАМ И ШИНЕ PCI x86_64
static inline void outdword(uint16_t port, uint32_t value) {
    __asm__ volatile("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t indword(uint16_t port) {
    uint32_t ret;
    __asm__ volatile("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

uint32_t pci_read_config_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t address;
    uint32_t lbus  = (uint32_t)bus;
    uint32_t lslot = (uint32_t)slot;
    uint32_t lfunc = (uint32_t)func;

    address = (uint32_t)((lbus << 16) | (lslot << 11) | (lfunc << 8) | (offset & 0xFC) | ((uint32_t)0x80000000));

    outdword(0x0CF8, address);
    return indword(0x0CFC);
}

uint16_t pci_read_config_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config_dword(bus, slot, func, offset);
    return (uint16_t)((val >> ((offset & 2) * 8)) & 0xFFFF);
}

uint8_t pci_read_config_byte(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset) {
    uint32_t val = pci_read_config_dword(bus, slot, func, offset);
    return (uint8_t)((val >> ((offset & 3) * 8)) & 0xFF);
}

uint64_t kernel_virtual_to_physical(void *virtual_addr) {
    return (uint64_t)virtual_addr;
}

// 2. ГАРАНТИРОВАННОЕ ВЫРАВНИВАНИЕ ПАМЯТИ ПО СТРАНИЦАМ (4096 БАЙТ)
__attribute__((aligned(4096))) static uint8_t gpu_q0_desc[256 * 16];
__attribute__((aligned(4096))) static uint8_t gpu_q0_avail[6 + (256 * 2)];
__attribute__((aligned(4096))) static uint8_t gpu_q0_used[6 + (256 * 8)];

__attribute__((aligned(4096))) static uint8_t gpu_q1_desc[256 * 16];
__attribute__((aligned(4096))) static uint8_t gpu_q1_avail[6 + (256 * 2)];
__attribute__((aligned(4096))) static uint8_t gpu_q1_used[6 + (256 * 8)];

__attribute__((aligned(4096))) static uint8_t gpu_display_buffer[DISPLAY_WIDTH * DISPLAY_HEIGHT * 4];

void* kernel_alloc_pages_aligned(uint32_t size, uint32_t alignment) {
    static int alloc_counter = 0;
    void *result = 0;

    switch (alloc_counter) {

        case 0: result = (void*)gpu_q0_desc; break;
        case 1: result = (void*)gpu_q0_avail; break;
        case 2: result = (void*)gpu_q0_used; break;

        case 3: result = (void*)gpu_q1_desc; break;
        case 4: result = (void*)gpu_q1_avail; break;
        case 5: result = (void*)gpu_q1_used; break;

        case 6: result = (void*)gpu_display_buffer; break;
        default: return 0;
    }

    alloc_counter++;
    memset(result, 0, size);
    return result;
}

static uint64_t virtio_pci_get_bar_addr(uint8_t bus, uint8_t slot, uint8_t func, uint8_t bar_index) {
    uint8_t bar_offset = 0x10 + (bar_index * 4);
    uint32_t bar_low = pci_read_config_dword(bus, slot, func, bar_offset);

    if ((bar_low & 0x06) == 0x04) {
        uint32_t bar_high = pci_read_config_dword(bus, slot, func, bar_offset + 4);
        return (((uint64_t)bar_high << 32) | (bar_low & 0xFFFFFFF0));
    }

    return (bar_low & 0xFFFFFFF0);
}

// 3. РЕАЛИЗАЦИЯ ИНТЕРФЕЙСА СТАНДАРТА VIRTIO MODERN
int virtio_pci_init_device(virtio_pci_device_t *vdev, uint8_t bus, uint8_t slot, uint8_t func) {
    (*vdev).bus = bus;
    (*vdev).slot = slot;
    (*vdev).func = func;
    (*vdev).common_cfg = 0;
    (*vdev).isr_cfg = 0;
    (*vdev).device_cfg = 0;
    (*vdev).notify_base_addr = 0;

    uint16_t pci_status = pci_read_config_word(bus, slot, func, 0x06);
    if ((pci_status & 0x0010) == 0) {
        return -1;
    }

    uint8_t cap_ptr = pci_read_config_byte(bus, slot, func, 0x34);

    while (cap_ptr != 0) {
        if (cap_ptr == 0xFF) {
            break;
        }

        uint8_t cap_id = pci_read_config_byte(bus, slot, func, cap_ptr);

        if (cap_id == 0x09) {
            struct virtio_pci_cap cap;

            cap.cfg_type = pci_read_config_byte(bus, slot, func, cap_ptr + 3);
            cap.bar      = pci_read_config_byte(bus, slot, func, cap_ptr + 4);
            cap.offset   = pci_read_config_dword(bus, slot, func, cap_ptr + 8);
            cap.length   = pci_read_config_dword(bus, slot, func, cap_ptr + 12);

            if (cap.bar <= 5) {
                uint64_t bar_base = virtio_pci_get_bar_addr(bus, slot, func, cap.bar);
                uint64_t target_addr = bar_base + cap.offset;

                switch (cap.cfg_type) {
                    case VIRTIO_PCI_CAP_COMMON_CFG:
                        (*vdev).common_cfg = (volatile struct virtio_pci_common_cfg *)target_addr;
                        break;

                    case VIRTIO_PCI_CAP_ISR_CFG:
                        (*vdev).isr_cfg = (volatile uint8_t *)target_addr;
                        break;

                    case VIRTIO_PCI_CAP_DEVICE_CFG:
                        (*vdev).device_cfg = (volatile void *)target_addr;
                        break;

                    case VIRTIO_PCI_CAP_NOTIFY_CFG:
                        (*vdev).notify_base_addr = target_addr;
                        (*vdev).notify_bar = cap.bar;
                        (*vdev).notify_multiplier = pci_read_config_dword(bus, slot, func, cap_ptr + 16);
                        break;
                }
            }
        }
        cap_ptr = pci_read_config_byte(bus, slot, func, cap_ptr + 1);
    }

    if (!(*vdev).common_cfg || !(*vdev).isr_cfg || !(*vdev).notify_base_addr) {
        return -2;
    }

    (*(*vdev).common_cfg).device_status = VIRTIO_STATUS_RESET;
    while ((*(*vdev).common_cfg).device_status != VIRTIO_STATUS_RESET) {
        __asm__ volatile("pause");
    }

    (*(*vdev).common_cfg).device_status |= VIRTIO_STATUS_ACKNOWLEDGE;
    (*(*vdev).common_cfg).device_status |= VIRTIO_STATUS_DRIVER;

    return 0;
}

int virtio_gpu_negotiate_features(virtio_pci_device_t *vdev) {
    volatile struct virtio_pci_common_cfg *cfg = (*vdev).common_cfg;

    (*cfg).device_feature_select = 0;
    uint32_t host_low = (*cfg).device_feature;

    (*cfg).device_feature_select = 1;
    uint32_t host_high = (*cfg).device_feature;

    if ((host_high & 0x00000001) == 0) {
        (*cfg).device_status |= VIRTIO_STATUS_FAILED;
        return -3;
    }

    (*cfg).driver_feature_select = 0;
    (*cfg).driver_feature = host_low;

    (*cfg).driver_feature_select = 1;
    (*cfg).driver_feature = host_high;

    (*cfg).device_status |= VIRTIO_STATUS_FEATURES_OK;

    uint8_t status = (*cfg).device_status;
    if ((status & VIRTIO_STATUS_FEATURES_OK) == 0) {
        (*cfg).device_status |= VIRTIO_STATUS_FAILED;
        return -4;
    }

    return 0;
}

int virtio_queue_setup(virtio_pci_device_t *vdev, uint16_t queue_index) {
    volatile struct virtio_pci_common_cfg *cfg = (*vdev).common_cfg;

    (*cfg).queue_select = queue_index;

    uint16_t q_size = (*cfg).queue_size;
    if (q_size == 0) {
        return -5;
    }

    uint32_t desc_table_size = q_size * 16;
    uint32_t avail_ring_size = 6 + (q_size * 2);
    uint32_t used_ring_size  = 6 + (q_size * 8);

    void *desc_mem  = kernel_alloc_pages_aligned(desc_table_size, 4096);
    void *avail_mem = kernel_alloc_pages_aligned(avail_ring_size, 4096);
    void *used_mem  = kernel_alloc_pages_aligned(used_ring_size, 4096);

    if (!desc_mem || !avail_mem || !used_mem) {
        return -6;
    }

    virtio_queue_t *q = (*vdev).queues + queue_index;

    (*q).desc = (volatile struct virtq_desc *)desc_mem;
    (*q).avail = (volatile struct virtq_avail *)avail_mem;
    (*q).used = (volatile struct virtq_used *)used_mem;
    (*q).queue_size = q_size;
    (*q).last_seen_used = 0;
    (*q).free_head = 0;

    (*cfg).queue_desc   = kernel_virtual_to_physical(desc_mem);
    (*cfg).queue_driver = kernel_virtual_to_physical(avail_mem);
    (*cfg).queue_device = kernel_virtual_to_physical(used_mem);

    (*cfg).queue_enable = 1;

    return 0;
}

int virtio_dev_send_command(virtio_pci_device_t *vdev, uint16_t queue_index,
                            void *cmd_buf, uint32_t cmd_len,
                            void *resp_buf, uint32_t resp_len)
{
    virtio_queue_t *q = (*vdev).queues + queue_index;

    uint16_t idx_cmd  = 0;
    uint16_t idx_resp = 1;

    (*((*q).desc + idx_cmd)).addr  = kernel_virtual_to_physical(cmd_buf);
    (*((*q).desc + idx_cmd)).len   = cmd_len;
    (*((*q).desc + idx_cmd)).flags = VIRTQ_DESC_F_NEXT;
    (*((*q).desc + idx_cmd)).next  = idx_resp;

    (*((*q).desc + idx_resp)).addr  = kernel_virtual_to_physical(resp_buf);
    (*((*q).desc + idx_resp)).len   = resp_len;
    (*((*q).desc + idx_resp)).flags = VIRTQ_DESC_F_WRITE;
    (*((*q).desc + idx_resp)).next  = 0;

    (*((*q).avail)).flags = 0;

    uint16_t avail_idx = (*((*q).avail)).idx;
    uint16_t ring_pos = avail_idx & ((*q).queue_size - 1);
    (*((*q).avail)).ring[ring_pos] = idx_cmd;

    __asm__ volatile("mfence" : : : "memory");
    (*((*q).avail)).idx = avail_idx + 1;
    __asm__ volatile("mfence" : : : "memory");

    (*(*vdev).common_cfg).queue_select = queue_index;
    uint16_t notify_off = (*(*vdev).common_cfg).queue_notify_off;

    uint64_t notify_addr = (*vdev).notify_base_addr + (notify_off * (*vdev).notify_multiplier);
    volatile uint16_t *doorbell = (volatile uint16_t *)notify_addr;

    *doorbell = queue_index;

    volatile struct virtq_used *used = (*q).used;
    while ((*used).idx == (*q).last_seen_used) {
        __asm__ volatile("pause");
    }

    (*q).last_seen_used = (*q).last_seen_used + 1;

    return 0;
}
