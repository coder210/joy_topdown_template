#ifndef PLAYER_EVENT_INPUT_H
#define PLAYER_EVENT_INPUT_H
#include <vector>
#include <joy/jmath.h>

struct PlayerEventInput {
    // 动作类型（对齐 3D：近战/射击/射线由同一事件队列分发，敌我共用）
    enum ActionType : int {
        ATTACK_MELEE  = 0,   // 近战（含敌人 Melee 事件，ai_system 默认推 0）
        ATTACK_BULLET = 1,   // 射击：玩家"射"按钮发射单颗子弹
        ATTACK_RAY    = 2,   // 射线：玩家"线"按钮沿朝向 hitscan 大招
    };

    // 单个动作定义
    struct Action {
        uint32_t client_seq;   // 客户端序列号（用于调试/防重放）
        uint8_t type;          // ActionType
        // 可扩展：如目标位置、角度等
    };

    // 待处理的事件队列（FIFO）
    std::vector<Action> pending_actions;

    // 方便清空
    void clear() { pending_actions.clear(); }

};

#endif
