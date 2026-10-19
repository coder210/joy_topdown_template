#ifndef INTERPOLATE_PASS_H
#define INTERPOLATE_PASS_H
#include <math.h>
#include <joy/jmath.h>
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/transform_component.h"
#include "../components/id_component.h"
#include "../components/global_state_component.h"
#include "../components/enemy_component.h"
#include "../components/action_state_component.h"
#include "../components/bullet_component.h"
#include "render_math.h"   // GD_PI 渲染数学常量（与 draw_enemy_pass 共用，保证朝向角一致）

struct Context;

// 渲染插值：逻辑帧 20Hz、渲染帧 60Hz，用"指数平滑"把 20Hz 逻辑位置补成 60Hz 平滑移动。
//
// 为什么不用 上一帧→当前帧 的线性插值：
//   线性插值只把 20Hz 的"步进"补成直线段，但**拐角（方向突变）它不圆**——敌人是 4 邻域 A* +
//   轴对齐移动，每格拐一个 90° 直角，线性插值把这些直角原样保留，多次拐弯就显出抖动/机械感。
//   指数平滑（低通）天然把拐角磨圆，步进也一并补平。它只写 TransformComponent（不参与帧同步
//   checksum，双端安全）；逻辑位置 LogicPositionComponent 始终是 20Hz 真值，确定性不受影响。
//
// 平滑目标 = 当前逻辑帧碰撞盒中心；时间常数 = fixed_timestep（= 1 个逻辑步长，约 50ms）。
// 首帧（interp_init=false）直接对齐，避免从 (0,0) 滑入；之后每帧 dt 缩放的 exp 平滑。
//
// 例外：箭/子弹（BulletComponent）不参与渲染平滑——平滑会给匀速弹道带来 速度×tau 的滞后
// （提前消失），外推又易越过墙。子弹改由 draw_bullet_pass 直接按 LogicPosition 渲染（与玩家分开）。
static void interpolate_pass(entt::registry& registry, Context* ctx, float dt)
{
    float tau = 1.0f / 20.0f;   // 逻辑步长（与 game_runtime(ctx).FIXED_TIMESTEP 对齐）
    if (ctx->ecs.ctx().contains<GlobalState>())
        tau = ctx->ecs.ctx().get<GlobalState>().fixed_timestep;
    if (tau <= 0.0f) tau = 1.0f / 20.0f;

    // dt 缩放的指数平滑系数（与帧率无关）：f = 1 - e^{-dt/tau}
    float f = (dt > 0.0f) ? (1.0f - expf(-dt / tau)) : 1.0f;
    if (f < 0.0f) f = 0.0f; else if (f > 1.0f) f = 1.0f;

    auto view = registry.view<IdComponent, LogicPositionComponent, LogicRectComponent, TransformComponent>();
    for (auto e : view) {
        // 子弹不参与平滑（与玩家渲染分开），由 draw_bullet_pass 直接按逻辑位置绘制
        if (registry.all_of<BulletComponent>(e)) continue;

        auto& lp   = view.get<LogicPositionComponent>(e);
        auto& rect = view.get<LogicRectComponent>(e);
        auto& t    = view.get<TransformComponent>(e);

        // 平滑目标 = 当前逻辑帧碰撞盒中心
        const float hw = fp_to_float(rect.width) * 0.5f;
        const float hh = fp_to_float(rect.height) * 0.5f;
        const float tx = fp_to_float(lp.x) + hw;
        const float ty = fp_to_float(lp.y) + hh;

        if (!t.interp_init) {
            t.position.x = tx; t.position.y = ty;
        } else {
            t.position.x += (tx - t.position.x) * f;
            t.position.y += (ty - t.position.y) * f;
        }

        // 敌人朝向：把 4 向离散朝向平滑成连续旋转，消掉拐弯时朝向线"啪"地跳变。
        // 只有敌人（带 EnemyComponent）才写 rotation；玩家渲染不依赖 rotation，保持原样。
        if (registry.all_of<EnemyComponent>(e) && registry.all_of<ActionStateComponent>(e)) {
            int dir_val = registry.get<ActionStateComponent>(e).dir;
            float target = -GD_PI * 0.5f;
            switch (dir_val) {
            case 0: target =  GD_PI * 0.5f; break;
            case 1: target =  GD_PI;        break;
            case 2: target =  0.0f;         break;
            case 3: target = -GD_PI * 0.5f; break;
            }
            if (!t.interp_init) {
                t.rotation = target;
            } else {
                // 最短弧插值（处理 ±π 环绕）
                float d = target - t.rotation;
                while (d >  GD_PI) d -= 2.0f * GD_PI;
                while (d < -GD_PI) d += 2.0f * GD_PI;
                t.rotation += d * f;
            }
        }
    }
    // 标记初始化放到最后统一置位：上面若有 enemy 分支在 interp_init=false 时也要先写好 rotation。
    view.each([&](IdComponent&, LogicPositionComponent&, LogicRectComponent&, TransformComponent& t) {
        t.interp_init = true;
    });
}

#endif
