#ifndef COLLISION_SYSTEM_H
#define COLLISION_SYSTEM_H
#include "../components/enemy_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/player_component.h"
#include "../components/bullet_component.h"
#include "map_def.h"

struct Context;

// 定点数向下取整转 int（算术右移即 floor，负向负无穷），纯整数、帧同步确定性。
static inline int collision_fp_floor_to_int(fp_t x)
{
    return (int)(x >> FP_FRACTIONAL_PLACES);
}

// 玩家/敌人与地图碰撞处理 system。
// 在 OnUpdate 阶段运行，在 movement_system 之后。
//
// 性能优化（修复 LogicFPS 过低）：
// 原实现遍历"所有带 LogicRect 的实体"作为碰撞体，而 map_spawn_system 为每格障碍
// 创建了实体，地图障碍格有数百个 → 每 tick 移动体(玩家+敌人+子弹) × 数百障碍 =
// 上万次矩形碰撞判定，单 tick 耗时 100~220ms，导致 LogicFPS 跌到 ~6。
//
// 地图碰撞体位置即格子坐标，直接用 MAP_LAYOUT 网格判断即可（O(1) 查表），
// 无需遍历数百个 tile 实体。改为网格碰撞：检测移动体 AABB 覆盖到的格子是否不可通行。
static void collision_system(entt::registry& registry, Context* ctx, float dt) {
    (void)ctx;
    (void)dt;

    // 移动体视图：所有玩家/敌人（同时需有 LogicRectComponent + LogicPositionComponent）
    auto mover_view = registry.view<LogicRectComponent, LogicPositionComponent>();

    for (auto mover_entity : mover_view) {
        // 只对玩家/敌人做碰撞修正，静态碰撞体本身不参与
        if (!registry.all_of<PlayerComponent>(mover_entity) &&
            !registry.all_of<EnemyComponent>(mover_entity)) continue;

        auto& mover_rect = mover_view.get<LogicRectComponent>(mover_entity);
        auto& mover_pos = mover_view.get<LogicPositionComponent>(mover_entity);

        // 移动体 AABB 边界（纯定点数，帧同步确定性）
        const fp_t x0 = mover_pos.x;
        const fp_t y0 = mover_pos.y;
        const fp_t w  = mover_rect.width;
        const fp_t h  = mover_rect.height;
        const fp_t x1 = fp_add(x0, w);
        const fp_t y1 = fp_add(y0, h);
        const fp_t EPS = fp_from_float(0.001f);

        // 计算移动体覆盖的格子范围（含边缘），越界视为墙
        int gx0 = collision_fp_floor_to_int(x0);
        int gx1 = collision_fp_floor_to_int(fp_sub(x1, EPS));
        int gy0 = collision_fp_floor_to_int(y0);
        int gy1 = collision_fp_floor_to_int(fp_sub(y1, EPS));

        // 对覆盖的每个格子做碰撞推挤（沿最小渗透方向）
        for (int gy = gy0; gy <= gy1; gy++) {
            for (int gx = gx0; gx <= gx1; gx++) {
                // 格子可通行则跳过；越界视为墙
                bool blocked;
                if (gx < 0 || gy < 0 || gx >= MAP_W || gy >= MAP_H)
                    blocked = true;
                else
                    blocked = map_is_collidable(map_tile_at(gx, gy));
                if (!blocked) continue;

                // 墙格矩形 [gx, gy]~[gx+1, gy+1]，与移动体 AABB 求最小渗透方向并推出
                rectanglef_t mover_box = { x0, y0, w, h };
                rectanglef_t tile_box  = { fp_from_int(gx), fp_from_int(gy),
                                           fp_one(), fp_one() };
                contact2df_t contact;
                if (collision2df_get_rectangles(mover_box, fp_zero(), tile_box, fp_zero(), &contact)) {
                    fp_t depth = contact.depth;
                    if (depth < fp_zero()) depth = fp_sub(fp_zero(), depth);
                    fp_t nx = fp_sub(fp_zero(), contact.normal.x);
                    fp_t ny = fp_sub(fp_zero(), contact.normal.y);
                    mover_pos.x = fp_add(mover_pos.x, fp_mul(nx, depth));
                    mover_pos.y = fp_add(mover_pos.y, fp_mul(ny, depth));
                    // 推出后更新边界与格子范围，继续检查下一格
                    gx0 = collision_fp_floor_to_int(mover_pos.x);
                    gx1 = collision_fp_floor_to_int(fp_sub(fp_add(mover_pos.x, w), EPS));
                    gy0 = collision_fp_floor_to_int(mover_pos.y);
                    gy1 = collision_fp_floor_to_int(fp_sub(fp_add(mover_pos.y, h), EPS));
                }
            }
        }
    }
}

#endif
