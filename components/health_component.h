#ifndef HEALTH_COMPONENT_H
#define HEALTH_COMPONENT_H
#include <joy/jmath.h>

// 玩家初始血量（远程敌人子弹伤害结算用）
static constexpr int PLAYER_MAX_HP = 100;
// 敌人初始血量（玩家远程子弹伤害结算用）
static constexpr int ENEMY_MAX_HP = 50;
// 重生耗时（tick 数）：3 秒 @ 20Hz 逻辑 tick（帧同步计数，避免浮点累积误差）
static constexpr int PLAYER_RESPAWN_TICKS = 60;
// 重生无敌保护（tick 数）：2 秒 @ 20Hz。避免玩家重生回出生点时
// 被周围敌人（尤其加多敌人后）瞬间集火，导致"重生血量 0"的假象。
static constexpr int SPAWN_INVULN_TICKS = 40;

// 血量组件：子弹命中扣血，HP 通过快照同步给客户端 HUD 显示。
// 数值与计时均为整数（逻辑层不用浮点，保证两端确定性）。
// 出生点见 SpawnPointComponent（地图数据，与血量状态解耦）。
struct HealthComponent {
    int  hp;            // 当前血量
    int  max_hp;        // 最大血量
    bool dead;          // 死亡标记（帧同步确定性：两端同一 tick 置位/清除，不通过快照同步）
    int  respawn_timer; // 重生倒计时（tick 数），死亡期间由 player_death_system 每 tick 递减
    int  spawn_invuln;  // 重生无敌保护（tick 数），>0 时免疫伤害，由 player_death_system 每 tick 递减

    HealthComponent() = default;
    HealthComponent(int h, int m, bool d = false, int rt = 0, int si = 0)
        : hp(h), max_hp(m), dead(d), respawn_timer(rt), spawn_invuln(si) {}
};

#endif
