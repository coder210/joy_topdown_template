#ifndef PLAYER_DEATH_SYSTEM_H
#define PLAYER_DEATH_SYSTEM_H
#include "../components/health_component.h"
#include "../components/spawn_point_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/player_component.h"
#include "../components/enemy_component.h"
#include "../components/action_state_component.h"
#include "../components/player_state_input.h"
#include "../components/transform_component.h"
#include <vector>

struct Context;

// 玩家死亡 / 重生（帧同步确定性：两端同一 tick 判定与传送，无需网络同步）
// 注册在 melee_attack_system 之后：近战扣血 → 死亡判定 → 重生计时
static void player_death_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    auto view = registry.view<PlayerComponent, HealthComponent, LogicPositionComponent>();
    for (auto e : view) {
        auto& h   = view.get<HealthComponent>(e);
        auto& pos = view.get<LogicPositionComponent>(e);

        // 死亡判定：HP 归零（melee_attack_system 已把 HP 钳制在 0）
        if (!h.dead && h.hp <= 0) {
            h.dead = true;
            h.respawn_timer = PLAYER_RESPAWN_TICKS;
        }

        // 无敌保护倒计时（玩家可能未被判定死亡，但重生后有无敌窗口）
        if (h.spawn_invuln > 0) h.spawn_invuln -= 1;

        // 重生计时与传送回出生点
        if (h.dead) {
            h.respawn_timer -= 1;   // 每逻辑 tick 减 1
            if (h.respawn_timer <= 0) {
                h.dead = false;
                h.hp = h.max_hp;
                h.spawn_invuln = SPAWN_INVULN_TICKS;  // 重生无敌，防出生点被围殴"重生即死"
                const auto& sp = registry.get<SpawnPointComponent>(e);
                // SpawnPoint 存的是出生格中心；LogicPosition(左上角) = 中心 - 半宽高
                auto& rect = registry.get<LogicRectComponent>(e);
                pos.x = fp_sub(sp.x, fp_mul(rect.width, fp_half()));
                pos.y = fp_sub(sp.y, fp_mul(rect.height, fp_half()));
                // 清重生前残留输入/动作状态：
                // 防"死亡时按着方向键/出招中，重生瞬间自动冲出或摆出攻击姿势"
                if (registry.all_of<PlayerStateInput>(e))
                    registry.get<PlayerStateInput>(e).move_bits = 0;
                if (registry.all_of<ActionStateComponent>(e)) {
                    auto& act = registry.get<ActionStateComponent>(e);
                    act.state           = ActionStateComponent::IDLE;
                    act.attack_timer    = 0;
                    act.melee_cooldown  = 0;   // 清近战冷却，防重生瞬间立刻挥击
                }
                // 同步渲染坐标（碰撞盒中心 = 出生格中心），避免插值缓慢漂移
                if (registry.all_of<TransformComponent>(e)) {
                    auto& t    = registry.get<TransformComponent>(e);
                    const auto& sp = registry.get<SpawnPointComponent>(e);
                    t.position.x = fp_to_float(sp.x);
                    t.position.y = fp_to_float(sp.y);
                }
            }
        }
    }

    // 敌人死亡：HP 归零即销毁实体（不保留尸体）。
    // 该销毁在两端同 tick 发生（死亡判定确定性一致 → 帧同步两端一致）；
    // "补满到目标数"由权威端 enemy_maintain_system 负责（生成新敌人并经快照下发）。
    {
        std::vector<entt::entity> dead_enemies;
        auto eview = registry.view<EnemyComponent, HealthComponent>();
        for (auto e : eview) {
            if (eview.get<HealthComponent>(e).hp <= 0)
                dead_enemies.push_back(e);
        }
        for (auto e : dead_enemies) registry.destroy(e);
    }
}

#endif
