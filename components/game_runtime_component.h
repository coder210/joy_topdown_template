#ifndef GAME_RUNTIME_COMPONENT_H
#define GAME_RUNTIME_COMPONENT_H
#include <queue>
#include <string>

// 游戏场景共享玩法运行态。
// 拆自原 context.h 的 game_scene_state。因 ECS system / render pass / network_utils 仅持有
// Context*、拿不到 game_scene，故将"跨 system 共享的运行量"存为 registry.ctx() 上下文组件
// （与 GlobalState 同一模式），经 game_runtime(ctx) 读取。
// 纯编排态（accumulator / heartbeatTimer / lastTime / server_inputs）保留在 GameScene
// （见 scenes/game_scene.h，原 game_scene struct 字段已内联为该类的公有成员）。
struct GameRuntime {
    float FIXED_TIMESTEP = 1.0f / 20.0f;   // 固定逻辑步长（20Hz）
    float visual_time = 0.0f;              // 视觉时间（仅渲染用，不进帧同步）
    int   num_entities = 0;

    int   local_dir = 0;                   // 本地玩家方向
    int   current_input_mask = 0;
    bool  attack_triggered = false;        // 近战 tap

    bool  ready = false;
    int   server_frameid = 0;

    std::queue<std::string> command_queue; // 网络命令帧缓冲（网络推入，game_scene 弹出模拟）
};
#endif