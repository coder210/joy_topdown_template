#ifndef DRAWING_MINIMAP_PASS_H
#define DRAWING_MINIMAP_PASS_H

#include <joy/joy.h>
#include "../components/global_state_component.h"
#include "../components/enemy_component.h"
#include "../components/player_component.h"
#include "../components/health_component.h"
#include "../components/logic_position_component.h"
#include "../components/transform_component.h"
#include "../systems/map_def.h"
#include "../protocol.h"
#include "fog_pass_2d.h"   // 复用视野参数(FOV2D_VIEW_ANGLE 等)与朝向辅助

struct Context;

// 小地图：世界坐标 [0,MAP_W]x[0,MAP_H] 映射到右上角矩形（避开右下角虚拟按键）。
// 水墨风：宣纸底 + 墨色边框；地形/障碍物始终全图显示；
// 仅敌人与玩家单位按本地玩家视野扇形提示身位（看不到脑袋后面的单位）。
// 身周近圈(FOV2D_NEAR_RADIUS)不受朝向限制，始终揭示。
static void drawing_minimap_pass(Context* ctx)
{
    if (!ctx->r2d) return;

    // 视口按地图宽高比适配（MAP_W/MAP_H 已随 tilemap 动态更新），非方形地图不拉伸。
    const float mm_size = 128.0f;
    float mm_w = mm_size, mm_h = mm_size;
    if (MAP_W > MAP_H)      { mm_w = mm_size; mm_h = mm_size * (float)MAP_H / (float)MAP_W; }
    else if (MAP_H > MAP_W) { mm_h = mm_size; mm_w = mm_size * (float)MAP_W / (float)MAP_H; }
    const float mm_x = 480.0f - mm_w - 8.0f, mm_y = 8.0f;

    // 本地玩家位置与朝向；无本地玩家(服务端/观战)则揭示全图
    float ppx = 0.0f, ppy = 0.0f;
    int   pdir = -1;
    bool  has_local = fog2d_find_player(ctx, ppx, ppy, pdir);
    float facing = has_local ? fog2d_dir_to_angle(pdir) : 0.0f;
    bool  cone_on = has_local;

    const float kRad2Deg = 57.2957795f;
    // wx,wy 为世界坐标；判断是否落入视野扇形（就近圈或角度内）
    auto revealed = [&](float wx, float wy, float px, float py) {
        if (!cone_on) return true;
        float dx = wx - px, dy = wy - py;
        float d = sqrtf(dx * dx + dy * dy);
        if (d <= FOV2D_NEAR_RADIUS) return true;
        if (d > FOV2D_VIS_RADIUS) return false;
        float a = atan2f(dx, dy) * kRad2Deg;
        return fabsf(fog2d_angle_diff(a, facing)) <= FOV2D_VIEW_ANGLE * 0.5f + 1e-4f;
    };

    // 宣纸底 + 墨色边框
    r2d_set_color(ctx->r2d, 248, 246, 240, 255);
    jrect_t bg = { mm_x, mm_y, mm_w, mm_h };
    shape_draw_rectangle(ctx->r2d, "fill", bg);

    r2d_set_color(ctx->r2d, 40, 40, 45, 255);
    jrect_t border = { mm_x - 1.0f, mm_y - 1.0f, mm_w + 2.0f, mm_h + 2.0f };
    shape_draw_rectangle(ctx->r2d, "line", border);

    auto to_map = [&](float wx, float wy, float& mx, float& my) {
        mx = mm_x + wx / (float)MAP_W * mm_w;
        my = mm_y + wy / (float)MAP_H * mm_h;
    };

    // 每格边长(px)：X/Y 等比一致，墙块严格落在各自格内，避免纵向错位重叠
    const float cell = mm_w / (float)MAP_W;

    // 障碍物（淡墨方块，始终全图显示——地形布局是常识，不做视野限制）
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (!map_is_collidable(map_tile_at(x, y))) continue;
            float cwx = (float)x + 0.5f, cwy = (float)y + 0.5f;
            float mx, my;
            to_map(cwx, cwy, mx, my);
            r2d_set_color(ctx->r2d, 90, 90, 98, 255);
            jrect_t bq = { mx - cell * 0.5f, my - cell * 0.5f, cell, cell };
            shape_draw_rectangle(ctx->r2d, "fill", bq);
        }
    }

    // 存活敌人（朱砂红点，仅视野内揭示）——用 TransformComponent.position(世界中心)。
    // 标记尺寸与墙格同尺度：主场景玩家/敌人碰撞盒 0.8m ≈ 0.8格，此处约为 1 格(≈2px)，
    // 避免之前 4px/6px 标记(2~3 格)与墙块 2px 比例严重失调。
    auto ev = ctx->ecs.view<EnemyComponent, TransformComponent, HealthComponent>();
    for (auto e : ev) {
        if (ev.get<HealthComponent>(e).dead) continue;
        auto& t = ev.get<TransformComponent>(e);
        if (!revealed(t.position.x, t.position.y, ppx, ppy)) continue;
        float mx, my;
        to_map(t.position.x, t.position.y, mx, my);
        r2d_set_color(ctx->r2d, 200, 60, 50, 255);
        jrect_t dq = { mx - cell * 0.4f, my - cell * 0.4f, cell * 0.8f, cell * 0.8f };
        shape_draw_rectangle(ctx->r2d, "fill", dq);
    }

    // 玩家（本地墨色大点始终揭示；其他玩家淡墨小点仅视野内揭示）——同样用世界中心
    auto pv = ctx->ecs.view<PlayerComponent, TransformComponent>();
    for (auto e : pv) {
        auto& t = pv.get<TransformComponent>(e);
        bool local = pv.get<PlayerComponent>(e).conv == (int)net_session(ctx).local_conv;
        if (!local && !revealed(t.position.x, t.position.y, ppx, ppy)) continue;
        float mx, my;
        to_map(t.position.x, t.position.y, mx, my);
        if (local) {
            r2d_set_color(ctx->r2d, 40, 40, 45, 255);
            jrect_t lq = { mx - cell * 0.5f, my - cell * 0.5f, cell, cell };
            shape_draw_rectangle(ctx->r2d, "fill", lq);
        } else {
            r2d_set_color(ctx->r2d, 120, 120, 128, 255);
            jrect_t oq = { mx - cell * 0.4f, my - cell * 0.4f, cell * 0.8f, cell * 0.8f };
            shape_draw_rectangle(ctx->r2d, "fill", oq);
        }
    }
}

#endif