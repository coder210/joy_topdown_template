#ifndef INPUT_CONSUME_INPUT_SYSTEM_H
#define INPUT_CONSUME_INPUT_SYSTEM_H

#include "../components/action_state_component.h"
#include "../components/health_component.h"
#include "../components/player_state_input.h"
#include "../components/player_event_input.h"
#include "../protocol.h"

struct Context;

static void input_consume_input_system(entt::registry& registry, Context* ctx, float /*dt*/) {
	auto& net = registry.ctx().get<NetComponent>();
	if (net.frame_id < 0) return;

	// 攻击计时每逻辑 tick 无条件递减（与 ai_system 敌人部分对齐）。
	// 原实现只在"本帧有输入"时才递减，导致玩家按一次攻击后停手，
	// attack_timer 永久卡住、攻击状态永不退出、挥砍动画无法推进。
	auto pv = registry.view<PlayerComponent, ActionStateComponent>();
	for (auto pe : pv) {
		auto& pact = pv.get<ActionStateComponent>(pe);
		if (pact.attack_timer > 0) pact.attack_timer -= 1;
		if (pact.knockback_timer > 0) pact.knockback_timer -= 1;

		// 兜底：本帧没有该玩家的输入即视为静止（松开方向键 / 未攻击）。
		// 输入收集端在 dir/attack/ranged/ray 全为 0 时不发送输入（见
		// NetworkManager::CollectLocalInput / SendInput），若此处不兜底，
		// act.state 会滞留上一次的 WALK（或 ATTACK），动画永远停在 run。
		// 下方“本帧有输入”的分支会覆盖这里的值。
		pact.state = (pact.attack_timer > 0) ? ActionStateComponent::ATTACK
		                                     : ActionStateComponent::IDLE;
	}

	auto view = registry.view<PlayerComponent>();
	for (int i = 0; i < net.num_inputs; i++) {
		auto& input = net.inputs[i];

		entt::entity player = entt::null;
		for (auto e : view) {
			if (registry.get<PlayerComponent>(e).conv == input.conv) {
				player = e;
				break;
			}
		}
		if (player == entt::null) continue;

		// 死亡期间不消费输入：尸体不转向、不出招、不留移动位。
		// 帧同步确定性：dead 由两端同一 tick 置位，故此处判定两端一致
		if (registry.all_of<HealthComponent>(player) &&
		    registry.get<HealthComponent>(player).dead) continue;

		PlayerStateInput state_input;
		state_input.move_bits = input.dir;

		// log_info("Raw dir: %d\n", input.dir);

		if (registry.all_of<PlayerStateInput>(player)) {
			auto& state = registry.get<PlayerStateInput>(player);
			state.move_bits = state_input.move_bits;
		}
		else {
			registry.emplace<PlayerStateInput>(player, state_input);
		}

		auto push_action = [&](int type) {
			PlayerEventInput::Action a;
			a.client_seq = input.client_seq;
			a.type       = (uint8_t)type;
			if (registry.all_of<PlayerEventInput>(player)) {
				auto& ev = registry.get<PlayerEventInput>(player);
				ev.pending_actions.push_back(a);
			} else {
				PlayerEventInput ev;
				ev.pending_actions.push_back(a);
				registry.emplace<PlayerEventInput>(player, ev);
			}
		};
		// 攻/射/线：均为"完整点击"离散触发。射改单发（对齐 3D），线为大招 hitscan。
		if (input.attack) push_action(PlayerEventInput::ATTACK_MELEE);
		if (input.ranged) push_action(PlayerEventInput::ATTACK_BULLET);
		if (input.ray)    push_action(PlayerEventInput::ATTACK_RAY);
		// 切换武器：离散触发，按库存固定顺序轮转到下一把持有武器（空手/单把不移动）。
		if (input.weapon_switch) {
			auto& pc = registry.get<PlayerComponent>(player);
			pc_select_next_weapon(pc);
		}

		// 更新动作状态：attack_timer 驱动攻击持续（替代动画状态机复位），
		// 其余由移动位掩码决定 WALK/IDLE 与朝向
		if (registry.all_of<ActionStateComponent>(player)) {
			auto& act = registry.get<ActionStateComponent>(player);


			int  dir_idx = input_to_dir(input.dir);
			bool moving  = has_dir(input.dir);

			if (input.attack) {
				act.state        = ActionStateComponent::ATTACK;
				act.attack_timer = ActionStateComponent::ATTACK_DURATION;
				act.attack_dir   = moving ? dir_idx : act.dir;   // 锁定攻击起始朝向（站桩沿用当前朝向）
				if (moving) act.dir = dir_idx;
			} else {
				act.state = act.attack_timer > 0 ? ActionStateComponent::ATTACK
				                                 : (moving ? ActionStateComponent::WALK
				                                           : ActionStateComponent::IDLE);
				if (moving) act.dir = dir_idx;
			}
		}
	}
	net.num_inputs = 0;
}

#endif
