#ifndef MAP_SPAWN_SYSTEM_H
#define MAP_SPAWN_SYSTEM_H

#include "../context.h"
#include "map_def.h"
#include "../components/local_only_component.h"

// 在 OnStart 阶段运行，根据 MAP_LAYOUT 创建静态碰撞地形实体。
// 这些实体不带 Player/Enemy 标记，会被 collision_system 视为碰撞体。
// 带 LocalOnlyComponent，因此不会被 snapshot_system 打包同步（地图由客户端本地生成，
// 两端共享同一份 MAP_LAYOUT，保证初始碰撞条件一致）。
// 注意：墙不挂 IdComponent、也不调用 get_entity_id —— 墙不进快照/不参与 checksum，id 无意义；
// 且若客户端调用 get_entity_id 会推进 server_entity_id，污染玩家 id 起点导致 checksum 错位。
static void map_spawn_system(entt::registry& registry, Context* ctx, float /*dt*/) {
    for (int y = 0; y < MAP_H; y++) {
        for (int x = 0; x < MAP_W; x++) {
            char c = map_grid_at(x, y);
            if (!map_is_collidable(c)) continue;

            float px = x + 0.5f; // 格子中心（米）
            float py = y + 0.5f;

            entt::entity e = registry.create();
            // 碰撞盒 = 整格 [x, x+1]，LogicPosition 取左上角（与玩家/敌人一致）
            registry.emplace<LogicPositionComponent>(e, fp_from_float(x), fp_from_float(y));
            registry.emplace<LogicRectComponent>(e, fp_from_float(1), fp_from_float(1));
            // 渲染原点 = 碰撞盒中心 = 格子中心
            registry.emplace<TransformComponent>(e, px, py, 0.0f, 1.0f, 1.0f);
            // 本地专用：不进快照、不跨网络同步
            registry.emplace<LocalOnlyComponent>(e);
        }
    }
}

#endif
