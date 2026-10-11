#ifndef DRAW_ENEMY_PASS_H
#define DRAW_ENEMY_PASS_H
#include <math.h>
#include <joy/joy.h>
#include "../components/global_state_component.h"
#include "../components/enemy_component.h"
#include "../components/transform_component.h"
#include "../components/action_state_component.h"
#include "../protocol.h"
#include "render_math.h"

struct Context;

// 绘制旋转方块（几何占位：敌人尚未接入序列帧素材前的临时表现）
static void draw_rotated_square(Context* ctx, vec2_t c, float half, float rot,
        jcolor_t fill, jcolor_t stroke)
{
    vec2_t pts[4];
    for (int i = 0; i < 4; i++) {
        float a = rot + i * (GD_PI * 0.5f);
        pts[i].x = c.x + cosf(a) * half;
        pts[i].y = c.y + sinf(a) * half;
    }
    shape_draw_polygon(ctx->r2d, "fill", pts, 4, fill);
    shape_draw_polygon(ctx->r2d, "line", pts, 4, stroke);
}

// 渲染所有敌人实体（墨色方块 + 朝向线，临时占位美术）
static void draw_enemy_pass(Context* ctx)
{
    auto view = ctx->ecs.view<EnemyComponent, TransformComponent>();
    for (auto e : view) {
        if (!ctx->ecs.valid(e)) continue;
        auto& t = view.get<TransformComponent>(e);
        auto& cam = ctx->ecs.ctx().get<GlobalState>().camera_position;
        vec2_t c;
        c.x = meters_to_pixels(t.position.x - cam.x);
        c.y = meters_to_pixels(t.position.y - cam.y);

        // 视觉半径：紧凑固定值，避免按碰撞盒全尺寸画出遮挡地图的巨大方块。
        float half = meters_to_pixels(0.30f);
        if (half < 8.0f) half = 8.0f;

        // 朝向（由 ActionStateComponent.dir 推导）
        float dir_rad = -GD_PI * 0.5f;
        if (ctx->ecs.all_of<ActionStateComponent>(e)) {
            int dir_val = ctx->ecs.get<ActionStateComponent>(e).dir;
            switch (dir_val) {
            case 0: dir_rad =  GD_PI * 0.5f; break;
            case 1: dir_rad =  GD_PI;        break;
            case 2: dir_rad =  0.0f;         break;
            case 3: dir_rad = -GD_PI * 0.5f; break;
            }
        }

        // 敌人：墨色填充 + 淡墨描边
        jcolor_t fill   = { 40, 40, 45, 255 };
        jcolor_t stroke = { 120, 120, 128, 255 };
        draw_rotated_square(ctx, c, half, 0.785398f, fill, stroke);

        // 朝向指示：淡朱砂红
        r2d_set_color(ctx->r2d, 200, 60, 50, 220);
        vec2_t tip = { c.x + cosf(dir_rad) * half * 0.8f, c.y + sinf(dir_rad) * half * 0.8f };
        shape_draw_line(ctx->r2d, c.x, c.y, tip.x, tip.y);
    }
}

#endif
