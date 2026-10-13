#ifndef PLAYER_SHOOT_SYSTEM_H
#define PLAYER_SHOOT_SYSTEM_H

#include "../context.h"
#include "../components/player_event_input.h"
#include "../components/action_state_component.h"
#include "../components/logic_position_component.h"
#include "../components/logic_rect_component.h"
#include "../components/player_component.h"
#include "../components/id_component.h"
#include "../components/transform_component.h"
#include "../components/bullet_component.h"
#include "../systems/weapon_catalog.h"
#include "../protocol.h"
#include <joy/jmath.h>

struct Context;

// 箭参数（暂用固定默认值，后续可从 weapon_catalog 扩展 per-weapon 字段）
static constexpr float BULLET_SPEED        = 10.0f;  // 米/秒
static constexpr int   BULLET_DAMAGE       = 20;     // 两发命中 ENEMY_MAX_HP=50
static constexpr int   BULLET_LIFE_TICKS   = 120;    // 6s @20Hz
static constexpr float BULLET_SIZE         = 0.15f;  // 碰撞盒边长（米）
static constexpr float BULLET_SPAWN_OFFSET = 0.5f;   // 出生点沿瞄准方向偏移（避免重叠自身）

// 从事件队列里移除近战攻击事件（只清 ATTACK_MELEE，不影响同帧其它事件）。
// 用普通循环原地压缩，避免引入 <algorithm> 的 remove_if。
static void remove_melee_events(std::vector<PlayerEventInput::Action>& v) {
    size_t w = 0;
    for (size_t r = 0; r < v.size(); ++r) {
        if (v[r].type != PlayerEventInput::ATTACK_MELEE)
            v[w++] = v[r];
    }
    v.resize(w);
}

// 鼠标瞄准角 -> 单位方向向量（定点）查表。
// 表在首次调用时用定点三角函数（fp_cos/fp_sin）一次性生成，之后每次发射只做整数查表，
// 逻辑路径不含浮点。角度 = q * 2π / 255（与 aim_yaw_degrees 同角度），全程定点，两端表逐位一致。
struct AimDirFp { fp_t dx; fp_t dy; };

static AimDirFp aim_yaw_dir(int q) {
    static AimDirFp table[kAimYawMax + 1];
    static bool built = false;
    if (!built) {
        for (int i = 0; i <= kAimYawMax; ++i) {
            fp_t rad = fp_div(fp_mul(fp_from_int(i), FP_PI_TIMES_2), fp_from_int(255));
            table[i].dx = fp_cos(rad);
            table[i].dy = fp_sin(rad);
        }
        built = true;
    }
    if (q < 0 || q > kAimYawMax) q = 0;   // 网络来的异常值兜底，避免越界
    return table[q];
}

// 远程射击系统：消费 PlayerEventInput.pending_actions 中的 ATTACK_MELEE 事件。
// 仅处理"玩家 + 远程武器（kind==0）"的情形；近战武器与敌人攻击交给 melee_attack_system。
// 方向只取自 ActionStateComponent.aim_yaw（鼠标量化角，0..254 映射 0..360°）；上下左右键不参与。
// 帧同步确定性：aim_yaw 由同步输入推导，两端一致；方向用定点查表（见 aim_yaw_dir），逻辑路径无浮点。
static void player_shoot_system(entt::registry& registry, Context* ctx, float /*dt*/)
{
    auto view = registry.view<PlayerEventInput, ActionStateComponent,
                               LogicPositionComponent, LogicRectComponent, PlayerComponent>();

    for (auto e : view) {
        auto& events = view.get<PlayerEventInput>(e);
        auto& act    = view.get<ActionStateComponent>(e);
        auto& lpos   = view.get<LogicPositionComponent>(e);
        auto& lrec   = view.get<LogicRectComponent>(e);
        auto& pc     = view.get<PlayerComponent>(e);

        // 仅处理本系统负责的事件类型
        bool has_attack = false;
        for (size_t i = 0; i < events.pending_actions.size(); ++i) {
            if (events.pending_actions[i].type == PlayerEventInput::ATTACK_MELEE) {
                has_attack = true;
                break;
            }
        }

        // 射击冷却（独立字段，避免与攻击动画计时 attack_timer 冲突）：
        // 每 tick 递减；冷却中丢弃射击事件，防止按住攻击键高频发射。
        if (act.bullet_cooldown > 0) {
            act.bullet_cooldown--;
            if (has_attack) remove_melee_events(events.pending_actions);
            continue;
        }

        if (!has_attack) continue;

        // 只处理远程武器；近战武器交由 melee_attack_system（它已对此分支放行/跳过）
        if (pc.weapon_id < 0 || g_weaponCatalog.get(pc.weapon_id).kind != 0) continue;

        // 方向只来自鼠标瞄准角 aim_yaw（帧同步输入推导，两端一致）；上下左右键不参与方向。
        // 无瞄准数据时本 tick 不发射，并吞掉该攻击事件，避免在队列里堆积。
        if (act.aim_yaw == kAimYawInvalid) {
            remove_melee_events(events.pending_actions);
            continue;
        }
        AimDirFp dir = aim_yaw_dir(act.aim_yaw);   // 定点查表，逻辑路径无浮点
        fp_t dx = dir.dx;
        fp_t dy = dir.dy;

        // 出生点：玩家中心沿瞄准方向偏移
        fp_t cx = fp_add(lpos.x, fp_div(lrec.width,  fp_from_float(2.0f)));
        fp_t cy = fp_add(lpos.y, fp_div(lrec.height, fp_from_float(2.0f)));
        fp_t sx = fp_add(cx, fp_mul(dx, fp_from_float(BULLET_SPAWN_OFFSET)));
        fp_t sy = fp_add(cy, fp_mul(dy, fp_from_float(BULLET_SPAWN_OFFSET)));
        fp_t bw = fp_from_float(BULLET_SIZE), bh = fp_from_float(BULLET_SIZE);

        entt::entity bullet = registry.create();

        registry.emplace<IdComponent>(bullet, get_entity_id(ctx));
        registry.emplace<LogicPositionComponent>(bullet,
            fp_sub(sx, fp_div(bw, fp_from_float(2.0f))),
            fp_sub(sy, fp_div(bh, fp_from_float(2.0f))));
        registry.emplace<LogicRectComponent>(bullet, bw, bh);
        registry.emplace<TransformComponent>(bullet, fp_to_float(sx), fp_to_float(sy), 0.0f, 1.0f, 1.0f);

        BulletComponent b;
        b.vx          = fp_mul(fp_from_float(BULLET_SPEED), dx);
        b.vy          = fp_mul(fp_from_float(BULLET_SPEED), dy);
        b.damage      = BULLET_DAMAGE;
        b.source_conv = 0;
        b.life_ticks  = BULLET_LIFE_TICKS;
        b.from_player = 1;
        registry.emplace<BulletComponent>(bullet, b);

        // 触发拉弓动画（复用 ATTACK 状态与 attack_timer，与近战同源）+ 设置射击冷却
        act.state           = ActionStateComponent::ATTACK;
        act.attack_timer    = ActionStateComponent::ATTACK_DURATION;
        act.bullet_cooldown = ActionStateComponent::BULLET_COOLDOWN;

        remove_melee_events(events.pending_actions);
    }
}

#endif
