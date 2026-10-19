#ifndef CAMERA_FOLLOW_PASS_H
#define CAMERA_FOLLOW_PASS_H
#include "../components/global_state_component.h"
#include "../components/player_component.h"
#include "../components/transform_component.h"
#include "../protocol.h"

struct Context;

// 将摄像机跟随指定玩家，使玩家居中于屏幕。
// 视口半宽/半高由"实际逻辑分辨率"推导（不再写死 960x640 / 4.8x3.2 米）：
// 宽屏（手机横屏）逻辑宽度更大 → 横向多看世界，玩家依然居中。
static void camera_follow_pass(entt::registry& registry, Context* ctx, float /*dt*/) {
    auto& gs = registry.ctx().get<GlobalState>();
    if (gs.camera_follow_conv < 0) return;

    int lw = 960, lh = 640;
    if (ctx && ctx->r2d) {
        r2d_get_logical_presentation(ctx->r2d, &lw, &lh, NULL);
        if (lw <= 0 || lh <= 0) r2d_get_size(ctx->r2d, &lw, &lh);
    }
    const float half_w = (float)lw * 0.5f / (float)PIXELS_PER_METER;
    const float half_h = (float)lh * 0.5f / (float)PIXELS_PER_METER;

    registry.view<PlayerComponent, TransformComponent>().each(
        [&gs, half_w, half_h](auto /*e*/, PlayerComponent& pc, TransformComponent& t)
    {
        if (pc.conv == gs.camera_follow_conv) {
            // X：始终跟随 + 前视偏置（横版看前方）
            gs.camera_position.x = t.position.x - half_w + gs.camera_bias_x;
            // Y：side_view 自动锁走廊中线（首次跟随取玩家 Y）；否则 camera_lock_y 手动锁；否则随玩家上下
            if (gs.side_view) {
                if (gs.camera_lock_y < 0.0f) gs.camera_lock_y = t.position.y;  // 首次捕获走廊中线
                gs.camera_position.y = gs.camera_lock_y - half_h;
            } else {
                gs.camera_position.y = (gs.camera_lock_y >= 0.0f)
                        ? (gs.camera_lock_y - half_h)
                        : (t.position.y - half_h);
            }
        }
    });
}

#endif