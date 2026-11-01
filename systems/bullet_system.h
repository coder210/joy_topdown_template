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

/* 命中敌人的处置方式（二选一，互斥）：
 *   true  = 停在接触点（不销毁）—— 观察用
 *   false = 命中即销毁 —— 正式行为
 *
 * 为什么必须互斥：二分逼近求的是"最后一个【不重叠】的位置"，即接触前的一瞬。
 * 如果把它和销毁串起来，箭会被移回"还没碰到"的位置再销毁，表现就是
 * "箭没碰到敌人就消失了"。销毁必须在"已经重叠"的位置直接发生，不做回退。 */
static constexpr bool BULLET_ENEMY_STOP = false;

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

        // 命中墙/边界：用"箭尖"(中心沿飞行方向前移半长)判定（碰撞盒对齐贴图）。
        // 【临时测试-测完务必还原】箭撞墙"被阻住"：回退本 tick 位移，箭停在墙前，
        // 既不穿透也不销毁，便于核对"箭框 vs 绿色墙框"的接触位置。
        // 这里刻意不清零 b.vx/b.vy —— 速度保留才能保证下一 tick 仍能算出正确的
        // 飞行方向（ddx/ddy），否则 travel=0 会让方向退化成默认的 +X，画出横躺的框。
        // 还原为：把 `|| hit_wall` 补回下面的边界条件（命中即销毁）。
        fp_t tipx = fp_add(cx, fp_mul(ddx, hlong));
        fp_t tipy = fp_add(cy, fp_mul(ddy, hlong));
        int gx = collision_fp_floor_to_int(tipx);
        int gy = collision_fp_floor_to_int(tipy);
        const bool hit_wall = map_is_collidable(map_tile_at(gx, gy));
        if (hit_wall) {
            // 整 tick 回退会让箭停在"距墙最多一个 tick 位移(0.3m/30px)"的位置，
            // 看着像"明明离墙还有一段距离就判定碰墙"。这里改为在 [上一位置, 当前位置]
            // 之间二分逼近，定位到"再往前一丝就进墙"的最远安全点。
            // 定点数 + 固定迭代次数 -> 帧同步两端结果一致。
            const fp_t sx = fp_sub(lpos.x, fp_mul(b.vx, fp_from_float(dt)));  // 本 tick 起点
            const fp_t sy = fp_sub(lpos.y, fp_mul(b.vy, fp_from_float(dt)));
            const fp_t mdx = fp_mul(b.vx, fp_from_float(dt));                 // 本 tick 位移
            const fp_t mdy = fp_mul(b.vy, fp_from_float(dt));
            const fp_t two = fp_from_float(2.0f);
            fp_t lo = fp_zero(), hi = fp_one();
            fp_t bestx = sx, besty = sy;                                      // 已知安全的最远点
            for (int it = 0; it < 10; it++) {
                const fp_t mid = fp_div(fp_add(lo, hi), two);
                const fp_t px  = fp_add(sx, fp_mul(mdx, mid));
                const fp_t py  = fp_add(sy, fp_mul(mdy, mid));
                const fp_t pcx = fp_add(px, fp_div(lrec.width,  two));
                const fp_t pcy = fp_add(py, fp_div(lrec.height, two));
                const fp_t ptx = fp_add(pcx, fp_mul(ddx, hlong));
                const fp_t pty = fp_add(pcy, fp_mul(ddy, hlong));
                const int pgx = collision_fp_floor_to_int(ptx);
                const int pgy = collision_fp_floor_to_int(pty);
                const bool blocked = (pgx < 0 || pgy < 0 || pgx >= MAP_W || pgy >= MAP_H)
                                     || map_is_collidable(map_tile_at(pgx, pgy));
                if (blocked) { hi = mid; }
                else { lo = mid; bestx = px; besty = py; }
            }
            lpos.x = bestx;
            lpos.y = besty;
            cx = fp_add(lpos.x, fp_div(lrec.width,  fp_from_float(2.0f)));
            cy = fp_add(lpos.y, fp_div(lrec.height, fp_from_float(2.0f)));
        }
        if (gx < 0 || gy < 0 || gx >= MAP_W || gy >= MAP_H) {
            to_destroy.push_back(e);
            continue;
        }

        // [TEST] 去掉扫掠：只在"当前落点"用贴图大小的旋转盒做 SAT（观察真实移动碰撞）。
        vec2f_t swept[8];
        int swept_n = 0;
        vec2f_t bcenter = { cx, cy };
        vec2f_t baxis   = { ddx, ddy };
        collision2df_swept_obb(bcenter, baxis, hlong, hshort, fp_zero(), fp_zero(), swept, &swept_n);
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
                // 【临时测试-测完务必还原】箭击退已注释：命中时本会给敌人施加
                // KnockbackComponent 把它推走，干扰"箭停在接触点"的观察。测完解除注释。
                // 击退：仅对敌人施加（避免玩家被推导致联机不同步），沿子弹速度方向
                // fp_t len = fp_sqrt(fp_add(fp_mul(b.vx, b.vx), fp_mul(b.vy, b.vy)));
                // if (len > 0) {
                //     fp_t ks = fp_from_float(KNOCKBACK_SPEED);
                //     auto& kb = registry.emplace_or_replace<KnockbackComponent>(ee);
                //     kb.vx = fp_div(fp_mul(b.vx, ks), len);
                //     kb.vy = fp_div(fp_mul(b.vy, ks), len);
                //     kb.ticks = KNOCKBACK_TICKS;
                // }

                // 二分逼近：先求出"最后一个不重叠的位置" = 精确接触点。
                // 停住和销毁【都】需要它，因为渲染发生在逻辑之后：
                //   若在本 tick 直接 destroy，渲染时实体已不存在，玩家看到的最后一帧是
                //   上一 tick 的位置（距目标最多一个 tick 位移 0.3m ≈ 30px），
                //   表现就是"箭离敌人还有一段距离就没了"。
                {
                    const fp_t esx = fp_sub(lpos.x, fp_mul(b.vx, fp_from_float(dt))); // 本 tick 起点
                    const fp_t esy = fp_sub(lpos.y, fp_mul(b.vy, fp_from_float(dt)));
                    const fp_t edx = fp_mul(b.vx, fp_from_float(dt));                 // 本 tick 位移
                    const fp_t edy = fp_mul(b.vy, fp_from_float(dt));
                    const fp_t two = fp_from_float(2.0f);
                    fp_t lo = fp_zero(), hi = fp_one();
                    fp_t bestx = esx, besty = esy;                                    // 已知不重叠的最远点
                    for (int it = 0; it < 10; it++) {
                        const fp_t mid = fp_div(fp_add(lo, hi), two);
                        const fp_t px  = fp_add(esx, fp_mul(edx, mid));
                        const fp_t py  = fp_add(esy, fp_mul(edy, mid));
                        vec2f_t pc = { fp_add(px, fp_div(lrec.width,  two)),
                                       fp_add(py, fp_div(lrec.height, two)) };
                        vec2f_t pv[8];
                        int pv_n = 0;
                        collision2df_swept_obb(pc, baxis, hlong, hshort,
                                               fp_zero(), fp_zero(), pv, &pv_n);
                        polygonf_t probe = { pv, pv_n };
                        if (collision2df_check_polygons(probe, enemy_poly)) { hi = mid; }
                        else { lo = mid; bestx = px; besty = py; }
                    }
                    lpos.x = bestx;
                    lpos.y = besty;
                }
                if (!BULLET_ENEMY_STOP) {
                    // 【销毁】延迟 1 tick 再销毁：本 tick 让箭渲染在"接触点"上（紧贴敌人），
                    // 下一 tick 走寿命分支真正销毁。这样看到的是"碰到即消失"，
                    // 而不是在上一 tick 的位置凭空消失。
                    // 复用 life_ticks 而非新增字段 -> BulletComponent 快照结构不变，帧同步安全。
                    b.life_ticks = 1;
                }
                break;  // 命中一个目标即可，跳出敌人循环
            }
        }
    }

    for (size_t i = 0; i < to_destroy.size(); ++i)
        registry.destroy(to_destroy[i]);
}

#endif
