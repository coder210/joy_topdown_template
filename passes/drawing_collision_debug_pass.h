#ifndef DRAWING_COLLISION_DEBUG_PASS_H
#define DRAWING_COLLISION_DEBUG_PASS_H

#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/player_component.h"
#include "../components/local_only_component.h"
#include "../components/enemy_component.h"
#include "../components/bullet_component.h"
#include "../components/global_state_component.h"
#include "../protocol.h"
#include "../context.h"

struct Context;

#include <math.h>   // sqrtf：箭的扫掠盒需要归一化速度向量

// 碰撞盒调试可视化：绘制玩家 AABB（淡墨灰）与墙 AABB（更淡暖灰），均为低透明度，
// 标注实体真实占位（LogicPosition 左上角 + LogicRect 宽高），避免喧宾夺主。
static void drawing_collision_debug_pass(Context* ctx)
{
    // 开关：设置界面"显示碰撞框"勾选项，关闭时跳过（帧同步无关，仅渲染辅助）
    if (!ctx->settings.show_collision) return;

    auto& cam = ctx->ecs.ctx().get<GlobalState>().camera_position;

    // 玩家碰撞盒：红色线框
    auto pview = ctx->ecs.view<PlayerComponent, LogicRectComponent, LogicPositionComponent>();
    r2d_set_color(ctx->r2d, 220, 45, 45, 180);
    for (auto e : pview) {
        auto& pos  = pview.get<LogicPositionComponent>(e);
        auto& rect = pview.get<LogicRectComponent>(e);
        float w = fp_to_float(rect.width);
        float h = fp_to_float(rect.height);
        if (w <= 0.0f || h <= 0.0f) continue;
        jrect_t r;
        r.x = meters_to_pixels(fp_to_float(pos.x) - cam.x);
        r.y = meters_to_pixels(fp_to_float(pos.y) - cam.y);
        r.w = meters_to_pixels(w);
        r.h = meters_to_pixels(h);
        shape_draw_rectangle(ctx->r2d, "line", r);
    }

    // 墙碰撞盒：绿色线框（墙由 map_spawn_system 生成为 LocalOnly 实体，1×1 米）
    auto wview = ctx->ecs.view<LocalOnlyComponent, LogicRectComponent, LogicPositionComponent>();
    r2d_set_color(ctx->r2d, 50, 200, 95, 150);
    for (auto e : wview) {
        auto& pos  = wview.get<LogicPositionComponent>(e);
        auto& rect = wview.get<LogicRectComponent>(e);
        float w = fp_to_float(rect.width);
        float h = fp_to_float(rect.height);
        if (w <= 0.0f || h <= 0.0f) continue;
        jrect_t r;
        r.x = meters_to_pixels(fp_to_float(pos.x) - cam.x);
        r.y = meters_to_pixels(fp_to_float(pos.y) - cam.y);
        r.w = meters_to_pixels(w);
        r.h = meters_to_pixels(h);
        shape_draw_rectangle(ctx->r2d, "line", r);
    }

    // 敌人碰撞盒：橙色线框（EnemyComponent + LogicRect + LogicPosition）
    // 原来 pass 只有"玩家 + 墙"两段，敌人/箭没有绘制代码，所以勾了也看不到。
    auto eview = ctx->ecs.view<EnemyComponent, LogicPositionComponent, LogicRectComponent>();
    r2d_set_color(ctx->r2d, 240, 160, 40, 190);
    for (auto e : eview) {
        auto& pos  = eview.get<LogicPositionComponent>(e);
        auto& rect = eview.get<LogicRectComponent>(e);
        float w = fp_to_float(rect.width);
        float h = fp_to_float(rect.height);
        if (w <= 0.0f || h <= 0.0f) continue;
        jrect_t r;
        r.x = meters_to_pixels(fp_to_float(pos.x) - cam.x);
        r.y = meters_to_pixels(fp_to_float(pos.y) - cam.y);
        r.w = meters_to_pixels(w);
        r.h = meters_to_pixels(h);
        shape_draw_rectangle(ctx->r2d, "line", r);
    }

    /* 箭/子弹：画"扫掠旋转盒(OBB)"，必须与 bullet_system 的判定完全一致。
     * 判定已不是落点 AABB，而是箭沿速度方向拉长、覆盖整 tick 位移的旋转矩形；
     * 若这里仍画 40x40 的轴对齐小方框，看到的框就和真实判定对不上。
     * 构造方式与 bullet_system 一致：半长 = 半边长 + 位移/2，中心回退位移/2。 */
    auto bview = ctx->ecs.view<BulletComponent, LogicPositionComponent, LogicRectComponent>();
    const float tick = ctx->ecs.ctx().get<GlobalState>().fixed_timestep;
    jcolor_t bc = { 60, 190, 255, 210 };
    for (auto e : bview) {
        auto& pos  = bview.get<LogicPositionComponent>(e);
        auto& rect = bview.get<LogicRectComponent>(e);
        auto& bul  = bview.get<BulletComponent>(e);
        float w = fp_to_float(rect.width);
        float h = fp_to_float(rect.height);
        if (w <= 0.0f || h <= 0.0f) continue;

        // 当前中心（世界米）
        float ccx = fp_to_float(pos.x) + w * 0.5f;
        float ccy = fp_to_float(pos.y) + h * 0.5f;

        // 运动方向单位向量 + 本 tick 位移（米）
        float vx = fp_to_float(bul.vx), vy = fp_to_float(bul.vy);
        float vlen = sqrtf(vx * vx + vy * vy);
        float dx = 1.0f, dy = 0.0f;
        if (vlen > 0.0f) { dx = vx / vlen; dy = vy / vlen; }
        float travel = vlen * tick;

        // 扫掠体：直接调 joy 的 collision2df_swept_obb（与 bullet_system 同一个函数），
        // 传「起点盒 + 位移」，凸包由 joy 求 —— 不再在这里手写拉长/回退，
        // 避免两套实现各自漂移。起点 = 当前中心回退一个 tick 的位移。
        float pcx = ccx - dx * travel;      // 起点中心（世界米）
        float pcy = ccy - dy * travel;

        vec2f_t sc = { fp_from_float(pcx), fp_from_float(pcy) };
        vec2f_t sa = { fp_from_float(dx),  fp_from_float(dy)  };
        fp_t hlong  = fp_div(fp_from_float(BULLET_HIT_LEN), fp_from_float(2.0f));
        fp_t hshort = fp_div(fp_from_float(BULLET_HIT_WID), fp_from_float(2.0f));
        vec2f_t swept[8];
        int swept_n = 0;
        collision2df_swept_obb(sc, sa, hlong, hshort,
                               fp_from_float(dx * travel),
                               fp_from_float(dy * travel),
                               swept, &swept_n);

        vec2_t pts[8];
        for (int i = 0; i < swept_n; i++) {
            pts[i].x = meters_to_pixels(fp_to_float(swept[i].x) - cam.x);
            pts[i].y = meters_to_pixels(fp_to_float(swept[i].y) - cam.y);
        }
        if (swept_n >= 3) shape_draw_polygon(ctx->r2d, "line", pts, swept_n, bc);
    }
}

#endif