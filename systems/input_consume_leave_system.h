#ifndef INPUT_CONSUME_LEAVE_SYSTEM_H
#define INPUT_CONSUME_LEAVE_SYSTEM_H

struct Context;

static void input_consume_leave_system(entt::registry& registry, Context* ctx, float /*dt*/) {
	auto& net = registry.ctx().get<NetComponent>();
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
