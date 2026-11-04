#ifndef INPUT_CONSUME_JOIN_SYSTEM_H
#define INPUT_CONSUME_JOIN_SYSTEM_H

#include "../world/global_state.h"
#include "../world/frame_command_buffer.h"
#include "../world/entity_factory.h"
#include "../components/health_component.h"
#include "../components/spawn_point_component.h"

struct Context;

static void input_consume_join_system(entt::registry& registry, Context* ctx, float /*dt*/) {
	auto& net = registry.ctx().get<FrameCommandBuffer>();
	if (net.frame_id < 0) return;

	auto& gs = registry.ctx().get<GlobalState>();

	for (int i = 0; i < net.num_joins; i++) {
		auto& join = net.joins[i];

		// 该 conv 玩家可能已由加载快照创建（client 端）：不重复创建，仅刷新出生点。
		// 武器等装配字段由实体工厂统一授予，快照重建时已一致，无需在此补丁。
		entt::entity existing = entt::null;
		auto pview = registry.view<PlayerComponent>();
		for (auto pe : pview) {
			if (registry.get<PlayerComponent>(pe).conv == join.conv) {
				existing = pe;
				break;
			}
		}
		if (existing != entt::null) {
			// 已有实体：仅刷新出生点（重连/重新加入场景）
			if (registry.all_of<SpawnPointComponent>(existing)) {
				auto& sp = registry.get<SpawnPointComponent>(existing);
				sp.x = join.position_x;
				sp.y = join.position_y;
			}
			continue;
		}

		// 唯一构造入口：创建玩家实体（Id/Player/武器/位置/碰撞盒/血量/出生点/动作态）
		spawn_player_on_join(registry, ctx, join.conv, join.char_id,
		                     join.position_x, join.position_y);

		// 首次加入的玩家作为摄像机跟随目标
		if (gs.camera_follow_conv < 0) {
			gs.camera_follow_conv = join.conv;
		}
	}
	net.num_joins = 0;
}

#endif
