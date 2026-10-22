#ifndef BULLET_COMPONENT_H
#define BULLET_COMPONENT_H

#include <joy/jmath.h>

// 箭贴图实际可见尺寸（64px 格子内仅 10×44 非透明像素；64px = 1 米）：
//   宽 10/64 = 0.15625 米，长 44/64 = 0.6875 米。
// 碰撞盒对齐贴图：沿飞行方向(长) BULLET_HIT_LEN、垂直方向(短) BULLET_HIT_WID。
static constexpr float BULLET_HIT_LEN = 0.6875f;
static constexpr float BULLET_HIT_WID = 0.15625f;

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
