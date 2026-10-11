#ifndef SPAWN_ENEMY_SYSTEM_H
#define SPAWN_ENEMY_SYSTEM_H

#include "../context.h"
#include "../components/enemy_component.h"
#include "../components/ai_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/transform_component.h"
#include "../components/action_state_component.h"
#include "../components/health_component.h"
#include "../components/spawn_point_component.h"
#include "../systems/spawn_def.h"

// 在 OnStart 阶段运行，按当前关卡 'E' 格确定性创建初始敌人实体
static void spawn_enemy_system(entt::registry& registry, Context* ctx, float /*dt*/) {
    int count = 0;
    const SpawnDef* ENEMY_SPAWNS = level_enemy_defs(ctx->selected_level, count);

    for (int i = 0; i < count; i++) {
        auto& s = ENEMY_SPAWNS[i];
        entt::entity e = registry.create();
        registry.emplace<IdComponent>(e, get_entity_id(ctx));
        registry.emplace<EnemyComponent>(e);
        registry.emplace<SpawnIdxComponent>(e, i);
        // 出生格中心 s.x/s.y；碰撞盒 0.8×0.8 以出生点居中，
        // LogicPosition(左上角) = 中心 - 半宽(0.4)，Transform(渲染中心) = 出生点
        registry.emplace<LogicPositionComponent>(e, fp_from_float(s.x - 0.4f), fp_from_float(s.y - 0.4f));
        registry.emplace<LogicRectComponent>(e, fp_from_float(0.8f), fp_from_float(0.8f));
        registry.emplace<TransformComponent>(e, s.x, s.y, 0.0f, 1.0f, 1.0f);

        registry.emplace<ActionStateComponent>(e);
        // 血量组件：玩家远程子弹命中扣血；死亡后重生回出生点
        registry.emplace<HealthComponent>(e, ENEMY_MAX_HP, ENEMY_MAX_HP);
        registry.emplace<SpawnPointComponent>(e, fp_from_float(s.x), fp_from_float(s.y));
        registry.emplace<AIComponent>(e,
            fp_from_float(s.x), fp_from_float(s.y),  // patrol_center
            fp_from_float(s.patrol_radius),
            fp_from_float(s.detect_radius),
            fp_from_float(s.attack_radius),
            s.ranged,                                   // 是否远程敌人
            fp_from_float(s.bullet_speed),              // 子弹速度
            fp_from_float(s.bullet_damage),             // 子弹伤害
            (int)(s.bullet_interval * 20.0f + 0.5f),    // 射击间隔（tick）
            0                                           // 射击冷却
        );
    }
}

#endif