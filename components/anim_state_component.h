#pragma once
#include <joy/janim.h>

// 角色序列帧动画的运行时状态（每实体一份）。
// 纯渲染表现，不参与帧同步、不进快照。
// 作为 ECS 组件挂载到玩家实体上：实体销毁时由 ECS 自动回收，
// 从而消除原先裸全局 unordered_map<id, anim_t> 在实体离开/死亡时不清理导致的泄漏。
// 武器攻击动画运行时（纯渲染、不进快照、不影响帧同步）。
// 由 draw_player_pass 在渲染端检测"逻辑态跳变"触发：近战(挥刀扫弧) / 射击(枪口闪光+后坐)。
struct WeaponAnimState {
    bool  playing = false;   // 是否正在播放攻击动画
    int   kind = 0;          // 1=近战挥动 2=射击
    int   frame = 0;         // 当前攻击条帧序号
    float elapsed = 0.0f;    // 已播放时长（秒）
    float duration = 0.0f;   // 总时长（秒）
    float aim_lock = 0.0f;   // 攻击起手锁定的瞄准角（近战扫弧以它为基准）
    int   prev_attack_timer = 0;  // 上一渲染帧的 attack_timer（检测跳变）
    int   prev_ranged_cd = 0;     // 上一渲染帧的 ranged_cooldown（检测跳变）
};

struct AnimStateComponent {
    anim_t anim;
    const anim_def_t* def = nullptr;   // 一次性缓存角色动画定义，避免每帧按 char_id 查 g_anim_defs
    WeaponAnimState weapon;            // 武器攻击动画运行时（纯渲染）
    AnimStateComponent() { anim_init(&anim); }
};
