#include "debug_layer.h"
#include <joy/joy.h>

DebugLayer::DebugLayer(Context* ctx) : UILayer(ctx) {}

DebugLayer::~DebugLayer() = default;

void DebugLayer::Render()
{
        // 旧版左上角 60x60 红块调试可视化会遮挡 F1 调试文字，已停用。
        // 保留 DebugLayer 以便以后放真正的调试可视化；需要时再单独接开关。
        (void)ctx_;
}

void DebugLayer::Destroy() {}

// 原 on_render：在节点 world position（0,0）画一个 60x60 方块，颜色由 zorder 决定；
// debug_layer 节点 zorder 默认 0 → 红色。此处保留该可视化行为。
void DebugLayer::drawDebug()
{
        if (!ctx_) return;
        jr2d_p r2d = ctx_->r2d;
        float x = 0.0f, y = 0.0f;
        jcolor_t c = { 255, 80, 80, 255 };
        jrect_t r = { x - 30, y - 30, 60, 60 };
        r2d_set_color(r2d, c.r, c.g, c.b, c.a);
        shape_draw_rectangle(r2d, "fill", r);
}
