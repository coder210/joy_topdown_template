#ifndef MAP_DEF_H
#define MAP_DEF_H

#include <joy/joy.h>
#include <cstdio>
#include <cmath>
#include <cstring>   // std::memcpy
#include "../asset_manager.h"   // 关卡 .lua 路径走资源索引单点查询

// 诊断日志（定义见文件末尾）
static void map_log_load(const char* source, int level);

// jtilemap 渲染资源根：贴图路径经 default_remap 剥掉 ../ 后拼在此前缀之后。
// 与 adventure2d 的 adventure2d/ 用法一致，且和 resources.json 的 joy_topdown_template/... 同一套相对根。
static constexpr const char* MAP_ASSET_BASE = "joy_topdown_template/";

// ============================================================
// 关卡数据唯一来源：Tiled 地图 → 导出 levels/levelN.lua（路径见 resources.json 的 maps 段）。
// 玩法网格 g_map_grid 由 map_load_from_tilemap 从 .lua 解析填充：
//   墙 → "Collision" 对象层的矩形；出生点 → "Player"/"spawn" 对象层；
//   敌人出生点 → "Enemy" 对象层（或 spawn 层里 type=="enemy" 的对象）。
// 服务端/客户端解析同一文件 → 逐格一致，可作帧同步依据。
// 注意：引擎只认 Tiled 的 Lua 导出（.lua），不解析 .tmx（那是编辑器源文件）。
// ============================================================

// C++11 兼容：
//  - 编译期常量用 constexpr（每翻译单元一份拷贝，值相同无碍），可作数组大小；
//  - 可变共享全局（MAP_W/MAP_H/g_map_grid/g_map_info）用「头文件 extern + map_def.cpp 定义」。
extern int MAP_W;
extern int MAP_H;
constexpr int MAP_MAX_CELLS = 64 * 64;                      // 玩法网格/A* 上限
// 关卡总数：须与选关界面 LEVEL_META 条数、resources.json 的 maps 段（level1..levelN）一致。
constexpr int LEVEL_COUNT = 8;

// 地形类型枚举（渲染用；关卡网格仅含墙/地，树石水均为保留类型）
enum MapTileType {
    MAP_TILE_EMPTY = 0,
    MAP_TILE_WALL,    // '#' 或 GID=1
    MAP_TILE_TREE,    // 保留（不再由关卡生成）
    MAP_TILE_ROCK,
    MAP_TILE_WATER,
    MAP_TILE_GRASS,
    MAP_TILE_FLOWER,
    MAP_TILE_PATH,
};

// 运行时网格 + 出生信息（由 map_load_level 填充；定义在 map_def.cpp）
extern char g_map_grid[MAP_MAX_CELLS];  // 行主序 y*MAP_W+x；尺寸由 tilemap/ASCII 决定

struct MapLevelInfo {
    int level = -1;
    int player_cell = -1;      // 'P' / spawn 点格子索引 (y*MAP_W + x)，-1=缺失
    int enemy_cells[256];      // 'E' / enemy 格子索引（行主序扫描，双端同序）
    int enemy_n = 0;
    jtilemap_p  tilemap = nullptr;      // jtilemap 句柄，由 jtilemap_load_file 持有
    jtilemap_render_p tilemap_render = nullptr; // 渲染句柄
};
extern MapLevelInfo g_map_info;

// 设置玩法网格尺寸并整体清为可通行('.')。行主序 y*MAP_W+x。
// 调用方须保证 MAP_W*MAP_H ≤ MAP_MAX_CELLS。
static inline void map_grid_clear(int w, int h)
{
    MAP_W = w;
    MAP_H = h;
    for (int i = 0; i < MAP_W * MAP_H; i++)
        g_map_grid[i] = '.';
}

// 碰撞对象层上下文：对象坐标是 Tiled 像素（每瓦片 tw×th 像素），须换算成玩法格子。
struct map_collision_ctx_t {
    int tw, th;   // tilewidth/tileheight（对象坐标单位）
    int mw, mh;   // 玩法网格宽高（= MAP_W/MAP_H）
};

// 把一个矩形碰撞对象按覆盖到的格子标为墙 '#'（行主序 g_map_grid[y*mw+x]）。
static void map_rasterize_collision_rect(jtilemap_object_p obj, void* arg)
{
    map_collision_ctx_t* c = (map_collision_ctx_t*)arg;
    int c0 = (int)floorf(obj->x / (float)c->tw);
    int r0 = (int)floorf(obj->y / (float)c->th);
    int c1 = (int)ceilf((obj->x + obj->width) / (float)c->tw);
    int r1 = (int)ceilf((obj->y + obj->height) / (float)c->th);
    if (c0 < 0) c0 = 0; if (r0 < 0) r0 = 0;
    if (c1 > c->mw) c1 = c->mw; if (r1 > c->mh) r1 = c->mh;
    for (int y = r0; y < r1; y++)
        for (int x = c0; x < c1; x++)
            g_map_grid[y * c->mw + x] = '#';
}

// 查找碰撞对象层：兼容常见命名（含 Tiled 里 "Collsion" 的拼写差异）。
static jtilemap_object_layer_p map_find_collision_layer(jtilemap_p tm)
{
    static const char* names[] = { "Collision", "Collsion", "collision" };
    for (const char* n : names) {
        jtilemap_object_layer_p ol = jtilemap_find_object_layer(tm, n);
        if (ol) return ol;
    }
    return nullptr;
}

// 探测给定路径的关卡 .lua 是否可读（单点路径；候选前缀遍历已由资源索引取代）
static bool map_tm_probe_path(const char* path)
{
        FILE* f = std::fopen(path, "rb");
        if (f) { std::fclose(f); return true; }
        return false;
}

// 本关是否"已制作"：resources.json 的 maps 段指向的 levelN.lua 是否存在。
// 选关界面据此把缺图的关卡标为制作中并阻止进入（避免空图进游戏）。
static inline bool level_map_available(int level)
{
        if (level < 1 || level > LEVEL_COUNT) return false;
        char map_id[32];
        snprintf(map_id, sizeof(map_id), "level%d", level);
        AssetManager* am = AssetManager::GetInstance();
        const char* rel = am ? am->GetResourcePath(AssetManager::kCatMaps, map_id) : nullptr;
        if (!rel) return false;
        return map_tm_probe_path(rel);
}

// 出生点解析上下文：对象坐标是 Tiled 像素（每瓦片 tw×th 像素），换算成玩法格子。
// point 形状取坐标本身，矩形取中心；type=="enemy" 归敌人，否则归玩家出生点。
struct map_spawn_ctx_t {
    int tw, th;          // tilewidth/tileheight（对象坐标单位）
    int mw, mh;          // 玩法网格宽高（= MAP_W/MAP_H）
    int player_cell = -1;
    int enemy_n = 0;
    int enemy_cells[256];
    bool force_enemy = false;  // true 时该层内所有对象都视作敌人（用于独立的 Enemy 层）
};
static void map_parse_spawn_obj(jtilemap_object_p obj, void* arg)
{
    map_spawn_ctx_t* s = (map_spawn_ctx_t*)arg;
    float cx, cy;
    if (obj->shape == JTILEMAP_SHAPE_POINT) { cx = obj->x; cy = obj->y; }
    else                                    { cx = obj->x + obj->width * 0.5f; cy = obj->y + obj->height * 0.5f; }
    int col = (int)floorf(cx / (float)s->tw);
    int row = (int)floorf(cy / (float)s->th);
    if (col < 0) col = 0; else if (col >= s->mw) col = s->mw - 1;
    if (row < 0) row = 0; else if (row >= s->mh) row = s->mh - 1;
    int cell = row * s->mw + col;
    // 该层强制为敌人（独立 Enemy 层），或对象 type=="enemy" 时归为敌人；其余归玩家出生点。
    bool is_enemy = s->force_enemy || (obj->type && strcmp(obj->type, "enemy") == 0);
    if (is_enemy) {
        if (s->enemy_n < 256) s->enemy_cells[s->enemy_n++] = cell;
    } else {
        s->player_cell = cell;
    }
}
// 查找出生对象层：优先 Tiled 里新增的 "Player"，兼容旧名 "player"/"spawn"。
static jtilemap_object_layer_p map_find_spawn_layer(jtilemap_p tm)
{
    static const char* names[] = { "Player", "player", "spawn" };
    for (const char* n : names) {
        jtilemap_object_layer_p ol = jtilemap_find_object_layer(tm, n);
        if (ol) return ol;
    }
    return nullptr;
}
// 查找敌人对象层：优先 "Enemy"，兼容小写 "enemy" 与地图里误拼的 "Emeny"。
static jtilemap_object_layer_p map_find_enemy_layer(jtilemap_p tm)
{
    static const char* names[] = { "Enemy", "enemy", "Emeny" };
    for (const char* n : names) {
        jtilemap_object_layer_p ol = jtilemap_find_object_layer(tm, n);
        if (ol) return ol;
    }
    return nullptr;
}

// 用 jtilemap 装载本关 Tiled 导出的 levels/levelN.lua 作为玩法+渲染唯一数据源。
//  - 玩法网格 g_map_grid：暂不解析墙（满地图单源对齐前玩家/敌人自由通行；后续加"collision"层再判墙）。
//  - 出生/敌人点：来自 "spawn" 对象层的 player/enemy 对象。
//  - tilemap 同时交给渲染层绘制（draw_map_pass 按 GID 裁 Kenney 图集）。
// 成功（文件存在且解析成功）返回 true；缺失/解析失败返回 false，由调用方回退 ASCII 保证可玩。
static bool map_load_from_tilemap(int level)
{
    // 关卡 .lua 路径统一走资源索引：category="maps"、id="level<N>"。
    // 原实现在此遍历 3 个候选前缀（"levels/"、"joy_topdown_template/levels/"、"../joy_topdown_template/levels/"），
    // 现由 AssetManager 单点解析（索引缺失时回退内置默认路径），路径真源只剩 resources.json 一处。
    char map_id[32];
    snprintf(map_id, sizeof(map_id), "level%d", level);
    const char* rel = AssetManager::GetInstance()
        ? AssetManager::GetInstance()->GetResourcePath(AssetManager::kCatMaps, map_id)
        : nullptr;
    if (!rel) { map_log_load("no-path", level); return false; }

    char path[256];
    snprintf(path, sizeof(path), "%s", rel);
    if (!map_tm_probe_path(path)) return false;

    jtilemap_p tm = jtilemap_load_file(path);
    if (!tm) return false;

    // 玩法网格尺寸以 tilemap 为准：小地图/碰撞/寻路/移动等系统共用这组动态宽高。
    int w = jtilemap_get_width(tm);
    int h = jtilemap_get_height(tm);
    if (w <= 0 || h <= 0 || w * h > MAP_MAX_CELLS) { jtilemap_destroy(tm); return false; }

    // 网格：先清为全通，再按碰撞对象层把墙格标为 '#'
    map_grid_clear(w, h);
    jtilemap_object_layer_p col_layer = map_find_collision_layer(tm);
    if (col_layer) {
        int tw = jtilemap_get_tilewidth(tm);
        int th = jtilemap_get_tileheight(tm);
        if (tw > 0 && th > 0) {
            map_collision_ctx_t cctx = { tw, th, MAP_W, MAP_H };
            jtilemap_object_layer_foreach(col_layer, map_rasterize_collision_rect, &cctx);
        }
    }

    // 出生/敌人点：Player/spawn 对象层（对象坐标为 Tiled 像素，用 tilewidth 换算成格子）
    const int tw = jtilemap_get_tilewidth(tm);
    const int th = jtilemap_get_tileheight(tm);
    g_map_info.player_cell = -1;
    g_map_info.enemy_n = 0;
    if (jtilemap_object_layer_p pl = map_find_spawn_layer(tm)) {
        map_spawn_ctx_t sctx;
        sctx.tw = (tw > 0) ? tw : 60;
        sctx.th = (th > 0) ? th : 60;
        sctx.mw = MAP_W;
        sctx.mh = MAP_H;
        jtilemap_object_layer_foreach_all(pl, map_parse_spawn_obj, &sctx);
        g_map_info.player_cell = sctx.player_cell;
        g_map_info.enemy_n = sctx.enemy_n;
        std::memcpy(g_map_info.enemy_cells, sctx.enemy_cells, (size_t)sctx.enemy_n * sizeof(int));
    }

    // 敌人点：独立的 Enemy 层（地图里也可能误拼为 "Emeny"）。该层内所有对象都视作敌人，
    // 忽略对象自身 type（地图导出时 type 常为空），与上面 spawn 层里 type=="enemy" 的对象合并计入。
    if (jtilemap_object_layer_p el = map_find_enemy_layer(tm)) {
        map_spawn_ctx_t ectx;
        ectx.tw = (tw > 0) ? tw : 60;
        ectx.th = (th > 0) ? th : 60;
        ectx.mw = MAP_W;
        ectx.mh = MAP_H;
        ectx.force_enemy = true;
        jtilemap_object_layer_foreach_all(el, map_parse_spawn_obj, &ectx);
        for (int i = 0; i < ectx.enemy_n && g_map_info.enemy_n < 256; ++i)
            g_map_info.enemy_cells[g_map_info.enemy_n++] = ectx.enemy_cells[i];
    }

    g_map_info.tilemap = tm;   // 玩法数据源与渲染源合一；切关时由 map_load_level 销毁
    return true;
}

static inline void map_load_level(int level)
{
    if (level < 1 || level > LEVEL_COUNT) level = 1;
    if (g_map_info.level == level) return;

    // 切关时释放上一关的 jtilemap
    if (g_map_info.tilemap) { jtilemap_destroy(g_map_info.tilemap); g_map_info.tilemap = nullptr; }

    if (g_map_info.tilemap_render) { jtilemap_render_destroy(g_map_info.tilemap_render); g_map_info.tilemap_render = nullptr; }
    g_map_info = {};                    // 清零（enemy_cells 归零）
    g_map_info.level = level;

    // 唯一数据源：Tiled 导出的 levelN.lua。缺失/解析失败即视为"该关未制作"，
    // 由选关界面（level_map_available）拦截并提示，不会带着空图进游戏。
    if (map_load_from_tilemap(level)) { map_log_load("tilemap", level); return; }
    map_log_load("missing", level);
}

// 获取指定格子的地形字符（越界一律视为墙）
static inline char map_grid_at(int x, int y)
{
    if (x < 0 || y < 0 || x >= MAP_W || y >= MAP_H) return '#';
    return g_map_grid[y * MAP_W + x];
}
// 兼容旧名
static inline char map_tile_at(int x, int y) { return map_grid_at(x, y); }

static inline int map_tile_type(char c)
{
    switch (c) {
    case '#': return MAP_TILE_WALL;
    default:  return MAP_TILE_EMPTY;
    }
}

// 该地形是否参与碰撞（关卡中仅墙碰撞）
static inline bool map_is_collidable(char c)
{
    return c == '#';
}

// 日志：诊断地图加载结果写到 map_load_debug.log
static void map_log_load(const char* source, int level)
{
    FILE* f = std::fopen("map_load_debug.log", "a");
    if (!f) return;
    fprintf(f, "[%s] level=%d enemy_n=%d player_cell=%d\n",
        source, level, g_map_info.enemy_n, g_map_info.player_cell);
    if (g_map_info.player_cell >= 0) {
        int cx = g_map_info.player_cell % MAP_W;
        int cy = g_map_info.player_cell / MAP_W;
        int walls = 0;
        for (int dy = -1; dy <= 1; dy++)
            for (int dx = -1; dx <= 1; dx++)
                walls += map_is_collidable(map_grid_at(cx + dx, cy + dy));
        fprintf(f, " => player @(%d,%d) 3x3 walls=%d\n", cx, cy, walls);
    }
    int n_walls = 0;
    for (int y = 0; y < MAP_H; y++)
        for (int x = 0; x < MAP_W; x++)
            n_walls += map_is_collidable(map_grid_at(x, y));
    fprintf(f, " => total walls: %d\n\n", n_walls);
    fclose(f);
}

// 玩家出生点：'P' 格中心（世界米坐标），缺失回退 (0.5,0.5)
static inline void map_player_spawn(int level, fp_t* fx, fp_t* fy)
{
    if (g_map_info.level != level) map_load_level(level);
    if (g_map_info.player_cell < 0) { *fx = fp_from_float(0.5f); *fy = fp_from_float(0.5f); return; }
    *fx = fp_from_float((float)(g_map_info.player_cell % MAP_W) + 0.5f);
    *fy = fp_from_float((float)(g_map_info.player_cell / MAP_W) + 0.5f);
}

// ============================================================
// Scribble Dungeons 像素地形映射层（纯渲染，不改碰撞/逻辑/帧同步）
// 图集 Tilesheet/tilesheet.png（Kenney Scribble Dungeons，每块 64×64）14 列按行递增，
// GID=g => col=(g-1)%14, row=(g-1)/14；源像素坐标 (src_x,src_y)=col*64,row*64。
// 坐标依据官方 sampleMap.tmx 校准：大面积室内地板用 GID8，墙体用 GID3 墙件。
// 电平网格仅含 墙(#)/地板(.)，其余类型为保留项（关卡不生成）。
// ============================================================
struct TileSprite {
    int src_x = 0, src_y = 0;   // atlas 内该 64×64 瓦片的左上角像素坐标
};

// 图集单块尺寸（64×64）
static constexpr int TILE_ATLAS_BLK = 64;

// 渲染用映射：MapTileType → Tilesheet 图块源坐标（64×64，官方 sampleMap 校准）
static inline TileSprite tile_sprite_of(int type)
{
    TileSprite s;
    switch (type) {
    case MAP_TILE_WALL:  s.src_x = TILE_ATLAS_BLK * 3; s.src_y = 0;                 break; // 墙  GID4  (col3,row0) 竖纹墙身，明显区分地板
    case MAP_TILE_ROCK:  s.src_x = TILE_ATLAS_BLK * 5; s.src_y = 0;                 break; // 石/岩 (col5,row0 保留)
    case MAP_TILE_WATER: s.src_x = TILE_ATLAS_BLK * 9; s.src_y = TILE_ATLAS_BLK * 2; break; // 水   (col9,row2 保留)
    case MAP_TILE_GRASS: s.src_x = TILE_ATLAS_BLK * 8; s.src_y = 0;                 break; // 草   (col8,row0 保留)
    case MAP_TILE_FLOWER:s.src_x = TILE_ATLAS_BLK * 9; s.src_y = TILE_ATLAS_BLK * 4; break; // 花   (col9,row4 保留)
    case MAP_TILE_PATH:  s.src_x = TILE_ATLAS_BLK * 1; s.src_y = TILE_ATLAS_BLK * 1; break; // 路   (col1,row1 保留)
    case MAP_TILE_TREE:  s.src_x = TILE_ATLAS_BLK * 3; s.src_y = TILE_ATLAS_BLK * 1; break; // 树   (col3,row1 保留)
    case MAP_TILE_EMPTY:
    default:             s.src_x = TILE_ATLAS_BLK * 1; s.src_y = 0;                 break; // 地板 GID2  (col1,row0) 官方 Ground 大面积用的浅色实心底
    }
    return s;
}

#endif