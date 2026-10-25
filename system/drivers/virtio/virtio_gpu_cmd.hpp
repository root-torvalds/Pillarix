#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

extern uint32_t *os_framebuffer;

int  virtio_gpu_setup_screen(void);
void virtio_gpu_redraw(void);
void virtio_gpu_redraw_rect(uint32_t x, uint32_t y, uint32_t width, uint32_t height);
void virtio_gpu_clean_cache(void *addr, uint32_t bytes);

#ifdef __cplusplus
}
#endif
