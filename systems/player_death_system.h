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

struct Context;

// 玩家死亡 / 重生（帧同步确定性：两端同一 tick 判定与传送，无需网络同步）
// 注册在 bullet_system 之后：子弹扣血 → 死亡判定 → 重生计时
static void player_death_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    auto view = registry.view<PlayerComponent, HealthComponent, LogicPositionComponent>();
    for (auto e : view) {
        auto& h   = view.get<HealthComponent>(e);
        auto& pos = view.get<LogicPositionComponent>(e);

        // 死亡判定：HP 归零（bullet_system 已把 HP 钳制在 0）
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
                    act.ranged_cooldown = 0;   // 清射击冷却，防重生瞬间立刻开火
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

    // 敌人死亡：标记死亡后保持死亡（不重生），达到"肃清全关"通关条件。
    // 敌人 body 保留在场景中（dead 标记），渲染/血环/寻路均跳过死亡敌人。
    auto eview = registry.view<EnemyComponent, HealthComponent>();
    for (auto e : eview) {
        auto& h = eview.get<HealthComponent>(e);
        if (!h.dead && h.hp <= 0) {
            h.dead = true;
        }
    }
}

#endif
