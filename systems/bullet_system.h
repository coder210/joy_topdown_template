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

        // 推进前的中心：与推进后的中心一起构成"扫掠段"，用于扫掠判定
        fp_t prev_cx = fp_add(lpos.x, fp_div(lrec.width,  fp_from_float(2.0f)));
        fp_t prev_cy = fp_add(lpos.y, fp_div(lrec.height, fp_from_float(2.0f)));

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

        // 运动方向单位向量 + 本 tick 位移
        fp_t mvx = fp_sub(cx, prev_cx), mvy = fp_sub(cy, prev_cy);
        fp_t travel = fp_sqrt(fp_add(fp_mul(mvx, mvx), fp_mul(mvy, mvy)));
        fp_t ddx = fp_one(), ddy = fp_zero();
        if (travel > 0) { ddx = fp_div(mvx, travel); ddy = fp_div(mvy, travel); }

        // 箭碰撞盒半长(沿飞行) / 半宽(垂直)：与贴图实际可见尺寸一致(0.6875 × 0.156)
        fp_t hlong  = fp_div(fp_from_float(BULLET_HIT_LEN), fp_from_float(2.0f));
        fp_t hshort = fp_div(fp_from_float(BULLET_HIT_WID), fp_from_float(2.0f));

        // 命中墙/边界：用"箭尖"(中心沿飞行方向前移半长)判定，箭尖到墙即销毁（碰撞盒对齐贴图）。
        fp_t tipx = fp_add(cx, fp_mul(ddx, hlong));
        fp_t tipy = fp_add(cy, fp_mul(ddy, hlong));
        int gx = collision_fp_floor_to_int(tipx);
        int gy = collision_fp_floor_to_int(tipy);
        if (gx < 0 || gy < 0 || gx >= MAP_W || gy >= MAP_H ||
            map_is_collidable(map_tile_at(gx, gy))) {
            to_destroy.push_back(e);
            continue;
        }

        // 命中敌人：把箭（贴图大小的旋转矩形）沿速度拉成覆盖整段轨迹的扫掠体，做 SAT。
        vec2f_t swept[8];
        int swept_n = 0;
        vec2f_t bcenter = { prev_cx, prev_cy };
        vec2f_t baxis   = { ddx, ddy };
        collision2df_swept_obb(bcenter, baxis, hlong, hshort, mvx, mvy, swept, &swept_n);
        polygonf_t arrow_poly = { swept, swept_n };

        auto enemies = registry.view<EnemyComponent, LogicPositionComponent,
                                      LogicRectComponent, HealthComponent>();
        for (auto ee : enemies) {
            auto& ep   = enemies.get<LogicPositionComponent>(ee);
            auto& erec = enemies.get<LogicRectComponent>(ee);
            auto& hp   = enemies.get<HealthComponent>(ee);
            if (hp.hp <= 0) continue;

            fp_t ecx = fp_add(ep.x, fp_div(erec.width,  fp_from_float(2.0f)));
            fp_t ecy = fp_add(ep.y, fp_div(erec.height, fp_from_float(2.0f)));
            fp_t ehx = fp_div(erec.width,  fp_from_float(2.0f));
            fp_t ehy = fp_div(erec.height, fp_from_float(2.0f));

            // 敌人 AABB 4 顶点：屏幕顺时针 TL -> TR -> BR -> BL，
            // 与 vec2f_normal 的外法线约定一致（绕序反了 SAT 结果会完全相反）
            vec2f_t ev[4] = {
                { fp_sub(ecx, ehx), fp_sub(ecy, ehy) },
                { fp_add(ecx, ehx), fp_sub(ecy, ehy) },
                { fp_add(ecx, ehx), fp_add(ecy, ehy) },
                { fp_sub(ecx, ehx), fp_add(ecy, ehy) },
            };
            polygonf_t enemy_poly = { ev, 4 };

            bool overlap = collision2df_check_polygons(arrow_poly, enemy_poly);
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
