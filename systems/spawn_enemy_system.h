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
#include "../components/global_state_component.h"   // GlobalState / enemy_quota_remaining
#include "../systems/spawn_def.h"

// 敌人碰撞盒：对齐【可见贴图】尺寸，避免"箭还没碰到敌人就判命中消失"。
// 依据 imp.png（64×64）实测：可见像素 bbox = (11,12)-(52,41)，即 42×30 px；
// 贴图按 kEnemyDrawH = 1 米 高度绘制 -> 1 米 / 64 px。
//   可见宽 = 42/64 = 0.65625 m
//   可见高 = 30/64 = 0.46875 m
//   可见美术中心在图片 y=27（图片中线 y=32）-> 比实体中心高 5px = 0.078125 m
// 原来写死 0.8×0.8 且居中：左右各超出可见贴图 7px、上下各 17px，
// 叠加 0.078m 上移后下方差约 24px —— 箭从下方射来时会在离敌人一截处就判命中。
// 贴图若更换需重新测量这三个值。
static constexpr float ENEMY_HIT_W     = 0.65625f;
static constexpr float ENEMY_HIT_H     = 0.46875f;
static constexpr float ENEMY_HIT_OFF_Y = -0.078125f;   // 负值 = 碰撞盒相对实体中心上移

// 单个敌人实体的创建：从给定 SpawnDef 在出生点生成，并装配全部行为组件。
// 初始生成（spawn_enemy_system）与维持补充（enemy_maintain_system）共用，保证两份逻辑一致。
static void spawn_one_enemy(entt::registry& registry, Context* ctx, const SpawnDef& s, int spawn_idx) {
    entt::entity e = registry.create();
    registry.emplace<IdComponent>(e, get_entity_id(ctx));
    registry.emplace<EnemyComponent>(e);
    registry.emplace<SpawnIdxComponent>(e, spawn_idx);
    // 出生格中心 s.x/s.y；碰撞盒按可见贴图尺寸并随美术上移，
    // LogicPosition(左上角) = 盒中心 - 半宽/半高，Transform(渲染中心) = 出生点
    registry.emplace<LogicPositionComponent>(e,
        fp_from_float(s.x - ENEMY_HIT_W * 0.5f),
        fp_from_float(s.y + ENEMY_HIT_OFF_Y - ENEMY_HIT_H * 0.5f));
    registry.emplace<LogicRectComponent>(e,
        fp_from_float(ENEMY_HIT_W), fp_from_float(ENEMY_HIT_H));
    registry.emplace<TransformComponent>(e, s.x, s.y, 0.0f, 1.0f, 1.0f);

    registry.emplace<ActionStateComponent>(e);
    // 血量组件：玩家远程子弹命中扣血；死亡后由 player_death_system 销毁，交由 enemy_maintain_system 补充
    registry.emplace<HealthComponent>(e, ENEMY_MAX_HP, ENEMY_MAX_HP);
    registry.emplace<SpawnPointComponent>(e, fp_from_float(s.x), fp_from_float(s.y));
    registry.emplace<AIComponent>(e,
        fp_from_float(s.x), fp_from_float(s.y),  // patrol_center
        fp_from_float(s.patrol_radius),
        fp_from_float(s.detect_radius),
        fp_from_float(s.attack_radius)
    );
}

// 在 OnStart 阶段运行：初始化本关击杀配额，并生成首批敌人。
// 场上最多同时存活 MAINTAIN_ENEMY_COUNT 个；出生点不足时轮转复用。
// 后续由 enemy_maintain_system 死一个补一个，直到配额刷完。
static void spawn_enemy_system(entt::registry& registry, Context* ctx, float /*dt*/) {
    int count = 0;
    const SpawnDef* ENEMY_SPAWNS = level_enemy_defs(ctx->selected_level, count);

    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    if (gs.enemy_quota_remaining <= 0)
        gs.enemy_quota_remaining = level_enemy_total(ctx->selected_level);

    if (count <= 0) return;

    int n = MAINTAIN_ENEMY_COUNT;
    if (n > gs.enemy_quota_remaining) n = gs.enemy_quota_remaining;
    for (int i = 0; i < n; i++) {
        int idx = i % count;   // 出生点数量不足时轮转复用
        spawn_one_enemy(registry, ctx, ENEMY_SPAWNS[idx], idx);
    }
    gs.enemy_quota_remaining -= n;
}

#endif
