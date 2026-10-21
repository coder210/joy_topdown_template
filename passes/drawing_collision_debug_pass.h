#ifndef DRAWING_COLLISION_DEBUG_PASS_H
#define DRAWING_COLLISION_DEBUG_PASS_H

#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/player_component.h"
#include "../components/local_only_component.h"
#include "../components/global_state_component.h"
#include "../components/enemy_component.h"
#include "../components/bullet_component.h"
#include "../protocol.h"
#include "../context.h"

struct Context;

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

    // 敌人碰撞盒：蓝色线框（诊断用——和 1m 贴图对比，看真实判定范围）
    auto eview = ctx->ecs.view<EnemyComponent, LogicRectComponent, LogicPositionComponent>();
    r2d_set_color(ctx->r2d, 60, 90, 230, 220);
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

    // 子弹/箭碰撞盒：橙色线框（诊断用——0.15m 小盒 vs 1m 贴图，看命中判定到底在哪）
    auto bview = ctx->ecs.view<BulletComponent, LogicRectComponent, LogicPositionComponent>();
    r2d_set_color(ctx->r2d, 240, 150, 30, 230);
    for (auto e : bview) {
        auto& pos  = bview.get<LogicPositionComponent>(e);
        auto& rect = bview.get<LogicRectComponent>(e);
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
}

#endif