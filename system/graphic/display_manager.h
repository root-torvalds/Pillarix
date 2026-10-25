#pragma once

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

int display_manager_init_hardware_cursor(void *cursor_rgba_buffer);
int display_manager_move_cursor(uint32_t x, uint32_t y);
int display_manager_update_region(uint32_t x, uint32_t y, uint32_t w, uint32_t h);

#ifdef __cplusplus
}
#endif

