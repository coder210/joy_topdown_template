#ifndef FOG_PASS_2D_H
#define FOG_PASS_2D_H
#include <vector>
#include <cmath>
#include <joy/joy.h>
#include "../components/global_state_component.h"
#include "../components/player_component.h"
#include "../components/transform_component.h"
#include "../components/action_state_component.h"
#include "../systems/map_def.h"
#include "../protocol.h"

struct Context;

#ifndef FOV2D_VIS_RADIUS
#define FOV2D_VIS_RADIUS 3.0f      // 前方扇形视野长度（米）：面朝方向扇形延伸到此
#endif
#ifndef FOV2D_NEAR_RADIUS
#define FOV2D_NEAR_RADIUS 0.8f     // 身周近视角圆半径（米）：扇形外（含身后）此距离内可见
#endif
#ifndef FOV2D_OUTER_RADIUS
#define FOV2D_OUTER_RADIUS 20.0f   // 雾外径（米）：需盖住整个可见屏幕
#endif
#ifndef FOV2D_MESH_ANGLE
#define FOV2D_MESH_ANGLE 2.0f      // 边界细分角度（度）：越小边界越圆润
#endif
#ifndef FOV2D_FEATHER_PX
#define FOV2D_FEATHER_PX 5.0f      // 视野标记描边宽度（像素）：一圈细淡墨线，仅标出范围/方向，不遮挡场景
#endif
#ifndef FOV2D_VIEW_ANGLE
#define FOV2D_VIEW_ANGLE 150.0f    // 视野角度（度）：以面朝方向为中轴的前方扇形，身后不可见
#endif

// 玩家逻辑朝向 dir(0=down 1=left 2=right 3=up) → FOV 射线角度(度)。
// FOV 方向约定：射线方向=(sin a, cos a) 对应世界 (x, y)；世界 up=-y, right=+x。
static float fog2d_dir_to_angle(int dir)
{
    static const float T[4] = { 0.0f, 270.0f, 90.0f, 180.0f };
    return (dir < 0 || dir > 3) ? 0.0f : T[dir];
}

// 角度差 b-a，归一化到 [-180, 180]
static float fog2d_angle_diff(float a, float b)
{
    float d = fmodf(a - b + 540.0f, 360.0f) - 180.0f;
    return d;
}

// 地图布局由服务端/客户端共享且静态一致，遮挡矩形只需构建一次。
// 跨关卡重进场景会自动失效（记录 s_built_level）。
static bool fog2d_ensure_walls(std::vector<joy_fov_rect>& out)
{
    static std::vector<joy_fov_rect> s_walls;
    static int s_built_level = -1;
    if (s_built_level != g_map_info.level) {
        s_walls.clear();
        s_built_level = g_map_info.level;
        for (int y = 0; y < MAP_H; y++) {
            for (int x = 0; x < MAP_W; x++) {
                if (!map_is_collidable(map_tile_at(x, y))) continue;
                joy_fov_rect a;
                a.x = x + 0.5f; a.z = y + 0.5f;   // 格心
                a.hx = 0.5f;    a.hz = 0.5f;      // 半宽
                s_walls.push_back(a);
            }
        }
    }
    out = s_walls;
    return true;
}

// 定位本地玩家世界坐标（XZ→2D 的 x/y）与朝向 dir。返回 false 表示无本地玩家（服务端/观战）。
static bool fog2d_find_player(Context* ctx, float& px, float& py, int& out_dir)
{
    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    int follow_conv = (net_session(ctx).local_conv > 0) ? (int)net_session(ctx).local_conv : gs.camera_follow_conv;
    if (follow_conv < 0) return false;
    auto pv = ctx->ecs.view<PlayerComponent, TransformComponent, ActionStateComponent>();
    for (auto e : pv) {
        if (pv.get<PlayerComponent>(e).conv == follow_conv) {
            auto& t = pv.get<TransformComponent>(e);
            px = t.position.x; py = t.position.y;
            out_dir = pv.get<ActionStateComponent>(e).dir;
            return true;
        }
    }
    return false;
}

// 在地图/实体之上绘制 2D 战争迷雾：仅显示角色视野附近，墙后与远端被墨雾覆盖。
// 需在地图绘制之后、HUD 之前调用（相机相对像素坐标系）。
static void fog_pass_2d(Context* ctx)
{
    if (!ctx->r2d) return;

    float px, py;
    int   pdir;
    if (!fog2d_find_player(ctx, px, py, pdir)) return;

    std::vector<joy_fov_rect> walls;
    if (!fog2d_ensure_walls(walls)) return;
    if (walls.empty()) return;   // 无墙 → 全图可见，不画雾

    const int SEG = (int)ceilf(360.0f / FOV2D_MESH_ANGLE);
    const float STEP = 360.0f / SEG;

    // 世界米为单位逐角精确射线（绝不让雾穿墙），再换算到像素空间绘制。
    static std::vector<float> R_world, R_px;
    R_world.resize((size_t)SEG);
    R_px.resize((size_t)SEG);
    joy_fov_compute_radii(walls.data(), (int)walls.size(),
        px, py, FOV2D_VIS_RADIUS, 0.0f, STEP, SEG, R_world.data());

    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    float cxp = meters_to_pixels(px) - meters_to_pixels(gs.camera_position.x);
    float cyp = meters_to_pixels(py) - meters_to_pixels(gs.camera_position.y);
    float facing = fog2d_dir_to_angle(pdir);
    float half = FOV2D_VIEW_ANGLE * 0.5f;
    float near_px = meters_to_pixels(FOV2D_NEAR_RADIUS);
    for (int i = 0; i < SEG; i++) {
        // 视野扇形中轴 = 面朝方向；扇形外方向只保留身周近视角，形成"看不到身后"
        if (fabsf(fog2d_angle_diff(STEP * (float)i, facing)) <= half + 1e-4f)
            R_px[i] = meters_to_pixels(R_world[i]);
        else
            R_px[i] = near_px;
    }

    joy_fov_draw_2d(ctx->r2d, cxp, cyp, R_px.data(), SEG,
        FOV2D_FEATHER_PX, meters_to_pixels(FOV2D_OUTER_RADIUS));
}

// 判定世界坐标 (ex,ey) 上的实体在本地玩家全向视野内是否可见（距离 + 墙遮挡）。
// 无本地玩家（服务端/观战）则视为可见。默认实体绘制在雾之上，可用此函数选择性隐藏。
static bool fog2d_entity_visible(Context* ctx, float ex, float ey)
{
    if (!ctx->r2d) return true;

    float px, py;
    int   pdir;
    if (!fog2d_find_player(ctx, px, py, pdir)) return true;
    float facing = fog2d_dir_to_angle(pdir);

    float dx = ex - px, dy = ey - py;
    float d = sqrtf(dx * dx + dy * dy);
    if (d <= FOV2D_NEAR_RADIUS) return true;   // 身周近视角：扇形外也可见
    if (d > FOV2D_VIS_RADIUS) return false;

    // 视野角度限制：不在面朝方向扇形内的实体不可见（看不到身后）
    const float kRad2Deg = 57.2957795f;
    float dir_ang = atan2f(dx, dy) * kRad2Deg;
    if (fabsf(fog2d_angle_diff(dir_ang, facing)) > FOV2D_VIEW_ANGLE * 0.5f + 1e-4f) return false;

    std::vector<joy_fov_rect> walls;
    if (!fog2d_ensure_walls(walls)) return true;
    float inv = 1.0f / d;
    float hit = joy_fov_ray_dist(walls.data(), (int)walls.size(), px, py, dx * inv, dy * inv);
    if (hit < d - 1e-3f) return false;
    return true;
}

#endif