#ifndef EVENTS_H
#define EVENTS_H

// 用户自定义事件类型
#define TRANSITION_EVENT (0x6)

// 跨场景过渡事件码
enum transition_event_code {
        TRANSITION_SPLASH_DONE = 1,
        TRANSITION_LOADING_DONE = 2,
};

// 通关事件（game_scene 判定全清后推送，main.cpp 切到胜利场景）
#define VICTORY_EVENT (0x8)
enum victory_event_code {
        VICTORY_EVENT_TRIGGERED = 1,
};

#endif
