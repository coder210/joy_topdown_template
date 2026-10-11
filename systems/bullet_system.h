#ifndef BULLET_SYSTEM_H
#define BULLET_SYSTEM_H

#include "../components/bullet_component.h"
#include "../components/health_component.h"
#include "../components/player_component.h"
#include "../components/enemy_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "./map_def.h"
#include <vector>

struct Context;

// 定点数向下取整转 int（算术右移即 floor，负向负无穷），纯整数、帧同步确定性。
static inline int bullet_fp_floor_to_int(fp_t x)
{
    return (int)(x >> FP_FRACTIONAL_PLACES);
}
// 定点数向零截断转 int（等价于 (int)(x / 2^32) 的 C 语义），纯整数。
static inline int bullet_fp_trunc_to_int(fp_t x)
{
    int64_t q = x >> FP_FRACTIONAL_PLACES;
    if (x < 0 && (x & (FP_ONE - 1)) != 0) q += 1;
    return (int)q;
}

// 子弹推进系统：移动 / 地图碰撞 / 玩家命中 / 生命周期
// 两端确定性运行：逻辑只依赖相同输入，无需网络同步。
static void bullet_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    auto bullet_view = registry.view<BulletComponent, LogicPositionComponent>();
    auto player_view = registry.view<PlayerComponent, LogicPositionComponent,
                                     LogicRectComponent, HealthComponent>();
    auto enemy_view = registry.view<EnemyComponent, LogicPositionComponent,
                                    LogicRectComponent, HealthComponent>();

    std::vector<entt::entity> to_destroy;

    for (auto e : bullet_view) {
        auto& b   = bullet_view.get<BulletComponent>(e);
        auto& pos = bullet_view.get<LogicPositionComponent>(e);

        // 1) 移动（LogicPosition 为碰撞盒左上角，速度 * 固定步长）
        pos.x = fp_add(pos.x, fp_mul(b.vx, fp_from_float(game_runtime(ctx).FIXED_TIMESTEP)));
        pos.y = fp_add(pos.y, fp_mul(b.vy, fp_from_float(game_runtime(ctx).FIXED_TIMESTEP)));

        // 2) 生命周期（整数 tick 倒计时，确定性优于 float 秒累加）
        b.life_ticks--;
        if (b.life_ticks <= 0) { to_destroy.push_back(e); continue; }

        // 3) 地图碰撞：检测子弹中心所在格（纯定点数，帧同步确定性）
        const fp_t cxf = fp_add(pos.x, fp_from_float(BULLET_HALF));
        const fp_t cyf = fp_add(pos.y, fp_from_float(BULLET_HALF));
        if (map_is_collidable(map_tile_at(bullet_fp_floor_to_int(cxf), bullet_fp_floor_to_int(cyf)))) {
            to_destroy.push_back(e);
            continue;
        }

        // 4) 命中判定：敌人子弹（owner=0）命中玩家；玩家子弹（owner>0）命中敌人。无友伤。
        //    AABB 相交（子弹盒 vs 目标盒，均为左上角 + 宽高），纯定点数
        const fp_t bx = pos.x, by = pos.y;
        const fp_t bhalf = fp_from_float(BULLET_SIZE);
        const int dmg = bullet_fp_trunc_to_int(b.damage);
        if (b.owner_conv == 0) {
            for (auto p : player_view) {
                auto& ppos  = player_view.get<LogicPositionComponent>(p);
                auto& prect = player_view.get<LogicRectComponent>(p);
                const fp_t px = ppos.x, py = ppos.y;
                const fp_t pw = prect.width, ph = prect.height;
                if (bx < fp_add(px, pw) && fp_add(bx, bhalf) > px &&
                    by < fp_add(py, ph) && fp_add(by, bhalf) > py) {
                    auto& h = player_view.get<HealthComponent>(p);
                    if (h.dead) continue;           // 已死亡玩家不结算子弹伤害
                    if (h.spawn_invuln > 0) continue; // 重生无敌期间免疫伤害
                    h.hp -= dmg;
                    if (h.hp < 0) h.hp = 0;
                    to_destroy.push_back(e);
                    break;
                }
            }
        } else {
            // 玩家子弹 → 敌人（跳过已死亡敌人：尸体不受二次伤害）
            for (auto en : enemy_view) {
                auto& epos  = enemy_view.get<LogicPositionComponent>(en);
                auto& erect = enemy_view.get<LogicRectComponent>(en);
                const fp_t ex = epos.x, ey = epos.y;
                const fp_t ew = erect.width, eh = erect.height;
                if (bx < fp_add(ex, ew) && fp_add(bx, bhalf) > ex &&
                    by < fp_add(ey, eh) && fp_add(by, bhalf) > ey) {
                    auto& h = enemy_view.get<HealthComponent>(en);
                    if (h.dead) continue;   // 已死亡敌人不结算子弹伤害
                    h.hp -= dmg;
                    if (h.hp < 0) h.hp = 0;
                    to_destroy.push_back(e);
                    break;
                }
            }
        }
    }

    for (auto e : to_destroy) registry.destroy(e);
}

#endif
