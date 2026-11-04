#ifndef SPAWN_ENEMY_SYSTEM_H
#define SPAWN_ENEMY_SYSTEM_H

#include "../context.h"
#include "../world/global_state.h"
#include "../world/entity_factory.h"   // spawn_enemy_at / ENEMY_HIT_*
#include "../systems/spawn_def.h"

// 单个敌人实体的创建：从给定 SpawnDef 在出生点生成。
// 实体装配已收敛到 world/entity_factory.h，保证初始生成与快照恢复/维持补充三处一致。
static void spawn_one_enemy(entt::registry& registry, Context* ctx, const SpawnDef& s, int spawn_idx) {
    spawn_enemy_at(registry, ctx, s, spawn_idx);
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
