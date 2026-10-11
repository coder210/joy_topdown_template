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
    r2d_set_scale(r2d, 0.5f, 0.5f);
    jtext_printx(r2d, font, text, (int)strlen(text), c, x * 2.0f, y * 2.0f);
    r2d_set_scale(r2d, 1.0f, 1.0f);
}

#endif