#ifndef INTERPOLATE_PASS_H
#define INTERPOLATE_PASS_H
#include <joy/jmath.h>
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/transform_component.h"
#include "../components/id_component.h"
#include "../components/global_state_component.h"

struct Context;

// 插值：逻辑帧 20Hz、渲染帧 60Hz，渲染帧以"上一逻辑帧位置 → 当前逻辑帧位置"按时间比例插值，
// 把 20Hz 的步进补成平滑移动（玩家/敌人/相机一并受益，因为相机跟随的也是插值后的 transform）。
//
// 插值系数 render_alpha 由 GameScene::Update 写入 GlobalState（= 累加器余数 / 固定步长，0..1）。
// 用 上一帧→当前帧 的线性插值（恒定 1 逻辑帧延迟，无超调、无抖动）；若某实体刚生成/瞬移，
// 其 prev 已被初始化或显式重置为等于 curr，此时插值恒等于 curr，不会滑入。
static void interpolate_pass(entt::registry& registry, Context* ctx, float /*dt*/)
{
    float a = 0.0f;
    if (ctx->ecs.ctx().contains<GlobalState>())
        a = ctx->ecs.ctx().get<GlobalState>().render_alpha;
    if (a < 0.0f) a = 0.0f; else if (a > 1.0f) a = 1.0f;

    registry.view<IdComponent, LogicPositionComponent, LogicRectComponent, TransformComponent>().each(
        [&](IdComponent& /*id*/, LogicPositionComponent& lp, LogicRectComponent& rect, TransformComponent& t)
        {
            // 渲染坐标（碰撞盒中心）= 插值后的逻辑左上角 + 半宽高
            const float hw = fp_to_float(rect.width) * 0.5f;
            const float hh = fp_to_float(rect.height) * 0.5f;
            const float px = fp_to_float(lp.prev_x) + hw;
            const float py = fp_to_float(lp.prev_y) + hh;
            const float cx = fp_to_float(lp.x)       + hw;
            const float cy = fp_to_float(lp.y)       + hh;
            t.position.x = px + (cx - px) * a;
            t.position.y = py + (cy - py) * a;
        });
}

#endif
