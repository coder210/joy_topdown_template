#ifndef MELEE_ATTACK_SYSTEM_H
#define MELEE_ATTACK_SYSTEM_H

#include "../components/player_event_input.h"
#include "../components/action_state_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/player_component.h"
#include "../components/enemy_component.h"
#include "../components/ai_component.h"
#include "../components/health_component.h"
#include <algorithm>

struct Context;

// 近战攻击参数
static constexpr float PLAYER_MELEE_RANGE  = 1.0f;  // 玩家攻击盒向前延伸距离（米）
static constexpr int   PLAYER_MELEE_DAMAGE = 25;    // ENEMY_MAX_HP=50，两刀击杀
static constexpr int   ENEMY_MELEE_DAMAGE  = 20;    // PLAYER_MAX_HP=10000(测试临时值)，约500刀击杀

// 近战攻击结算：消费 PlayerEventInput.pending_actions（玩家由 input_consume_input_system
// 生产、敌人由 ai_system 生产）。攻击盒延伸方向优先用鼠标瞄准角 aim_yaw（与渲染视觉挥砍
// 一致），无瞄准数据时回退键盘朝向 act.dir；自身碰撞盒沿该方向向前延伸 range 做 AABB
// 命中判定并扣血，跳过死亡目标。帧同步确定性要求两端注册顺序一致：
// ai_system 之后、player_death 之前。
static void melee_attack_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    (void)ctx;
    auto view = registry.view<PlayerEventInput, ActionStateComponent,
                              LogicPositionComponent, LogicRectComponent>();
    auto player_view = registry.view<PlayerComponent, LogicPositionComponent,
                                     LogicRectComponent, HealthComponent>();
    auto enemy_view  = registry.view<EnemyComponent, LogicPositionComponent,
                                     LogicRectComponent, HealthComponent>();

    for (auto attacker : view) {
        auto& events = view.get<PlayerEventInput>(attacker);
        auto& act    = view.get<ActionStateComponent>(attacker);

        // 冷却自然流逝；近战事件来源（玩家点击 攻 / 敌人每 tick 推）由冷却节流
        if (act.melee_cooldown > 0) act.melee_cooldown -= 1;

        // 只处理近战类型事件；只移除近战事件，避免误清同一帧拍入的其它事件。
        auto erase_melee = [](std::vector<PlayerEventInput::Action>& v) {
            v.erase(std::remove_if(v.begin(), v.end(),
                [](const PlayerEventInput::Action& a){ return a.type == PlayerEventInput::ATTACK_MELEE; }),
                v.end());
        };
        bool want_melee = false;
        for (auto& a : events.pending_actions)
            if (a.type == PlayerEventInput::ATTACK_MELEE) { want_melee = true; break; }
        if (!want_melee) continue;

        bool is_player = registry.all_of<PlayerComponent>(attacker);

        // 玩家近战需持有近战武器（kind==1）。
        // 持有远程武器（kind==0）时不消费事件，放行交给 player_shoot_system 处理射击；
        // 空手（weapon_id<0 或 kind 既非 0 也非 1）则丢弃事件。
        if (is_player) {
            int wid = registry.get<PlayerComponent>(attacker).weapon_id;
            if (wid >= 0 && g_weaponCatalog.get(wid).kind != 1) {
                if (g_weaponCatalog.get(wid).kind == 0) continue;  // 远程：留待 player_shoot 消费
                erase_melee(events.pending_actions);                // 空手：丢弃
                continue;
            }
        }

        // 冷却中：本次攻击被吞
        if (act.melee_cooldown > 0) {
            erase_melee(events.pending_actions);
            continue;
        }

        auto& apos  = view.get<LogicPositionComponent>(attacker);
        auto& arect = view.get<LogicRectComponent>(attacker);

        // 攻击盒延伸方向：优先用鼠标瞄准角 aim_yaw（与渲染层视觉挥砍一致），
        // 无瞄准数据（服务端/本机从未动过鼠标）时回退键盘朝向 act.dir。
        // 瞄准角按就近吸附成 4 向（与本项目 4 向判定、帧同步确定性保持一致）：
        // 角度约定 0=右 90=下 180=左 270=上（屏幕 y 向下，与 dir_to_angle_deg 同约定）。
        int atk_dir = act.dir;
        if (act.aim_yaw != kAimYawInvalid) {
            int m = (int)aim_yaw_degrees(act.aim_yaw);
            m %= 360; if (m < 0) m += 360;
            if (m <= 45 || m > 315)      atk_dir = 2;   // 右
            else if (m <= 135)           atk_dir = 0;   // 下
            else if (m <= 225)           atk_dir = 1;   // 左
            else                         atk_dir = 3;   // 上
        }

        // 攻击盒 = 自身碰撞盒沿 atk_dir 向前延伸 range（纯定点数，帧同步确定性）
        const fp_t ax = apos.x, ay = apos.y;
        const fp_t aw = arect.width, ah = arect.height;
        const fp_t range = is_player ? fp_from_float(PLAYER_MELEE_RANGE)
                                     : registry.get<AIComponent>(attacker).attack_radius;
        fp_t hx = ax, hy = ay, hw = aw, hh = ah;
        switch (atk_dir) {
            case 0: hy = fp_add(ay, ah); hh = range; break;              // 下
            case 1: hx = fp_sub(ax, range); hw = range; break;           // 左
            case 2: hx = fp_add(ax, aw);   hw = range; break;            // 右
            default: hy = fp_sub(ay, range); hh = range; break;          // 上
        }

        const int damage = is_player ? PLAYER_MELEE_DAMAGE : ENEMY_MELEE_DAMAGE;
        if (is_player) {
            for (auto target : enemy_view) {
                auto& h = enemy_view.get<HealthComponent>(target);
                if (h.dead) continue;
                auto& tp  = enemy_view.get<LogicPositionComponent>(target);
                auto& tr  = enemy_view.get<LogicRectComponent>(target);
                const fp_t tx = tp.x, ty = tp.y;
                const fp_t tw = tr.width, th = tr.height;
                // AABB 相交判定（纯定点数）
                if (hx < fp_add(tx, tw) && fp_add(hx, hw) > tx &&
                    hy < fp_add(ty, th) && fp_add(hy, hh) > ty) {
                    h.hp -= damage; if (h.hp < 0) h.hp = 0;
                    break;
                }
            }
        } else {
            for (auto target : player_view) {
                auto& h = player_view.get<HealthComponent>(target);
                if (h.dead) continue;
                if (h.spawn_invuln > 0) continue;   // 重生无敌期间免疫伤害
                auto& tp  = player_view.get<LogicPositionComponent>(target);
                auto& tr  = player_view.get<LogicRectComponent>(target);
                const fp_t tx = tp.x, ty = tp.y;
                const fp_t tw = tr.width, th = tr.height;
                // AABB 相交判定（纯定点数）
                if (hx < fp_add(tx, tw) && fp_add(hx, hw) > tx &&
                    hy < fp_add(ty, th) && fp_add(hy, hh) > ty) {
                    h.hp -= damage; if (h.hp < 0) h.hp = 0;
                    break;
                }
            }
        }

        // 挥出攻击即进入冷却（无论是否命中），控制攻击频率；只消费近战事件
        act.melee_cooldown = ActionStateComponent::MELEE_COOLDOWN;
        erase_melee(events.pending_actions);
    }
}

#endif
