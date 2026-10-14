#ifndef ENEMY_MAINTAIN_SYSTEM_H
#define ENEMY_MAINTAIN_SYSTEM_H

#include <vector>
#include "../context.h"
#include "../components/enemy_component.h"
#include "../components/health_component.h"
#include "../components/spawn_point_component.h"
#include "../components/id_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/transform_component.h"
#include "../components/action_state_component.h"
#include "../components/ai_component.h"
#include "../systems/spawn_def.h"
#include "../systems/spawn_enemy_system.h"   // spawn_one_enemy
#include "../components/global_state_component.h"  // GlobalState（next_enemy_spawn_idx）

// 维持场景内存活敌人数 = MAINTAIN_ENEMY_COUNT（可配置，见 spawn_def.h）。
// 每 tick（仅权威端运行）：player_death_system 已销毁死亡敌人，这里统计存活数，
// 不足目标则从 Enemy 层出生点轮转补充，直到达到目标或耗尽出生点。
// 客户端不运行本系统，新敌人经快照下发，避免两端重复生成导致 id/数量错位。
static void enemy_maintain_system(entt::registry& registry, Context* ctx, float /*dt*/) {
    // 1) 统计当前存活敌人数（hp > 0 才算存活：子弹致死当 tick 的敌人 hp 已为 0 但 dead 标记
    //    要等下 tick 的 player_death 才清，用 hp>0 可让本 tick 即时补满，避免少一个的瞬态）。
    int alive = 0;
    {
        auto view = registry.view<EnemyComponent, HealthComponent>();
        for (auto e : view) {
            if (view.get<HealthComponent>(e).hp > 0) alive++;
        }
    }

    // 2) 目标受地图 Enemy 层出生点数约束
    int total_spawns = 0;
    const SpawnDef* defs = level_enemy_defs(ctx->selected_level, total_spawns);
    if (total_spawns > ENEMY_SPAWN_COUNT) total_spawns = ENEMY_SPAWN_COUNT;

    int target = MAINTAIN_ENEMY_COUNT;
    if (target > total_spawns) target = total_spawns;

    int need = target - alive;
    if (need <= 0 || total_spawns == 0) return;

    // 3) 从轮转出生点补充（游标存于 GlobalState，仅权威端推进，确定性一致）
    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    for (int i = 0; i < need; i++) {
        int idx = gs.next_enemy_spawn_idx % total_spawns;
        gs.next_enemy_spawn_idx = (gs.next_enemy_spawn_idx + 1) % total_spawns;
        spawn_one_enemy(registry, ctx, defs[idx], idx);
    }
}

#endif
