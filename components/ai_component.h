#ifndef AI_COMPONENT_H
#define AI_COMPONENT_H
#include <joy/jmath.h>

struct AIComponent {
    fp_t patrol_center_x, patrol_center_y; // 巡逻中心点
    fp_t patrol_radius;   // 巡逻范围
    fp_t detect_radius;   // 检测玩家范围
    fp_t attack_radius;   // 攻击范围
    int  state;           // 0=巡逻, 1=追击, 2=攻击
    int  timer;           // 计时器
    fp_t target_x, target_y; // 当前移动目标
    // 远程攻击参数（近战敌人由调用方传 false/0）
    bool ranged;          // 是否远程敌人
    fp_t bullet_speed;    // 子弹速度（m/s）
    fp_t bullet_damage;   // 子弹伤害
    int  shoot_interval;  // 射击间隔（tick）
    int  shoot_cooldown;  // 射击冷却倒计时

    AIComponent() = default;
    AIComponent(fp_t cx, fp_t cy, fp_t pr, fp_t dr, fp_t ar,
                bool rng, fp_t bs, fp_t bd, int si, int sc)
        : patrol_center_x(cx), patrol_center_y(cy), patrol_radius(pr),
          detect_radius(dr), attack_radius(ar), state(0), timer(0),
          target_x(cx), target_y(cy), ranged(rng), bullet_speed(bs),
          bullet_damage(bd), shoot_interval(si), shoot_cooldown(sc) {}
};

#endif
