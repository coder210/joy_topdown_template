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
#include "../asset_manager.h"
#include "hpbar.h"

struct Context;

// 敌人纹理（与玩家同画风）：64x64 调色板、深墨 #2A2A2A 粗描边 + 白色内衬 + 纯色主体块。
// 剪影为"圆角团 + 双角 + 双眼"，主色用品红 #F04C9E（与 绿/紫/红/黄 四个玩家色区分）。
// 加载失败时自动回退到墨色占位方块（见下）。
static const char* kEnemyImpPath  = "joy_topdown_template/sprites/enemies/imp.png";
static const float  kEnemyDrawH   = 50.0f;   // 绘制高度（像素），与玩家一致，便于比例统一

// 敌人血条：与玩家同款胶囊细条（实现见 passes/hpbar.h），悬浮于精灵正上方。
// 尺寸比玩家小一号（敌人数量多、体型小）：内芯 28×8、外 1px 墨描边。
// 填充用朱砂红 {200,60,50}——与朝向指示线同色，一眼标识"敌方"。
static const int   EHBAR_W   = 28;
static const int   EHBAR_H   = 8;
static const int   EHBAR_PAD = 1;
static image_p s_ehpbar_track = NULL;
static image_p s_ehpbar_fill  = NULL;

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

// 渲染所有敌人实体（imp.png 纹理 + 朝向线；纹理缺失时回退墨色占位方块）
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

        // 视觉半径：紧凑固定值（用于朝向指示线长度；贴图按固定高度绘制，不受碰撞盒尺寸影响）。
        float half = meters_to_pixels(0.30f);
        if (half < 8.0f) half = 8.0f;

        // 敌人本体：优先用纹理（与玩家同画风），按固定高度 50px 等比缩放、居中于实体位置。
        // 纹理加载失败时回退为墨色占位方块（接入素材前的旧表现，保证不会画丢）。
        auto* am = AssetManager::GetInstance();
        image_p eimg = am ? am->GetImage(ctx->r2d, kEnemyImpPath) : NULL;
        if (eimg) {
                float tw = 0.0f, th = 0.0f;
                if (image_get_size(eimg, &tw, &th) && tw > 0.0f && th > 0.0f) {
                        float scl = kEnemyDrawH / th;
                        jrect_t src = { 0.0f, 0.0f, tw, th };
                        vec2_t pos = { c.x, c.y };
                        vec2_t org = { tw * 0.5f, th * 0.5f };
                        image_draw_ex(eimg, &src, pos, 0.0f, { scl, scl }, org);
                }
        } else {
                jcolor_t fill   = { 40, 40, 45, 255 };
                jcolor_t stroke = { 120, 120, 128, 255 };
                draw_rotated_square(ctx, c, half, 0.785398f, fill, stroke);
        }

        // 敌人血条：与玩家同款胶囊细条，悬浮于精灵正上方（水平，不旋转）。
        if (ctx->ecs.all_of<HealthComponent>(e)) {
                auto& ehc = ctx->ecs.get<HealthComponent>(e);
                if (ehc.hp > 0 &&
                    hpbar_ensure(ctx->r2d, &s_ehpbar_track, &s_ehpbar_fill,
                                 EHBAR_W, EHBAR_H, EHBAR_PAD)) {
                        float eratio = (ehc.max_hp > 0)
                                ? ((float)ehc.hp / (float)ehc.max_hp) : 0.0f;
                        // 精灵按 kEnemyDrawH 等高绘制 → 顶边在 c.y - kEnemyDrawH/2，
                        // 血条中心再上移 间距 + (半厚+描边)。
                        float ecy = c.y - kEnemyDrawH * 0.5f - 3.0f
                                  - ((float)EHBAR_H * 0.5f + (float)EHBAR_PAD);
                        hpbar_draw(ctx->r2d, s_ehpbar_track, s_ehpbar_fill,
                                   EHBAR_W, EHBAR_H, EHBAR_PAD, eratio,
                                   c.x, ecy, 0.0f, 200, 60, 50);
                }
        }

        // 朝向：直接读 interpolate_pass 平滑后的连续旋转角（t.rotation），
        // 不再是 4 向离散跳变——拐弯时朝向线随移动方向平滑转动。
        float dir_rad = t.rotation;

        // 朝向指示：淡朱砂红（敌人贴图对称，靠这条线读朝向）
        r2d_set_color(ctx->r2d, 200, 60, 50, 220);
        vec2_t tip = { c.x + cosf(dir_rad) * half * 0.8f, c.y + sinf(dir_rad) * half * 0.8f };
        shape_draw_line(ctx->r2d, c.x, c.y, tip.x, tip.y);
    }
}

#endif
