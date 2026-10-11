#ifndef INTERPOLATE_PASS_H
#define INTERPOLATE_PASS_H
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/transform_component.h"
#include "../components/id_component.h"

struct Context;

static void interpolate_pass(entt::registry& registry, Context* /*ctx*/, float /*dt*/)
{
    registry.view<IdComponent, LogicPositionComponent, LogicRectComponent, TransformComponent>().each(
        [](IdComponent& /*id*/, LogicPositionComponent& lp, LogicRectComponent& rect, TransformComponent& t)
        {
            float target_x = fp_to_float(lp.x) + fp_to_float(rect.width) * 0.5f;
            float target_y = fp_to_float(lp.y) + fp_to_float(rect.height) * 0.5f;
            t.position.x = target_x;
            t.position.y = target_y;
        });
}

#endif