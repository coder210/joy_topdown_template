#ifndef PLAYER_RAY_SYSTEM_H
#define PLAYER_RAY_SYSTEM_H

#include "../components/player_event_input.h"
#include "../components/action_state_component.h"
#include "../components/player_component.h"
#include "../components/enemy_component.h"
#include "../components/health_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include <algorithm>

struct Context;

// 玩家射线（hitscan）大招参数
static const fp_t RAY_RANGE   = fp_from_int(10);  // 射线长度（米）
static const int  RAY_DAMAGE  = 35;               // ENEMY_MAX_HP=50，近两发带走

// 玩家"线"大招：完整点击 → 沿当前朝向发射一条即时命中（hitscan）射线，
// 命中射程内沿朝向最近的存活敌人（对齐 3D：单目标锁敌），扣血并进入冷却。
// 由 melee_attack_system 每 tick 递减 ray_cooldown 同款逻辑改为本系统自减。
// 帧同步确定性：纯定点数 + 固定迭代序选最近目标，两端结果一致，无需网络同步。
// 伤害仅扣 HP 并钳制到 0，是否置 dead 由随后的 player_death_system 统一判定。
static void player_ray_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    (void)ctx;
    auto view = registry.view<PlayerComponent, PlayerEventInput, ActionStateComponent,
                              LogicPositionComponent, LogicRectComponent>();
    auto enemy_view = registry.view<EnemyComponent, LogicPositionComponent,
                                    LogicRectComponent, HealthComponent>();

    for (auto e : view) {
        auto& act = view.get<ActionStateComponent>(e);
        // 冷却自然流逝（无条件，无论是否触发）
        if (act.ray_cooldown > 0) act.ray_cooldown -= 1;

        // 只消费射线事件；近战/射击事件由各自系统处理，这里仅移除射线事件
        auto erase_ray = [](std::vector<PlayerEventInput::Action>& v) {
            v.erase(std::remove_if(v.begin(), v.end(),
                [](const PlayerEventInput::Action& a){ return a.type == PlayerEventInput::ATTACK_RAY; }),
                v.end());
        };
        auto& events = view.get<PlayerEventInput>(e);
        bool want_ray = false;
        for (auto& a : events.pending_actions)
            if (a.type == PlayerEventInput::ATTACK_RAY) { want_ray = true; break; }
        if (!want_ray) continue;

        // 死亡玩家不能放线
        if (registry.all_of<HealthComponent>(e) && registry.get<HealthComponent>(e).dead) {
            erase_ray(events.pending_actions); continue;
        }
        if (act.ray_cooldown > 0) { erase_ray(events.pending_actions); continue; }

        act.ray_cooldown = ActionStateComponent::RAY_COOLDOWN;

        const fp_t half = fp_from_int(2);
        auto& pos  = view.get<LogicPositionComponent>(e);
        auto& rect = view.get<LogicRectComponent>(e);
        const fp_t ax = pos.x, ay = pos.y, aw = rect.width, ah = rect.height;
        const fp_t half_x = fp_add(ax, fp_div(aw, half));   // 玩家碰撞盒中心
        const fp_t half_y = fp_add(ay, fp_div(ah, half));

        // 沿朝向把玩家盒延长 RAY_RANGE 成射线光束 AABB（纯定点数）
        fp_t bx0 = ax, by0 = ay, bw = aw, bh = ah;
        switch (act.dir) {
            case 0: by0 = ay;            bh = fp_add(ah, RAY_RANGE); break;  // 下
            case 1: bx0 = fp_sub(ax, RAY_RANGE); bw = fp_add(aw, RAY_RANGE); break;  // 左
            case 2: bx0 = ax;            bw = fp_add(aw, RAY_RANGE); break;  // 右
            default: by0 = fp_sub(ay, RAY_RANGE); bh = fp_add(ah, RAY_RANGE); break; // 上
        }

        // 锁定沿朝向最近的目标（距离沿朝向轴度量；迭代序固定，双端一致）
        entt::entity lock = entt::null;
        fp_t lock_d = fp_from_float(1e9f);
        for (auto t : enemy_view) {
            auto& th = enemy_view.get<HealthComponent>(t);
            if (th.dead) continue;
            auto& tp = enemy_view.get<LogicPositionComponent>(t);
            auto& tr = enemy_view.get<LogicRectComponent>(t);
            const fp_t tx = tp.x, ty = tp.y, tw = tr.width, thh = tr.height;
            // 敌人盒与光束 AABB 相交
            if (bx0 < fp_add(tx, tw) && fp_add(bx0, bw) > tx &&
                by0 < fp_add(ty, thh) && fp_add(by0, bh) > ty) {
                fp_t d;
                if (act.dir == 0 || act.dir == 3) {
                    fp_t cy = fp_add(ty, fp_div(thh, half));
                    d = fp_abs(fp_sub(cy, half_y));
                } else {
                    fp_t cx = fp_add(tx, fp_div(tw, half));
                    d = fp_abs(fp_sub(cx, half_x));
                }
                if (d < lock_d) { lock = t; lock_d = d; }
            }
        }

        if (lock != entt::null) {
            auto& hp = enemy_view.get<HealthComponent>(lock);
            if (hp.hp > 0) {
                hp.hp -= RAY_DAMAGE;
                if (hp.hp < 0) hp.hp = 0;   // dead 置位由 player_death_system 统一定夺
            }
        }

        erase_ray(events.pending_actions);
    }
}

#endif