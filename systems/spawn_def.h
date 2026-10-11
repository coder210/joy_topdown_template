#ifndef SPAWN_DEF_H
#define SPAWN_DEF_H

#include "map_def.h"

// 敌人生成位置和参数（服务端和客户端共享，保证初始条件一致）
// 位置对应 map_def.h 中的可通行格，避免敌人卡在障碍物里
struct SpawnDef {
    float x, y;           // 位置
    float patrol_radius;  // 巡逻半径
    float detect_radius;  // 检测范围
    float attack_radius;  // 攻击范围
    bool  ranged;         // 是否远程敌人
    float bullet_speed;   // 子弹速度（m/s）
    float bullet_interval;// 射击间隔（秒）
    float bullet_damage;  // 子弹伤害
};

// ============================================================
// 敌人出生：由当前关卡('E' 格)确定性派生 SpawnDef（与 3D 关卡对齐）。
// 服务端/客户端解析同一关卡 + 同一 i 规则 → 生成的出生列表逐位一致，
// 保证帧同步初态一致；ai_type(=SpawnIdxComponent.idx) 即该数组下标。
// ============================================================
constexpr int ENEMY_SPAWN_COUNT = 256;   // 出生表容量上限（C++11 constexpr 常量）

static inline const SpawnDef* level_enemy_defs(int level, int& count)
{
    if (g_map_info.level != level) map_load_level(level);
    if (g_map_info.enemy_n > ENEMY_SPAWN_COUNT) g_map_info.enemy_n = ENEMY_SPAWN_COUNT;
    count = g_map_info.enemy_n;

    static SpawnDef defs[ENEMY_SPAWN_COUNT];
    for (int i = 0; i < count; ++i) {
        int cell = g_map_info.enemy_cells[i];
        int cx = cell % MAP_W, cy = cell / MAP_W;
        bool ranged = (i % 2 == 0);   // 近战/远程交替混合
        SpawnDef d;
        d.x = (float)cx + 0.5f;
        d.y = (float)cy + 0.5f;
        d.patrol_radius  = ranged ? 2.0f : 3.0f;
        d.detect_radius  = ranged ? 10.0f : 6.0f;
        d.attack_radius  = ranged ? 6.0f : 1.5f;
        d.ranged         = ranged;
        d.bullet_speed   = ranged ? 6.0f : 0.0f;
        d.bullet_interval= ranged ? 1.5f : 0.0f;
        d.bullet_damage  = ranged ? 10.0f : 0.0f;
        defs[i] = d;
    }
    return defs;
}

#endif
