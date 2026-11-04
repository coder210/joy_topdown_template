#ifndef LOGICAL_PRESENTATION_H
#define LOGICAL_PRESENTATION_H

#include <joy/joy.h>
#include "config/display_config.h"

// 固定 3:2 设计分辨率（480x320 基准）。当前所有场景已统一使用自适应宽度
// （见 presentation_set_adaptive），此函数保留，供确需固定比例布局时使用。
static inline void presentation_set_design(jr2d_p r2d)
{
        if (!r2d) return;
        r2d_set_logical_presentation_mode(r2d, 480, 320,
                                          JOY_LOGICAL_PRESENTATION_LETTERBOX);
}

// 游戏场景专用：高度固定 DISPLAY_DESIGN_H（PPM 与之同倍率 → 垂直可视 6.4 米、
// 角色大小观感不变），宽度按窗口宽高比动态计算，再用 LETTERBOX。
// 逻辑宽高比与窗口一致时正好铺满 → 左右不再有黑边，宽屏相当于横向多看地图。
static inline void presentation_set_adaptive(jr2d_p r2d)
{
        if (!r2d) return;

        int ow = 0, oh = 0;
        r2d_get_size(r2d, &ow, &oh);
        if (ow <= 0 || oh <= 0) {   // 初始化/切后台瞬间兜底
                ow = DISPLAY_WINDOW_W;
                oh = DISPLAY_WINDOW_H;
        }

        const int design_h = DISPLAY_DESIGN_H;
        int lw = (int)((float)design_h * (float)ow / (float)oh + 0.5f);
        if (lw < DISPLAY_WINDOW_W / 2)        lw = DISPLAY_WINDOW_W / 2;        // 极端竖长窗口兜底
        if (lw > DISPLAY_WINDOW_W * 5 / 2)    lw = DISPLAY_WINDOW_W * 5 / 2;    // 极端超宽窗口兜底

        r2d_set_logical_presentation_mode(r2d, lw, design_h,
                                          JOY_LOGICAL_PRESENTATION_LETTERBOX);
}

#endif // LOGICAL_PRESENTATION_H
