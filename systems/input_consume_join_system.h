#ifndef INPUT_CONSUME_JOIN_SYSTEM_H
#define INPUT_CONSUME_JOIN_SYSTEM_H

#include "../components/global_state_component.h"
#include "../components/action_state_component.h"
#include "../components/health_component.h"
#include "../components/spawn_point_component.h"
#include "../systems/character_catalog.h"
#include "../systems/weapon_catalog.h"

struct Context;

static void input_consume_join_system(entt::registry& registry, Context* ctx, float /*dt*/) {
	auto& net = registry.ctx().get<NetComponent>();
	if (net.frame_id < 0) return;

	auto& gs = registry.ctx().get<GlobalState>();

	for (int i = 0; i < net.num_joins; i++) {
		log_info("creating player %d", net.num_joins);
		auto& join = net.joins[i];

		// 该 conv 玩家可能已由加载快照创建（client 端）：不重复创建，仅刷新出生点
		bool exists = false;
		auto pview = registry.view<PlayerComponent>();
		for (auto pe : pview) {
			if (registry.get<PlayerComponent>(pe).conv == join.conv) {
				// 已有实体：仅刷新出生点（重连/重新加入场景）
				if (registry.all_of<HealthComponent>(pe)) {
					auto& sp = registry.get<SpawnPointComponent>(pe);
					sp.x = join.position_x;
					sp.y = join.position_y;
				}
				// 快照重建的玩家若尚未持有武器（重连/晚加入），按角色类型授权（幂等）。
				{
				    auto& pc = registry.get<PlayerComponent>(pe);
				    if (pc.weapon_id < 0) {
				        const auto& cd = g_characterCatalog.get(join.char_id);
				        pc_set_weapon(pc, cd.weapon.empty() ? nullptr : cd.weapon.c_str());
				    }
				}
				exists = true;
				break;
			}
		}
		if (exists) continue;

		entt::entity player = registry.create();
		registry.emplace<IdComponent>(player, get_entity_id(ctx));
		registry.emplace<PlayerComponent>(player, join.conv, join.char_id);

		// 角色固定持有的武器：由 characters.json 的 weapon 字段决定（单一，无切换）。
		// 角色类型两端加载同一份文件 → 结果一致，帧同步安全。
		// 查不到（键拼错 / 未登记进武器表）则空手（weapon_id=-1）。
		{
		    auto& pc = registry.get<PlayerComponent>(player);
		    const auto& def = g_characterCatalog.get(join.char_id);
		    pc_set_weapon(pc, def.weapon.empty() ? nullptr : def.weapon.c_str());
		}
		// join.position 是出生格中心；碰撞盒 0.8×0.8 以出生点居中，
		// LogicPosition(左上角) = 中心 - 半宽(0.4)，Transform(渲染中心) = 出生点
		registry.emplace<LogicPositionComponent>(player, fp_sub(join.position_x, fp_from_float(0.4f)), fp_sub(join.position_y, fp_from_float(0.4f)));
		registry.emplace<LogicRectComponent>(player, fp_from_float(0.8f), fp_from_float(0.8f));
		registry.emplace<TransformComponent>(player, fp_to_float(join.position_x), fp_to_float(join.position_y), 0.0f, 1.0f, 1.0f);

		// 挂载动作状态组件（几何渲染的朝向 / 攻击表现依赖它）
		registry.emplace<ActionStateComponent>(player);

		// 挂载血量组件（远程敌人子弹命中扣血）；初次加入也带重生无敌，防出生点被围殴
		registry.emplace<HealthComponent>(player, PLAYER_MAX_HP, PLAYER_MAX_HP,
			false, 0, SPAWN_INVULN_TICKS);
		// 出生点 = join 位置（重生传送目标）
		registry.emplace<SpawnPointComponent>(player, join.position_x, join.position_y);

		// 首次加入的玩家作为摄像机跟随目标
		if (gs.camera_follow_conv < 0) {
			gs.camera_follow_conv = join.conv;
		}
	}
	net.num_joins = 0;
}

#endif
