#ifndef DRAW_BULLET_PASS_H
#define DRAW_BULLET_PASS_H
#include <math.h>
#include <joy/joy.h>
#include "../components/global_state_component.h"
#include "../components/transform_component.h"
#include "../components/bullet_component.h"
#include "../asset_manager.h"
#include "render_math.h"

struct Context;

// 飞行中的箭/子弹渲染：用 tilesheet.png row9 col7 的箭贴图（64×64），
// 按速度方向旋转（贴图原生箭尖朝上 = -90°）。纯渲染，不参与帧同步、不进快照。
// 图集缺失时跳过绘制（逻辑仍在跑），不阻塞游戏。
static const char* kBulletTilesheetPath = "joy_topdown_template/tilesheet.png";

static void draw_bullet_pass(Context* ctx)
{
    auto& cam = ctx->ecs.ctx().get<GlobalState>().camera_position;
    auto* am = AssetManager::GetInstance();
    if (!am) return;
    image_p img = am->GetImage(ctx->r2d, kBulletTilesheetPath);
    if (!img) return;

    // 箭贴图：row9 col7，64×64
    jrect_t cell = { 448.0f, 576.0f, 64.0f, 64.0f };

    auto view = ctx->ecs.view<BulletComponent, TransformComponent>();
    for (auto e : view) {
        if (!ctx->ecs.valid(e)) continue;
        auto& t = view.get<TransformComponent>(e);
        auto& b = view.get<BulletComponent>(e);

        vec2_t pos;
        pos.x = meters_to_pixels(t.position.x - cam.x);
        pos.y = meters_to_pixels(t.position.y - cam.y);

        // 朝向 = 速度方向（屏幕坐标 y 向下）；贴图原生箭尖朝上(-90°)，
        // 旋转角 = velAngle - (-90°) = velAngle + 90°。速度为 0 时兜底朝上(旋转 0)。
        float velAngle = atan2f((float)b.vy, (float)b.vx);
        float ang = ((float)b.vx == 0.0f && (float)b.vy == 0.0f)
                        ? 0.0f
                        : velAngle + GD_PI * 0.5f;

        // 绘制高度 = 1 米（原写死 50px 即 PPM=50 时的 1 米），贴图 64px → scale = 1米像素/64
        float scale = (float)PIXELS_PER_METER / 64.0f;
        vec2_t scl    = { scale, scale };
        vec2_t origin = { cell.w * 0.5f, cell.h * 0.5f };
        image_draw_ex(img, &cell, pos, ang, scl, origin);
    }
}

#endif
