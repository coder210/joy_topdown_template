#ifndef KNOCKBACK_COMPONENT_H
#define KNOCKBACK_COMPONENT_H
#include <joy/jmath.h>

// 击退效果：被命中时由 melee/bullet 系统写入，knockback_system 每 tick 施加一次
// 位移并衰减。与 AI/寻路/输入的速度通道完全独立，不会被覆盖（否则敌人像纸片挨打不动）。
struct KnockbackComponent {
    fp_t vx, vy;   // 剩余击退速度（米/秒，定点）
    int  ticks;    // 剩余持续 tick 数（每 tick 乘一次衰减）
};

// 击退参数
static const float KNOCKBACK_SPEED = 6.0f;   // 初速度（敌人常态约 2.5 m/s，明显更猛）
static const int   KNOCKBACK_TICKS = 8;      // 持续约 0.4s @20Hz
static const float KNOCKBACK_DECAY = 0.8f;   // 每 tick 速度 ×0.8

#endif
