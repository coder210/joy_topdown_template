#ifndef BULLET_SYSTEM_H
#define BULLET_SYSTEM_H

#include <vector>
#include "../components/bullet_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/id_component.h"
#include "../components/enemy_component.h"
#include "../components/health_component.h"
#include "../components/knockback_component.h"
#include "../systems/collision_system.h"   // collision_fp_floor_to_int
#include "../systems/map_def.h"            // MAP_W/H, map_tile_at, map_is_collidable
#include <joy/jmath.h>

struct Context;

// 子弹推进系统：每 tick 移动 + 命中敌人扣血 + 寿命/越界/撞墙销毁。
// 与玩家/敌人运动解耦（子弹不带 LogicVelocityComponent，不受 movement/collision 系统处理），
// 全程定点数 -> 帧同步确定性。
static void bullet_system(entt::registry& registry, Context* ctx, float dt)
{
    auto bullets = registry.view<BulletComponent, LogicPositionComponent,
                                  LogicRectComponent, IdComponent>();

    // 先收集本 tick 需销毁的子弹，遍历结束后再删（避免修改 view 迭代器）
    std::vector<entt::entity> to_destroy;

    for (auto e : bullets) {
        auto& b    = bullets.get<BulletComponent>(e);
        auto& lpos = bullets.get<LogicPositionComponent>(e);
        auto& lrec = bullets.get<LogicRectComponent>(e);

        // 推进（帧同步使用固定 dt；速度已为定点数）
        lpos.x = fp_add(lpos.x, fp_mul(b.vx, fp_from_float(dt)));
        lpos.y = fp_add(lpos.y, fp_mul(b.vy, fp_from_float(dt)));

        // 寿命
        b.life_ticks--;
        if (b.life_ticks <= 0) {
            to_destroy.push_back(e);
            continue;
        }

        // 子弹中心
        fp_t cx = fp_add(lpos.x, fp_div(lrec.width,  fp_from_float(2.0f)));
        fp_t cy = fp_add(lpos.y, fp_div(lrec.height, fp_from_float(2.0f)));

        // 越界或撞墙 -> 销毁
        int gx = collision_fp_floor_to_int(cx);
        int gy = collision_fp_floor_to_int(cy);
        if (gx < 0 || gy < 0 || gx >= MAP_W || gy >= MAP_H ||
            map_is_collidable(map_tile_at(gx, gy))) {
            to_destroy.push_back(e);
            continue;
        }

        // 命中敌人（AABB 相交，复用 melee_attack 的判定式）
        auto enemies = registry.view<EnemyComponent, LogicPositionComponent,
                                      LogicRectComponent, HealthComponent>();
        for (auto ee : enemies) {
            auto& ep   = enemies.get<LogicPositionComponent>(ee);
            auto& erec = enemies.get<LogicRectComponent>(ee);
            auto& hp   = enemies.get<HealthComponent>(ee);
            if (hp.hp <= 0) continue;

            fp_t hx = lpos.x, hy = lpos.y, hw = lrec.width, hh = lrec.height;
            fp_t tx = ep.x,  ty = ep.y,  tw = erec.width, th = erec.height;
            bool overlap = (hx < tx + tw && hx + hw > tx &&
                            hy < ty + th && hy + hh > ty);
            if (overlap) {
                hp.hp -= b.damage;
                if (hp.hp < 0) hp.hp = 0;
                // 击退：仅对敌人施加（避免玩家被推导致联机不同步），沿子弹速度方向
                fp_t len = fp_sqrt(fp_add(fp_mul(b.vx, b.vx), fp_mul(b.vy, b.vy)));
                if (len > 0) {
                    fp_t ks = fp_from_float(KNOCKBACK_SPEED);
                    auto& kb = registry.emplace_or_replace<KnockbackComponent>(ee);
                    kb.vx = fp_div(fp_mul(b.vx, ks), len);
                    kb.vy = fp_div(fp_mul(b.vy, ks), len);
                    kb.ticks = KNOCKBACK_TICKS;
                }
                to_destroy.push_back(e);
                break;  // 命中一个目标即可，跳出敌人循环
            }
        }
    }

    for (size_t i = 0; i < to_destroy.size(); ++i)
        registry.destroy(to_destroy[i]);
}

#endif
