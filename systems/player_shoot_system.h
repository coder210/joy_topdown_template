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
// 箭速：原 10 米/秒。逻辑 20Hz 下每 tick 走 0.5 米，扫掠判定必须有 0.5 米拖尾，
// 导致"离得挺远就中"。降到 6 -> 每 tick 0.3 米，拖尾缩短 40%。
static constexpr float BULLET_SPEED        = 6.0f;   // 米/秒
static constexpr int   BULLET_DAMAGE       = 20;     // 两发命中 ENEMY_MAX_HP=50
static constexpr int   BULLET_LIFE_TICKS   = 120;    // 6s @20Hz
// 碰撞盒边长（米）。原为 0.15，但箭贴图绘制尺寸是 1 米（见 draw_bullet_pass.h），
// 判定只覆盖贴图的 15%，视觉上箭穿过敌人却不命中。
// 先调到 0.4 解决"打不中"，再收到 0.3：配合降速，扫掠盒总长 0.9 -> 0.6 米。
static constexpr float BULLET_SIZE         = 0.30f;
static constexpr float BULLET_SPAWN_OFFSET = 0.5f;   // 出生点沿瞄准方向偏移（避免重叠自身）

// 拉弓时长（tick）：按下射击后拉弓多久才离弦（期间弓显示"有箭的弓"纹理、箭还搭在弦上）。
// 取自 ActionStateComponent::SHOOT_DRAW_TICKS——渲染层（搭箭/松放分界）用同一常量，
// 所以"箭离弦"与"弓切到无箭纹理"必然同 tick。

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

// 在"当前"枪口位置生成一支箭（方向取自 aim_yaw 的定点查表，逻辑路径无浮点）。
// 抽成独立函数是为了让离弦发生在确定的 tick，并用玩家"那一刻"的位置——
// 边移动边射时出生点会跟随玩家，不会停在按下瞬间的旧位置。
static void spawn_player_bullet(entt::registry& registry, Context* ctx,
                                const LogicPositionComponent& lpos,
                                const LogicRectComponent& lrec, int aim_yaw)
{
    AimDirFp dir = aim_yaw_dir(aim_yaw);   // 定点查表，逻辑路径无浮点
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
}

// 远程射击系统：消费 PlayerEventInput.pending_actions 中的 ATTACK_MELEE 事件。
// 仅处理"玩家 + 远程武器（kind==0）"的情形；近战武器与敌人攻击交给 melee_attack_system。
// 方向只取自 ActionStateComponent.aim_yaw（鼠标量化角，0..254 映射 0..360°）；上下左右键不参与。
// 帧同步确定性：aim_yaw 由同步输入推导，两端一致；方向用定点查表（见 aim_yaw_dir），逻辑路径无浮点。
//
// 按下射击即发射：按下攻击键的当 tick 就用玩家"当时位置 + 当时瞄准角"生成箭并飞出，
// 不再有"拉弓等待"延迟——箭的出生点就是按下瞬间的位置。
// 弓的动画（有箭帧 → 无箭帧 → 有箭帧）是纯表现层，与箭的生成时机解耦。
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

        // 空弦计时：每 tick 无条件递减（不受冷却/事件影响），归零即"重新搭箭"→回到有箭帧。
        if (act.arrow_gone_timer > 0) act.arrow_gone_timer--;

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

        // ---- 发射：点击瞬间立即用"当前位置 + 当前瞄准角"生成箭（无拉弓延迟）----
        // 攻击状态/attack_timer 已由 input_consume_input_system 在"按下攻击键"时统一置位
        // （按钮→动画计时，全武器通用，本系统不再重复设），这里只负责冷却与生成箭：
        act.bullet_cooldown   = ActionStateComponent::BULLET_COOLDOWN;
        act.arrow_gone_timer  = ActionStateComponent::ARROW_GONE_TICKS;   // 空弦一小段→再搭箭
        spawn_player_bullet(registry, ctx, lpos, lrec, act.aim_yaw);

        // 音效与"真正发射"严格同步：只有通过冷却/瞄准检查、确实生成了箭才播放，
        // 快速连点被冷却吞掉的输入不会再响（这正是之前点攻击键音效的违和点）。
        // 独立服务端无本机音频（bow_sound == NULL），audio_shot_play 对 NULL 安全。
        if (ctx->bow_sound) audio_shot_play(ctx->bow_sound);

        remove_melee_events(events.pending_actions);
    }
}

#endif
