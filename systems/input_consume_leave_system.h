#ifndef INPUT_CONSUME_LEAVE_SYSTEM_H
#define INPUT_CONSUME_LEAVE_SYSTEM_H

#include "../world/frame_command_buffer.h"
#include "../components/player_component.h"

struct Context;

static void input_consume_leave_system(entt::registry& registry, Context* ctx, float /*dt*/) {
	auto& net = registry.ctx().get<FrameCommandBuffer>();
	if (net.frame_id < 0) return;

	auto view = registry.view<PlayerComponent>();
	for (int i = 0; i < net.num_leaves; i++) {
		auto& leave = net.leaves[i];
		for (auto e : view) {
			if (registry.get<PlayerComponent>(e).conv == leave.conv) {
				registry.destroy(e);
				break;
			}
		}
	}
	net.num_leaves = 0;
}

#endif
