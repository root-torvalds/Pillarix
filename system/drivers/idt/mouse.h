#pragma once

void mouse_wait_write();
void mouse_wait_read();
void mouse_write(uint8_t data);
uint8_t mouse_read();
void init_mouse();
void mouse_handler_c();
void draw_mouse (int x, int y, int a);
