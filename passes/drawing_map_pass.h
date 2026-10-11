#ifndef DRAWING_MAP_PASS_H
#define DRAWING_MAP_PASS_H

#include <cmath>
#include <joy/joy.h>
#include "../components/global_state_component.h"
#include "../systems/map_def.h"
#include "../protocol.h"

struct Context;

// 地图地形色块渲染：不再使用 Kenney tilesheet 图集，改为按 64×64 网格逐格画纯色块。
// 墙（'#'）=墨色块，地面（'.'/'P'/'E' 等）=浅宣纸色块。视觉为黑白水墨风；
// 格子尺寸每格 1 米（=50px），与实体/相机渲染的 1米=50px 精确一致，保证对齐。
static void draw_map_pass(Context* ctx)
{
    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    float cx = meters_to_pixels(gs.camera_position.x);
    float cy = meters_to_pixels(gs.camera_position.y);

    // 地面底色：先整屏铺浅宣纸色（可通行区域）
    r2d_set_color(ctx->r2d, 250, 248, 244, 255);
    jrect_t floor = { 0, 0, 480.0f, 320.0f };
    shape_draw_rectangle(ctx->r2d, "fill", floor);

    // 首选：Tiled 瓦片渲染（关卡 .lua 带 tileset 时按 GID 贴图绘制）
    // 每格画布像素强制为 1 米(=50px)，与实体/相机/碰撞网格精确对齐；相机传画布像素。
    if (g_map_info.tilemap) {
        if (!g_map_info.tilemap_render)
                g_map_info.tilemap_render = jtilemap_render_create(ctx->r2d, MAP_ASSET_BASE, NULL);
        if (g_map_info.tilemap_render) {
                jtilemap_render_set_tile_px(g_map_info.tilemap_render, meters_to_pixels(1.0f));
                jtilemap_render_tiles(g_map_info.tilemap_render, g_map_info.tilemap, cx, cy, (uint32_t)get_ticks());
                return;
        }
    }

    // 墙色块：遍历 64×64 网格，'#' 画墨色方块；仅绘制落在可视画布内的格子
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            if (!map_is_collidable(map_tile_at(x, y))) continue;
            // 格心世界坐标 → 画布像素
            float wx = meters_to_pixels((float)(x + 1)) - cx;      // 格左上/左界
            float wy = meters_to_pixels((float)(y + 1)) - cy;
            // 屏幕外快速剔除
            if (wx + meters_to_pixels(1.0f) < 0.0f || wx - meters_to_pixels(1.0f) > 480.0f) continue;
            if (wy + meters_to_pixels(1.0f) < 0.0f || wy - meters_to_pixels(1.0f) > 320.0f) continue;

            // 墨色墙体块 + 稍淡描边（水墨风）
            r2d_set_color(ctx->r2d, 40, 40, 45, 255);
            jrect_t cell = { wx - meters_to_pixels(1.0f), wy - meters_to_pixels(1.0f), meters_to_pixels(1.0f), meters_to_pixels(1.0f) };
            shape_draw_rectangle(ctx->r2d, "fill", cell);
        }
    }
}

#endif