#ifndef DRAW_BULLET_PASS_H
#define DRAW_BULLET_PASS_H
#include <math.h>
#include <joy/joy.h>
#include "../components/global_state_component.h"
#include "../components/bullet_component.h"
#include "../components/transform_component.h"
#include "../protocol.h"
#include "render_math.h"

struct Context;

// 渲染所有子弹实体（旋转方块，敌/我区分颜色，临时占位美术）
static void draw_bullet_pass(Context* ctx)
{
    auto view = ctx->ecs.view<BulletComponent, TransformComponent>();
    for (auto e : view) {
        if (!ctx->ecs.valid(e)) continue;
        auto& bc = view.get<BulletComponent>(e);
        auto& t  = view.get<TransformComponent>(e);
        auto& cam = ctx->ecs.ctx().get<GlobalState>().camera_position;
        vec2_t c;
        c.x = meters_to_pixels(t.position.x - cam.x);
        c.y = meters_to_pixels(t.position.y - cam.y);

        bool is_enemy_bullet = (bc.owner_conv == 0);
        jcolor_t glow, body, edge;
        if (is_enemy_bullet) {
            /* 敌人子弹：朱砂红墨 */
            glow = jcolor_t{ 120, 40, 34, 80 };
            body = jcolor_t{ 235, 60, 50, 255 };
            edge = jcolor_t{ 200, 30, 25, 255 };
        } else {
            /* 玩家子弹：青绿水墨 */
            glow = jcolor_t{ 40, 90, 100, 80 };
            body = jcolor_t{ 60, 220, 230, 255 };
            edge = jcolor_t{ 30, 180, 200, 255 };
        }
        float bs = 7.0f;
        float bspin = game_runtime(ctx).visual_time * 6.0f;
        vec2_t pts[4];
        for (int i = 0; i < 4; i++) {
            float a = bspin + i * (GD_PI * 0.5f);
            pts[i].x = c.x + cosf(a) * bs;
            pts[i].y = c.y + sinf(a) * bs;
        }
        shape_draw_polygon(ctx->r2d, "fill", pts, 4, glow);
        for (int i = 0; i < 4; i++) {
            float a = bspin + i * (GD_PI * 0.5f);
            pts[i].x = c.x + cosf(a) * bs * 0.6f;
            pts[i].y = c.y + sinf(a) * bs * 0.6f;
        }
        shape_draw_polygon(ctx->r2d, "fill", pts, 4, body);
        shape_draw_polygon(ctx->r2d, "line", pts, 4, edge);
    }
}

#endif
