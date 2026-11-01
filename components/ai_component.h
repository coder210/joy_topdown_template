#ifndef AI_COMPONENT_H
#define AI_COMPONENT_H
#include <joy/jmath.h>

struct AIComponent {
    fp_t patrol_center_x, patrol_center_y; // 巡逻中心点
    fp_t patrol_radius;   // 巡逻范围
    fp_t detect_radius;   // 检测玩家范围
    fp_t attack_radius;   // 攻击范围（近战：接触距离）
    int  state;           // 0=巡逻, 1=追击, 2=攻击
    int  timer;           // 计时器
    fp_t target_x, target_y; // 当前移动目标

    AIComponent() = default;
    AIComponent(fp_t cx, fp_t cy, fp_t pr, fp_t dr, fp_t ar)
        : patrol_center_x(cx), patrol_center_y(cy), patrol_radius(pr),
          detect_radius(dr), attack_radius(ar), state(0), timer(0),
          target_x(cx), target_y(cy) {}
};

#endif
