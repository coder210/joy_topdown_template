#ifndef FRAME_DISPLAY_PASS_H
#define FRAME_DISPLAY_PASS_H

#include <stdio.h>
#include <string.h>
#include <joy/joy.h>

static void frame_display_pass(jr2d_p r2d, font_p font, int frame_id, int logic_fps,
        float x, float y, int enemy_count = -1, int entity_id = -1)
{
    char text[96];
    if (enemy_count >= 0)
        snprintf(text, sizeof(text), "Frame: %d  LogicFPS: %d  Enemies: %d  EID: %d",
                 frame_id, logic_fps, enemy_count, entity_id);
    else
        snprintf(text, sizeof(text), "Frame: %d  LogicFPS: %d  EID: %d",
                 frame_id, logic_fps, entity_id);
    jcolor_t c = { 60, 60, 70, 235 };
    // 透明调试 HUD：不铺底，用 1px 白色描边保证可读；字号由调用方传 font12。
    // 位置按设计稿 480x320 比例化（x/y 为调用方传入的设计稿像素）。
    jcolor_t shadow = { 255, 255, 255, 170 };
    jtext_printf_scaled(r2d, font, text, (int)strlen(text), shadow,
                        (x + 1.0f) / 480.0f, (y + 1.0f) / 320.0f, 1.0f);
    jtext_printf_scaled(r2d, font, text, (int)strlen(text), c,
                        x / 480.0f, y / 320.0f, 1.0f);
}

#endif