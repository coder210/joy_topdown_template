#ifndef WORLD_ENTITY_FACTORY_H
#define WORLD_ENTITY_FACTORY_H

#include "../context.h"
#include "../components/id_component.h"
#include "../components/player_component.h"
#include "../components/enemy_component.h"
#include "../components/health_component.h"
#include "../components/action_state_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/transform_component.h"
#include "../components/spawn_point_component.h"
#include "../components/ai_component.h"
#include "../systems/spawn_def.h"     // SpawnDef / map_player_spawn

// ============================================================
// 实体工厂：玩家 / 敌人实体的【唯一构造入口】。
//
// 背景：此前同一实体在 3 个地方各 emplace 一套组件
//   - input_consume_join_system.h（玩家加入）
//   - network_manager.cpp HandleLoading（快照恢复）
//   - spawn_enemy_system.h（敌人出生）
// 字段集合逐渐漂移（例如快照恢复的玩家没有武器，要靠后续 join 补丁），
// 故统一收敛到本文件。新增字段时只改这里，所有创建路径自动一致。
//
// 约定：除本文件外，任何地方都不要再手工 emplace 一整套玩家/敌人组件。
// ============================================================

// 敌人碰撞盒（对齐【可见贴图】尺寸，避免"箭还没碰到敌人就判命中消失"）。
// 依据 imp.png（64×64）实测：可见像素 bbox = (11,12)-(52,41)，即 42×30 px；
// 贴图按 kEnemyDrawH = 1 米高度绘制 -> 1 米 / 64 px。
//   可见宽 = 42/64 = 0.65625 m
//   可见高 = 30/64 = 0.46875 m
//   可见美术中心在图片 y=27（图片中线 y=32）-> 比实体中心高 5px = 0.078125 m
// 贴图若更换需重新测量这三个值。
static constexpr float ENEMY_HIT_W     = 0.65625f;
static constexpr float ENEMY_HIT_H     = 0.46875f;
static constexpr float ENEMY_HIT_OFF_Y = -0.078125f;   // 负值 = 碰撞盒相对实体中心上移

// 玩家碰撞盒（出生格中心居中）
static constexpr float PLAYER_HIT_W  = 0.8f;
static constexpr float PLAYER_HIT_H  = 0.8f;
static constexpr float PLAYER_HALF_W = 0.4f;
static constexpr float PLAYER_HALF_H = 0.4f;

// ---------------- 玩家 ----------------

// 玩家装配参数。render_x/render_y 直接是渲染中心（float），
// 保证 join 与快照恢复各自沿用原有公式、逐位等价。
struct PlayerSpawnDesc {
    int   entity_id = -1;                 // -1 = 从 GlobalState 自动分配
    int   conv = 0;
    int   char_id = 0;
    fp_t  box_x = 0, box_y = 0;           // 碰撞盒左上角（LogicPosition）
    fp_t  box_w = 0, box_h = 0;
    fp_t  spawn_x = 0, spawn_y = 0;       // 重生锚点（SpawnPoint）
    float render_x = 0.0f, render_y = 0.0f;
    int   hp = PLAYER_MAX_HP;
    int   max_hp = PLAYER_MAX_HP;
};

static entt::entity spawn_player(entt::registry& reg, Context* ctx, const PlayerSpawnDesc& d)
{
    entt::entity e = reg.create();
    const int eid = (d.entity_id >= 0) ? d.entity_id : get_entity_id(ctx);

    reg.emplace<IdComponent>(e, eid);
    reg.emplace<PlayerComponent>(e, d.conv, d.char_id);

    // 武器：按角色表授权（唯一入口；快照恢复的玩家也在此直接获得武器，
    // 不再依赖后续 join 广播的补丁路径）。pc_set_weapon 自身幂等。
    {
        auto& pc = reg.get<PlayerComponent>(e);
        const auto& cd = g_characterCatalog.get(d.char_id);
        pc_set_weapon(pc, cd.weapon.empty() ? nullptr : cd.weapon.c_str());
    }

    reg.emplace<LogicPositionComponent>(e, d.box_x, d.box_y);
    reg.emplace<LogicRectComponent>(e, d.box_w, d.box_h);
    reg.emplace<TransformComponent>(e, d.render_x, d.render_y, 0.0f, 1.0f, 1.0f);
    reg.emplace<ActionStateComponent>(e);
    reg.emplace<HealthComponent>(e, d.hp, d.max_hp, false, 0, SPAWN_INVULN_TICKS);
    reg.emplace<SpawnPointComponent>(e, d.spawn_x, d.spawn_y);
    return e;
}

// 加入路径：以出生格中心创建，碰撞盒按默认玩家尺寸居中。
static entt::entity spawn_player_on_join(entt::registry& reg, Context* ctx,
                                         int conv, int char_id,
                                         fp_t center_x, fp_t center_y)
{
    PlayerSpawnDesc d;
    d.conv = conv;
    d.char_id = char_id;
    d.box_w = fp_from_float(PLAYER_HIT_W);
    d.box_h = fp_from_float(PLAYER_HIT_H);
    d.box_x = fp_sub(center_x, fp_from_float(PLAYER_HALF_W));
    d.box_y = fp_sub(center_y, fp_from_float(PLAYER_HALF_H));
    d.spawn_x = center_x;
    d.spawn_y = center_y;
    d.render_x = fp_to_float(center_x);
    d.render_y = fp_to_float(center_y);
    return spawn_player(reg, ctx, d);
}

// 快照恢复路径：位置/尺寸/血量取服务端快照，重生锚点取当前关卡出生格。
static entt::entity spawn_player_from_snapshot(entt::registry& reg, Context* ctx,
                                               int entity_id, int conv, int char_id,
                                               fp_t box_x, fp_t box_y,
                                               fp_t box_w, fp_t box_h, int hp)
{
    PlayerSpawnDesc d;
    d.entity_id = entity_id;
    d.conv = conv;
    d.char_id = char_id;
    d.box_x = box_x;
    d.box_y = box_y;
    d.box_w = box_w;
    d.box_h = box_h;
    d.hp = hp;
    d.render_x = fp_to_float(box_x) + fp_to_float(box_w) * 0.5f;
    d.render_y = fp_to_float(box_y) + fp_to_float(box_h) * 0.5f;
    map_player_spawn(ctx->ecs.ctx().get<GlobalState>().level, &d.spawn_x, &d.spawn_y);
    return spawn_player(reg, ctx, d);
}

// ---------------- 敌人 ----------------

struct EnemySpawnDesc {
    int   entity_id = -1;                 // -1 = 从 GlobalState 自动分配
    int   spawn_idx = 0;
    const SpawnDef* def = nullptr;        // nullptr = 最小装配（无 Health/AI/SpawnPoint）
    fp_t  box_x = 0, box_y = 0;           // 碰撞盒左上角（LogicPosition）
    fp_t  box_w = 0, box_h = 0;
    float render_x = 0.0f, render_y = 0.0f;
    int   hp = ENEMY_MAX_HP;
    int   max_hp = ENEMY_MAX_HP;
    // AI 运行时状态（仅快照恢复时需要；否则从 SpawnDef 重新开始）
    bool  restore_ai = false;
    int   ai_timer = 0, ai_state = 0;
    fp_t  ai_target_x = 0, ai_target_y = 0;
};

static entt::entity spawn_enemy(entt::registry& reg, Context* ctx, const EnemySpawnDesc& d)
{
    entt::entity e = reg.create();
    const int eid = (d.entity_id >= 0) ? d.entity_id : get_entity_id(ctx);

    reg.emplace<IdComponent>(e, eid);
    reg.emplace<EnemyComponent>(e);
    reg.emplace<SpawnIdxComponent>(e, d.spawn_idx);
    reg.emplace<LogicPositionComponent>(e, d.box_x, d.box_y);
    reg.emplace<LogicRectComponent>(e, d.box_w, d.box_h);
    reg.emplace<TransformComponent>(e, d.render_x, d.render_y, 0.0f, 1.0f, 1.0f);
    reg.emplace<ActionStateComponent>(e);

    if (d.def) {
        reg.emplace<HealthComponent>(e, d.hp, d.max_hp);
        reg.emplace<SpawnPointComponent>(e, fp_from_float(d.def->x), fp_from_float(d.def->y));
        reg.emplace<AIComponent>(e,
            fp_from_float(d.def->x), fp_from_float(d.def->y),
            fp_from_float(d.def->patrol_radius),
            fp_from_float(d.def->detect_radius),
            fp_from_float(d.def->attack_radius));

        if (d.restore_ai) {
            // 恢复服务端下发的 AI 运行时状态（timer/state/target），
            // 否则持续累积的状态会从 0/出生点开始，与服务端接入时值不同步。
            auto& ai = reg.get<AIComponent>(e);
            ai.timer    = d.ai_timer;
            ai.state    = d.ai_state;
            ai.target_x = d.ai_target_x;
            ai.target_y = d.ai_target_y;
        }
    }
    return e;
}

// 初始生成（权威端）：位置由 SpawnDef 派生，Transform 取出生点美术中心。
static entt::entity spawn_enemy_at(entt::registry& reg, Context* ctx,
                                   const SpawnDef& def, int spawn_idx)
{
    EnemySpawnDesc d;
    d.spawn_idx = spawn_idx;
    d.def = &def;
    d.box_x = fp_from_float(def.x - ENEMY_HIT_W * 0.5f);
    d.box_y = fp_from_float(def.y + ENEMY_HIT_OFF_Y - ENEMY_HIT_H * 0.5f);
    d.box_w = fp_from_float(ENEMY_HIT_W);
    d.box_h = fp_from_float(ENEMY_HIT_H);
    d.render_x = def.x;
    d.render_y = def.y;
    return spawn_enemy(reg, ctx, d);
}

// 快照恢复（客户端）：位置/尺寸/血量取快照；AI 参数取 SpawnDef，运行时状态取快照。
static entt::entity spawn_enemy_from_snapshot(entt::registry& reg, Context* ctx,
                                              int entity_id, const SpawnDef* def, int spawn_idx,
                                              fp_t box_x, fp_t box_y,
                                              fp_t box_w, fp_t box_h, int hp,
                                              int ai_timer, int ai_state,
                                              fp_t ai_target_x, fp_t ai_target_y)
{
    EnemySpawnDesc d;
    d.entity_id = entity_id;
    d.spawn_idx = spawn_idx;
    d.def = def;
    d.box_x = box_x;
    d.box_y = box_y;
    d.box_w = box_w;
    d.box_h = box_h;
    d.render_x = fp_to_float(box_x) + fp_to_float(box_w) * 0.5f;
    d.render_y = fp_to_float(box_y) + fp_to_float(box_h) * 0.5f;
    d.hp = hp;
    d.restore_ai = true;
    d.ai_timer = ai_timer;
    d.ai_state = ai_state;
    d.ai_target_x = ai_target_x;
    d.ai_target_y = ai_target_y;
    return spawn_enemy(reg, ctx, d);
}

#endif
