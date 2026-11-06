EFIAPI EFI_GRAPHICS_OUTPUT_PROTOCOL* init_gop(EFI_SYSTEM_TABLE *SystemTable);
void init_screen_driver(BootInfo* info);
EFIAPI void draw_taskbar(UINT32 x1, UINT32 y1, UINT32 x2, UINT32 y2, UINT32 rad, UINT8 r, UINT8 g, UINT8 b, UINT8 a);
