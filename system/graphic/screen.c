#include "../kernel/kernel/kernel.h"

UINT32* real_framebuffer = 0;
EFI_GRAPHICS_OUTPUT_BLT_PIXEL* virtual_framebuffer = 0;

#ifdef __cplusplus
extern "C"
#endif
extern uint32_t *os_framebuffer;

EFIAPI void draw_taskbar(UINT32 x, UINT32 y, UINT32 w, UINT32 h, UINT32 rad, UINT8 r, UINT8 g, UINT8 b, UINT8 a) {
    if (w == 0 || h == 0 || a == 0) return;

    if (rad * 2 > w) rad = w / 2;
    if (rad * 2 > h) rad = h / 2;

    if (rad == 0) {
        drawRect(x, y, w, h, r, g, b, a);
        return;
    }

    drawRect(x + rad, y, w - (rad * 2), h, r, g, b, a);
    drawRect(x, y + rad, rad, h - (rad * 2), r, g, b, a);
    drawRect((x + w) - rad, y + rad, rad, h - (rad * 2), r, g, b, a);

    UINT32 rad_scaled = rad * 256;
    UINT32 rad_sq_scaled = rad_scaled * rad_scaled;
    UINT32 inner_edge = rad_scaled - 256;

    struct Corner {
        UINT32 cx;
        UINT32 cy;
        int dx_sign;
        int dy_sign;
    } corners[4] = {
        { x + rad - 1,     y + rad - 1,     -1, -1 },
        { x + w - rad,     y + rad - 1,      1, -1 },
        { x + rad - 1,     y + h - rad,     -1,  1 },
        { x + w - rad,     y + h - rad,      1,  1 }
    };

    for (int i = 0; i < 4; i++) {
        UINT32 cx = corners[i].cx;
        UINT32 cy = corners[i].cy;

        for (UINT32 h_coord = 0; h_coord < rad; h_coord++) {
            UINT32 dy = h_coord;
            UINT32 dy_scaled = dy * 256;
            UINT32 dy_sq = dy_scaled * dy_scaled;

            UINT32 screen_y = (corners[i].dy_sign == -1) ? (cy - dy) : (cy + dy);

            for (UINT32 w_coord = 0; w_coord < rad; w_coord++) {
                UINT32 dx = w_coord;
                UINT32 dx_scaled = dx * 256;
                UINT32 dist_sq = (dx_scaled * dx_scaled) + dy_sq;

                if (dist_sq > rad_sq_scaled) continue;

                UINT32 screen_x = (corners[i].dx_sign == -1) ? (cx - dx) : (cx + dx);

                UINT32 dist = 0;
                while ((dist + 1) * (dist + 1) <= dist_sq) { dist++; }

                if (dist <= inner_edge) {
                    drawPoint(screen_x, screen_y, r, g, b, a);
                } else {
                    UINT32 edge_dist = dist - inner_edge;
                    UINT32 alpha_factor = 256 - edge_dist;
                    UINT32 final_alpha = (a * alpha_factor) >> 8;

                    drawPoint(screen_x, screen_y, r, g, b, (UINT8)final_alpha);
                }
            }
        }
    }
}
