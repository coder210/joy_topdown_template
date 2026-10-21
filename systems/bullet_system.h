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

/* OBB 扫掠相交测试，相交判定用 joy 内置的 SAT（collision2df_check_polygons）。
 *
 * A = 箭的扫掠旋转盒：中心 (cax,cay)，单位长轴 (aux,auy)，半长 ahl（已含扫掠拉伸），半宽 ahs
 * B = 敌人盒：中心 (cbx,cby)，轴对齐，半宽 bhx / 半高 bhy
 *
 * joy 只有瓦片地图专用的 joy_tilemap_sweep_x/y，没有通用"扫掠"接口，
 * 所以扫掠体（沿运动方向拉长的盒子）仍由自己构造；但拉长之后的凸多边形
 * 相交直接用 joy 的 SAT，不必再自己维护一份分离轴实现。
 *
 * 传的是顶点而不是角度，因此不需要 atan2/sin/cos —— 全程定点数，
 * 保持帧同步确定性。 */
static bool bullet_swept_obb_hit(
        fp_t cax, fp_t cay, fp_t aux, fp_t auy, fp_t ahl, fp_t ahs,
        fp_t cbx, fp_t cby, fp_t bhx, fp_t bhy)
{
        // A 的两条半轴分量：长轴 (aux,auy)*ahl，短轴 (-auy,aux)*ahs
        fp_t mx = fp_mul(aux, ahl), my = fp_mul(auy, ahl);
        fp_t nx = fp_mul(fp_sub(fp_zero(), auy), ahs), ny = fp_mul(aux, ahs);

        vec2f_t av[4];   // A：扫掠旋转盒 4 顶点
        av[0].x = fp_add(fp_add(cax, mx), nx);  av[0].y = fp_add(fp_add(cay, my), ny);
        av[1].x = fp_add(fp_sub(cax, mx), nx);  av[1].y = fp_add(fp_sub(cay, my), ny);
        av[2].x = fp_sub(fp_sub(cax, mx), nx);  av[2].y = fp_sub(fp_sub(cay, my), ny);
        av[3].x = fp_sub(fp_add(cax, mx), nx);  av[3].y = fp_sub(fp_add(cay, my), ny);

        vec2f_t ev[4];   // B：敌人 AABB 4 顶点
        ev[0].x = fp_sub(cbx, bhx);  ev[0].y = fp_sub(cby, bhy);
        ev[1].x = fp_add(cbx, bhx);  ev[1].y = fp_sub(cby, bhy);
        ev[2].x = fp_add(cbx, bhx);  ev[2].y = fp_add(cby, bhy);
        ev[3].x = fp_sub(cbx, bhx);  ev[3].y = fp_add(cby, bhy);

        polygonf_t pa = { av, 4 };
        polygonf_t pb = { ev, 4 };
        return collision2df_check_polygons(pa, pb);
}

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

        // 越界或撞墙 -> 销毁
        int gx = collision_fp_floor_to_int(cx);
        int gy = collision_fp_floor_to_int(cy);
        if (gx < 0 || gy < 0 || gx >= MAP_W || gy >= MAP_H ||
            map_is_collidable(map_tile_at(gx, gy))) {
            to_destroy.push_back(e);
            continue;
        }

        // 命中敌人：自实现的 OBB 扫掠判定（SAT）。
        // 原来只在"落点"做一次 AABB：箭每 tick 位移 0.5 米(50px)，擦边球能否命中取决于
        // 采样点恰好落在哪 —— 表现为"看着没碰到却命中 / 明显重叠才命中"。
        // 现在把箭沿速度方向拉成覆盖整段轨迹的旋转矩形，再与敌人盒做 SAT 相交测试。
        fp_t mvx = fp_sub(cx, prev_cx), mvy = fp_sub(cy, prev_cy);
        fp_t travel = fp_sqrt(fp_add(fp_mul(mvx, mvx), fp_mul(mvy, mvy)));
        fp_t ddx = fp_one(), ddy = fp_zero();                       // 运动方向单位向量
        if (travel > 0) { ddx = fp_div(mvx, travel); ddy = fp_div(mvy, travel); }

        fp_t bhalf = fp_div(lrec.width, fp_from_float(2.0f));       // 箭半边长

        // 扫掠体：直接用 joy 的 collision2df_swept_obb 求「起点盒 ⊕ 位移」的凸包。
        // 箭沿自身长轴平移，凸包退化成矩形；斜向平移时是六边形 —— 统一按多边形处理。
        vec2f_t swept[8];
        int swept_n = 0;
        vec2f_t bcenter = { prev_cx, prev_cy };
        vec2f_t baxis   = { ddx, ddy };
        collision2df_swept_obb(bcenter, baxis, bhalf, bhalf, mvx, mvy, swept, &swept_n);
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
