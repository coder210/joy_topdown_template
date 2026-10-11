#ifndef CAMERA_FOLLOW_PASS_H
#define CAMERA_FOLLOW_PASS_H
#include "../components/global_state_component.h"
#include "../components/player_component.h"
#include "../components/transform_component.h"
#include "../protocol.h"

struct Context;

// 将摄像机跟随指定玩家，使玩家居中于屏幕
// 逻辑屏幕 480x320，PPM=50，偏移量 = (480/2/50, 320/2/50) = (4.8, 3.2)
static void camera_follow_pass(entt::registry& registry, Context* /*ctx*/, float /*dt*/) {
    auto& gs = registry.ctx().get<GlobalState>();
    if (gs.camera_follow_conv < 0) return;

    registry.view<PlayerComponent, TransformComponent>().each(
        [&gs](auto /*e*/, PlayerComponent& pc, TransformComponent& t)
    {
        if (pc.conv == gs.camera_follow_conv) {
            // X：始终跟随 + 前视偏置（横版看前方）
            gs.camera_position.x = t.position.x - 4.8f + gs.camera_bias_x;
            // Y：side_view 自动锁走廊中线（首次跟随取玩家 Y）；否则 camera_lock_y 手动锁；否则随玩家上下
            if (gs.side_view) {
                if (gs.camera_lock_y < 0.0f) gs.camera_lock_y = t.position.y;  // 首次捕获走廊中线
                gs.camera_position.y = gs.camera_lock_y - 3.2f;
            } else {
                gs.camera_position.y = (gs.camera_lock_y >= 0.0f)
                        ? (gs.camera_lock_y - 3.2f)
                        : (t.position.y - 3.2f);
            }
        }
    });
}

#endif