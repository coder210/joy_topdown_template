#pragma once

#include "../protocol.h"   // kAimYawInvalid（鼠标瞄准角无效值）

struct ActionStateComponent {
    enum State : int { IDLE = 0, WALK = 1, ATTACK = 2 };
    State state = IDLE;
    int   dir = 0;          // 0=down 1=left 2=right 3=up

    // 攻击爆发时锁定的朝向（替代攻击中跟着转向导致直刺变挥砍）。
    // 直刺（剑）在攻击持续期间固定朝此方向平移，不受当帧移动转向影响；
    // 由 input_consume_input_system 在每次触发 attack 时刷新。
    int   attack_dir = 0;

    // 鼠标瞄准角（量化 0..254 → 0°..360°；255=无瞄准数据，见 protocol.h 的 aim_yaw_*）。
    // 由 input_consume_input_system 从随输入同步的 yaw_q 写入——**不能存 PlayerStateInput**，
    // 那是"每帧用完即清"的瞬时输入组件（cleanup_system 每帧 remove），渲染层读不到。
    // 渲染层（远程武器=弓）据此朝向；近战不受影响。
    int   aim_yaw = kAimYawInvalid;

    // 攻击持续计时（tick 数）：>0 视为攻击中。替代原动画状态机的"攻击动画播完复位"，
    // 由 ai_system / input_consume_input_system 每 tick 递减。
    // 时长取自原 warrior/goblin 攻击动画（6 帧 × 0.15s = 0.9s @20Hz = 18 ticks）
    int attack_timer = 0;
    static constexpr int ATTACK_DURATION = 18;

    // 击退/硬直计时（tick 数）：>0 视为硬直中。由伤害/击退系统在命中时置满，
    // 由 input_consume_input_system 每 tick 递减。驱动 janim 的 knocked 动画（独立于攻击）。
    int knockback_timer = 0;
    static constexpr int KNOCKED_DURATION = 18;   // 0.9s @ 20Hz（与 ATTACK_DURATION 一致）

    // 近战攻击冷却（tick 数）：攻击事件结算后置满，由 melee_attack_system 每 tick 递减，
    // 防止按住攻击键 / 敌人每 tick 产生攻击事件导致高频结算。
    int melee_cooldown = 0;
    static constexpr int MELEE_COOLDOWN = 18;   // 0.9s @ 20Hz（与 ATTACK_DURATION 一致）

    // 远程射击冷却（tick 数）：发射后置满，由 player_shoot_system 每 tick 递减，
    // 防止按住攻击键导致高频发射。与 melee_cooldown 同理，但与攻击动画计时 attack_timer 解耦。
    int bullet_cooldown = 0;
    static constexpr int BULLET_COOLDOWN = 12;  // 0.6s @ 20Hz
};