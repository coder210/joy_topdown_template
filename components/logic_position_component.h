#ifndef LOGIC_POSITION_COMPONENT_H
#define LOGIC_POSITION_COMPONENT_H
#include <joy/jmath.h>

struct LogicPositionComponent
{
        joy_fp_t x;        // 当前逻辑帧位置（左上角，世界单位）
        joy_fp_t y;
        joy_fp_t prev_x;   // 上一逻辑帧位置：用于渲染帧按节拍余数插值，消除 20Hz 步进的卡顿
        joy_fp_t prev_y;
        LogicPositionComponent()
                : x(fp_from_float(0.0f)), y(fp_from_float(0.0f)),
                  prev_x(fp_from_float(0.0f)), prev_y(fp_from_float(0.0f)) {}
        LogicPositionComponent(joy_fp_t ax, joy_fp_t ay)
                : x(ax), y(ay), prev_x(ax), prev_y(ay) {}
};

#endif
