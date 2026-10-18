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
    // 原实现是 r2d_set_scale(0.5) + 坐标×2 来得到半号字，但那会改动"全局渲染缩放"
    // （结尾复位成 1.0 会冲掉外部设过的缩放）。改用 *_scaled 只缩放本次文字、不动全局，
    // 位置按设计稿 480x320 比例化（x/y 为调用方传入的设计稿像素，如 8 / 60）。
    jtext_printf_scaled(r2d, font, text, (int)strlen(text), c,
                        x / 480.0f, y / 320.0f, 0.5f);
}

#endif