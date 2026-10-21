#ifndef KNOCKBACK_SYSTEM_H
#define KNOCKBACK_SYSTEM_H
#include "../components/knockback_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../context.h"          // game_runtime(ctx)
#include "movement_system.h"     // 复用 movement_box_blocked / movement_snap_axis 做撞墙即停

// 击退位移系统：注册在 movement/pathfinding/近战/子弹之后。对带 KnockbackComponent
// 的实体施加一次位移（有碰撞盒则逐轴撞墙即停），并每 tick 衰减，ticks 耗尽即移除组件。
// 放在 bullets 之后，保证命中当 tick 即生效；在 cleanup 之前、snapshot(PostUpdate) 之前，
// 因此击退位移会进入快照被联机客户端同步。
static void knockback_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    auto view = registry.view<KnockbackComponent, LogicPositionComponent>();
    const fp_t tick  = fp_from_float(game_runtime(ctx).FIXED_TIMESTEP);
    const fp_t decay = fp_from_float(KNOCKBACK_DECAY);
    for (auto e : view) {
        auto& kb = view.get<KnockbackComponent>(e);
        auto& p  = view.get<LogicPositionComponent>(e);
        fp_t dx = fp_mul(kb.vx, tick);
        fp_t dy = fp_mul(kb.vy, tick);

        // 有碰撞盒：逐轴位移，撞墙即停（与 movement_system 同款 4 参数盒查表）；否则直接位移
        if (registry.all_of<LogicRectComponent>(e)) {
            const fp_t w = registry.get<LogicRectComponent>(e).width;
            const fp_t h = registry.get<LogicRectComponent>(e).height;
            // X 轴
            if (dx != 0) {
                fp_t cand = fp_add(p.x, dx);
                int dir = (dx < 0) ? -1 : 1;
                if (!movement_box_blocked(cand, p.y, w, h)) p.x = cand;
                else p.x = movement_snap_axis(p.x, w, cand, dir);
            }
            // Y 轴
            if (dy != 0) {
                fp_t cand = fp_add(p.y, dy);
                int dir = (dy < 0) ? -1 : 1;
                if (!movement_box_blocked(p.x, cand, w, h)) p.y = cand;
                else p.y = movement_snap_axis(p.y, h, cand, dir);
            }
        } else {
            p.x = fp_add(p.x, dx);
            p.y = fp_add(p.y, dy);
        }

        kb.ticks--;
        kb.vx = fp_mul(kb.vx, decay);
        kb.vy = fp_mul(kb.vy, decay);
        if (kb.ticks <= 0) registry.remove<KnockbackComponent>(e);
    }
}
#endif
