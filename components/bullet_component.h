#ifndef BULLET_COMPONENT_H
#define BULLET_COMPONENT_H

#include <joy/jmath.h>

// 箭/子弹逻辑组件：由 player_shoot_system 在发射时生成，bullet_system 每 tick 推进。
// 速度使用定点数（fp_t），与玩家/敌人运动同一套定点体系 -> 帧同步确定性。
struct BulletComponent {
    fp_t vx, vy;        // 速度（米/秒，定点数）
    int  damage;        // 命中伤害
    int  source_conv;   // 发射者 conv（多人对战时可避免误伤队友/自己，当前固定 0）
    int  life_ticks;    // 剩余存活 tick（到 0 销毁）
    int  from_player;   // 1=玩家发射 0=敌人（命中目标分类用）
};

#endif
