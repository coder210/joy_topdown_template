#pragma once
#include <stdio.h>
#include <string.h>
#include <joy/joy.h>

// ============================================================
// FPS 显示（纯视觉 HUD，不影响帧同步/ECS 逻辑）
//   - 在逻辑屏幕坐标左上角绘制实时帧率
//   - 内部用指数移动平均平滑帧率，每 0.5s 刷新一次文本
//   - 需在 r2d_begin/r2d_end 之间调用
// ============================================================
static void draw_fps_pass(jr2d_p r2d, font_p font, float dt)
{
        static float ema      = 60.0f;
        static float accum    = 0.0f;
        static char  text[32] = "FPS: --";

        float instant = dt > 1e-5f ? 1.0f / dt : 0.0f;
        ema = ema * 0.9f + instant * 0.1f;
        accum += dt;
        if (accum >= 0.5f) {
                snprintf(text, sizeof(text), "FPS: %.0f", ema);
                accum = 0.0f;
        }

        jcolor_t c = { 60, 60, 70, 235 };   // 深灰，适配白底背景
        jcolor_t shadow = { 255, 255, 255, 170 };
        jtext_printf(r2d, font, text, (int)strlen(text), shadow,
                     9.0f / 480.0f, 5.0f / 320.0f);
        jtext_printf(r2d, font, text, (int)strlen(text), c,
                     8.0f / 480.0f, 4.0f / 320.0f);
}