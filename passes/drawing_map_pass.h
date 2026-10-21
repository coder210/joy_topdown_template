#ifndef DRAWING_MAP_PASS_H
#define DRAWING_MAP_PASS_H

#include <cmath>
#include <joy/joy.h>
#include "../components/global_state_component.h"
#include "../systems/map_def.h"
#include "../protocol.h"
#include "../asset_manager.h"

struct Context;

// Scribble Dungeons 地形图集：14 列 × 11 行，每块 64×64
//（源坐标映射见 map_def.h 的 tile_sprite_of / TILE_ATLAS_BLK）
// 路径与 jtilemap 加载器一致：图集位于 joy_topdown_template/tilesheet.png（Tiled 加载器按 MAP_ASSET_BASE 解析）
static constexpr const char* kTerrainAtlasPath = "joy_topdown_template/tilesheet.png";

// 地图地形渲染：按 64×64 网格逐格贴图集瓦片（Scribble 手绘白底深线风格）。
// 图集缺失时兜底为纯色块（墙=墨色、地面=纯白），保证仍可运行。
// 格子尺寸每格 1 米（像素由 meters_to_pixels 换算，随 PIXELS_PER_METER 变化），
// 与实体/相机/碰撞网格精确对齐。（旧注释写"=50px"是 PPM=50 时代的遗留，现已不固定。）
static void draw_map_pass(Context* ctx)
{
    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    float cx = meters_to_pixels(gs.camera_position.x);
    float cy = meters_to_pixels(gs.camera_position.y);

    // 地面底色：先整屏铺纯白（可通行区域）。地图 Background 层的空瓦片(GID=0)处
    // 即为透明，透出的就是这层颜色，故它是玩家看到的"背景"主色。
    // 用比例接口铺满整屏（0,0,1,1），不写死 480x320 —— 逻辑分辨率改变时自动适配。
    r2d_set_color(ctx->r2d, 255, 255, 255, 255);
    shape_draw_rectangle_f(ctx->r2d, "fill", 0.0f, 0.0f, 1.0f, 1.0f);

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

    // 地形瓦片：遍历 64×64 网格逐格贴图集；仅绘制落在可视画布内的格子
    //（画布尺寸按逻辑分辨率换算，每格 50px ⇒ 百来次绘制，开销可忽略）
    image_p atlas = AssetManager::GetInstance()
        ? AssetManager::GetInstance()->GetImage(ctx->r2d, kTerrainAtlasPath) : NULL;
    const float cell   = meters_to_pixels(1.0f);
    const float kScale = cell / (float)TILE_ATLAS_BLK;   // 64px 图块 → 每格画布像素

    // 画布尺寸（像素）：由比例 1.0 换算得到，不写死，供下面的屏幕外剔除使用
    float canvas_w = 0.0f, canvas_h = 0.0f;
    r2d_fpx(ctx->r2d, 1.0f, 1.0f, 0.0f, 0.0f, &canvas_w, &canvas_h, NULL, NULL);

    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            float wx = meters_to_pixels((float)x) - cx;   // 格左上角（画布像素）
            float wy = meters_to_pixels((float)y) - cy;
            // 屏幕外快速剔除（边界由逻辑分辨率换算，不写死）
            if (wx + cell < 0.0f || wx > canvas_w) continue;
            if (wy + cell < 0.0f || wy > canvas_h) continue;

            int type = map_tile_at(x, y);
            if (atlas) {
                TileSprite ts = tile_sprite_of(type);
                jrect_t  src    = { (float)ts.src_x, (float)ts.src_y,
                                    (float)TILE_ATLAS_BLK, (float)TILE_ATLAS_BLK };
                vec2_t   pos    = { wx, wy };
                vec2_t   scale  = { kScale, kScale };
                vec2_t   origin = { 0.0f, 0.0f };
                image_draw_ex(atlas, &src, pos, 0.0f, scale, origin);
            } else if (map_is_collidable(type)) {
                // 图集缺失兜底：墨色墙体块（水墨风）
                r2d_set_color(ctx->r2d, 40, 40, 45, 255);
                jrect_t wall = { wx, wy, cell, cell };
                shape_draw_rectangle(ctx->r2d, "fill", wall);
            }
        }
    }
}

#endif