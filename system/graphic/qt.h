#pragma once
#include <stdint.h>
#include "../kernel/kernel/kernel.h"

#ifdef __cplusplus
extern "C" {
    #endif

    EFIAPI void swapBuffers(EFI_GRAPHICS_OUTPUT_PROTOCOL *gop);
    void initScreenDriver(BootInfo* info);
    void screenSwitchToVirtio(void);
    EFIAPI void drawPoint(uint32_t x, uint32_t y, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void drawLine(int x0, int y0, int x1, int y1, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    EFIAPI void drawRect(uint32_t x, uint32_t y, uint32_t w, uint32_t h, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    EFIAPI void fillScreen(uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    void drawEllipse(int x, int y, int width, int height, uint8_t r, uint8_t g, uint8_t b, uint8_t a);
    static uint32_t isqrt64(uint64_t n);
    #ifdef __cplusplus
}
#endif
