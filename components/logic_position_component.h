#ifndef LOGIC_POSITION_COMPONENT_H
#define LOGIC_POSITION_COMPONENT_H
#include <joy/jmath.h>

struct LogicPositionComponent
{
        fp_t x;        // 当前逻辑帧位置（左上角，世界单位）
        fp_t y;
        fp_t prev_x;   // 上一逻辑帧位置：用于渲染帧按节拍余数插值，消除 20Hz 步进的卡顿
        fp_t prev_y;
        LogicPositionComponent()
                : x(fp_from_float(0.0f)), y(fp_from_float(0.0f)),
                  prev_x(fp_from_float(0.0f)), prev_y(fp_from_float(0.0f)) {}
        LogicPositionComponent(fp_t ax, fp_t ay)
                : x(ax), y(ay), prev_x(ax), prev_y(ay) {}
};

#endif
