#ifndef AI_SYSTEM_H
#define AI_SYSTEM_H

#include "../components/ai_component.h"
#include "../components/enemy_component.h"
#include "../components/player_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/transform_component.h"
#include "../components/id_component.h"
#include "../components/action_state_component.h"
#include "../components/player_event_input.h"
#include "../components/pathfinding_component.h"
#include "../components/health_component.h"
#include "../components/global_state_component.h"
#include <cmath>

// 本模板为纯近战：敌人不再发射子弹，故无 spawn_enemy_bullet / BulletComponent 相关逻辑。

struct Context;

// 定点数向零截断转 int（等价于 (int)(x / 2^32) 的 C 语义），纯整数运算，帧同步确定性。
static inline int fp_trunc_to_int(fp_t x)
{
    int64_t q = x >> FP_FRACTIONAL_PLACES;   // 负数时偏小（floor 方向）
    if (x < 0 && (x & (FP_ONE - 1)) != 0) q += 1;  // 向零修正
    return (int)q;
}

static void ai_system(entt::registry& registry, Context* ctx, float /*dt*/) {
    fp_t step = fp_mul(fp_from_float(2.5f), fp_from_float(game_runtime(ctx).FIXED_TIMESTEP));

    auto view = registry.view<IdComponent, EnemyComponent, AIComponent, LogicPositionComponent, ActionStateComponent>();
    auto player_view = registry.view<PlayerComponent, LogicPositionComponent, HealthComponent>();
    auto& gs = ctx->ecs.ctx().get<GlobalState>();

    for (auto e : view) {
        auto& id =  view.get<IdComponent>(e);
        auto& ai  = view.get<AIComponent>(e);
        auto& pos = view.get<LogicPositionComponent>(e);
        auto& act = view.get<ActionStateComponent>(e);
        (void)id;

        // 死亡敌人不行动（不追击/不攻击/不巡逻），等待 player_death_system 重生
        if (registry.all_of<HealthComponent>(e) && registry.get<HealthComponent>(e).dead) {
            if (registry.all_of<PathComponent>(e)) registry.remove<PathComponent>(e);
            continue;
        }

        // 攻击计时递减（替代动画状态机的攻击复位），每逻辑 tick 减 1
        if (act.attack_timer > 0) act.attack_timer -= 1;

        // 寻找最近玩家
        bool found_player = false;
        fp_t nearest_dist_sq = fp_mul(ai.detect_radius, ai.detect_radius);
        fp_t nearest_px = fp_zero(), nearest_py = fp_zero();

        for (auto p : player_view) {
            // 死亡玩家不作为敌人攻击/追击目标（不鞭尸）
            if (player_view.get<HealthComponent>(p).dead) continue;
            auto& ppos = player_view.get<LogicPositionComponent>(p);
            fp_t dx = fp_sub(ppos.x, pos.x);
            fp_t dy = fp_sub(ppos.y, pos.y);
            fp_t dist_sq = fp_add(fp_mul(dx, dx), fp_mul(dy, dy));
            if (dist_sq < nearest_dist_sq) {
                nearest_dist_sq = dist_sq;
                found_player = true;
                nearest_px = ppos.x;
                nearest_py = ppos.y;
            }
        }

        // 攻击半径判断
        fp_t attack_dist_sq = fp_mul(ai.attack_radius, ai.attack_radius);
        bool in_attack_range = found_player && (nearest_dist_sq < attack_dist_sq);

        if (in_attack_range) {
            // 状态 2：攻击
            ai.state = 2;
            // 首次进入攻击范围时设置攻击计时器
            if (act.attack_timer == 0) {
                    act.attack_timer = ActionStateComponent::ATTACK_DURATION;
            }
            act.state = ActionStateComponent::ATTACK;

            if (!registry.all_of<PlayerEventInput>(e)) {
                registry.emplace<PlayerEventInput>(e);
            }
            auto& events = registry.get<PlayerEventInput>(e);
            events.pending_actions.push_back({ 0, 0 });

            fp_t dx = fp_sub(nearest_px, pos.x);
            fp_t dy = fp_sub(nearest_py, pos.y);
            act.dir = (fp_abs(dx) > fp_abs(dy))
                ? (dx > fp_zero() ? 2 : 1)
                : (dy > fp_zero() ? 0 : 3);
        }
        else if (found_player) {
            // 状态 1：追击（实际移动由 pathfinding_system 的 A* 完成）
            ai.state = 1;
            // 攻击计时未结束前保持攻击表现，结束后回到 WALK
            act.state = act.attack_timer > 0 ? ActionStateComponent::ATTACK
                                             : ActionStateComponent::WALK;

            // 记录最近玩家所在格子作为寻路目标，标记需要重算
            if (!registry.all_of<PathComponent>(e)) {
                registry.emplace<PathComponent>(e);
            }
            auto& pc = registry.get<PathComponent>(e);
            pc.target_gx = fp_trunc_to_int(nearest_px);
            pc.target_gy = fp_trunc_to_int(nearest_py);
            pc.dirty = true;

            fp_t dx = fp_sub(nearest_px, pos.x);
            fp_t dy = fp_sub(nearest_py, pos.y);
            act.dir = (fp_abs(dx) > fp_abs(dy))
                ? (dx > fp_zero() ? 2 : 1)
                : (dy > fp_zero() ? 0 : 3);

            ai.timer = 0;
        }
        else {
            // 状态 0：巡逻
            ai.state = 0;
            // 攻击计时未结束前保持攻击表现，结束后回到 WALK
            act.state = act.attack_timer > 0 ? ActionStateComponent::ATTACK
                                             : ActionStateComponent::WALK;

            // 巡逻目标更新逻辑（仅当 timer 归零时生成新目标）
            ai.timer--;
            if (ai.timer <= 0) {
                // 纯定点数生成巡逻偏移（帧同步确定性，两端同 seed → 同序列 → 行为一致）。
                // lcg_random_next 返回普通 uint32_t（[0,2^32)），经 fp_from_fraction 映射为 [0,1.0) 定点数。
                // 再 *2-1 得 [-1,1)，乘巡逻半径即得偏移（与原 float 版本偏移范围一致）。
                auto& gs = ctx->ecs.ctx().get<GlobalState>();
                uint32_t r1 = lcg_random_next(gs.random);
                uint32_t r2 = lcg_random_next(gs.random);
                fp_t fx = fp_sub(fp_mul(fp_from_fraction(r1), fp_from_float(2.0f)), fp_one());
                fp_t fy = fp_sub(fp_mul(fp_from_fraction(r2), fp_from_float(2.0f)), fp_one());
                fp_t rx = fp_mul(ai.patrol_radius, fx);
                fp_t ry = fp_mul(ai.patrol_radius, fy);
                ai.target_x = fp_add(ai.patrol_center_x, rx);
                ai.target_y = fp_add(ai.patrol_center_y, ry);
                ai.timer = 120;
            }

            fp_t dx = fp_sub(ai.target_x, pos.x);
            fp_t dy = fp_sub(ai.target_y, pos.y);
            // 距离阈值：小于0.2个单位则认为到达
            if (fp_abs(dx) > fp_from_float(0.2f) || fp_abs(dy) > fp_from_float(0.2f)) {
                    // 确保有 PathComponent
                    if (!registry.all_of<PathComponent>(e)) {
                            registry.emplace<PathComponent>(e);
                    }
                    auto& pc = registry.get<PathComponent>(e);
                    // 将目标点转换为网格坐标（格子大小 TILE_SIZE=1，纯整数向下取整，帧同步确定性）
                    pc.target_gx = fp_trunc_to_int(ai.target_x);
                    pc.target_gy = fp_trunc_to_int(ai.target_y);
                    pc.dirty = true;
            }
            else {
                    // 到达目标，清除路径组件（可选）
                    if (registry.all_of<PathComponent>(e)) {
                            registry.remove<PathComponent>(e);
                    }
            }

            act.dir = (fp_abs(dx) > fp_abs(dy))
                ? (dx > fp_zero() ? 2 : 1)
                : (dy > fp_zero() ? 0 : 3);
        }
    }
}

#endif