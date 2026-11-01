#ifndef LOGICAL_PRESENTATION_H
#define LOGICAL_PRESENTATION_H

#include <joy/joy.h>

// 固定 3:2 设计分辨率（960x640）。当前所有场景已统一使用自适应宽度
// （见 presentation_set_adaptive），此函数保留，供确需固定比例布局时使用。
static inline void presentation_set_design(jr2d_p r2d)
{
        if (!r2d) return;
        r2d_set_logical_presentation_mode(r2d, 960, 640,
                                          JOY_LOGICAL_PRESENTATION_LETTERBOX);
}

// 游戏场景专用：高度固定 640（PPM=100 → 垂直可视 6.4 米、角色大小观感不变），
// 宽度按窗口宽高比动态计算（lw = 640 * 输出宽 / 输出高），再用 LETTERBOX。
// 逻辑宽高比与窗口一致时正好铺满 → 左右不再有黑边，宽屏相当于横向多看地图。
static inline void presentation_set_adaptive(jr2d_p r2d)
{
        if (!r2d) return;

        int ow = 0, oh = 0;
        r2d_get_size(r2d, &ow, &oh);
        if (ow <= 0 || oh <= 0) { ow = 960; oh = 640; }   // 初始化/切后台瞬间兜底

        const int design_h = 640;
        int lw = (int)((float)design_h * (float)ow / (float)oh + 0.5f);
        if (lw < 480)  lw = 480;    // 极端竖长窗口兜底
        if (lw > 2400) lw = 2400;   // 极端超宽窗口兜底

        r2d_set_logical_presentation_mode(r2d, lw, design_h,
                                          JOY_LOGICAL_PRESENTATION_LETTERBOX);
}

#endif // LOGICAL_PRESENTATION_H
