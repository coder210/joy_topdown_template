#ifndef CAPTURE_PREV_POSITION_SYSTEM_H
#define CAPTURE_PREV_POSITION_SYSTEM_H
#include "../components/logic_position_component.h"

struct Context;

// 逻辑帧开始时，把"当前逻辑位置"存为"上一帧位置"，供 interpolate_pass 在渲染帧按节拍余数插值。
// 必须注册在 PreUpdate 最前（早于 movement / ai_system / pathfinding），
// 这样 prev 是"本帧移动前"的位置，curr（移动后）才是"本帧结果"，插值才正确。
//
// 说明：本系统只写 prev_x/prev_y，不写 x/y（位置由移动/AI 系统负责）；
// 实体在 OnStart/中途生成时由 LogicPositionComponent 的 2 参构造把 prev 初始化为等于 curr，
// 所以首帧或刚生成的实体不会有"从原点滑入"的异常。
static void capture_prev_position_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
        (void)ctx;
        auto view = registry.view<LogicPositionComponent>();
        for (auto e : view) {
                auto& lpc = view.get<LogicPositionComponent>(e);
                lpc.prev_x = lpc.x;
                lpc.prev_y = lpc.y;
        }
}

#endif
