#ifndef WORLD_GAME_RUNTIME_H
#define WORLD_GAME_RUNTIME_H
#include <queue>
#include <string>

// 世界级玩法运行态（通过 registry.ctx().get<GameRuntime>() 访问）。
// 注意：这是"每个世界一份的资源"，不是挂在某个实体上的组件。
//
// 归属约定：凡"随关卡/世界重置而重置"的状态都放这里，不要再散落到
// GameScene 成员（场景是控制器，不是数据容器）或 Context（跨场景配置）。
struct GameRuntime {
    float FIXED_TIMESTEP = 1.0f / 20.0f;   // 固定逻辑步长（20Hz）
    float visual_time = 0.0f;              // 视觉时间（仅渲染用，不进帧同步）
    int   num_entities = 0;

    // ---- 帧时钟 ----
    float accumulator = 0.0f;              // 固定步长累加器
    float render_dt = 0.0f;                // 渲染期动画推进用的每帧 dt（Update 写入，Render 读取）
    uint32_t fps_measure_time = 0;         // 逻辑帧率测量窗口起点
    int      fps_measure_frame = 0;        // 逻辑帧率测量窗口起点帧号

    // ---- 关卡进度 ----
    bool  game_started = false;            // 敌人生成完毕、关卡正式开始
    float victory_delay = 0.0f;            // 全部肃清后进入胜利场景前的延时累积
    bool  victory_started = false;         // 已触发胜利切换（防重复）

    // ---- 本地输入 ----
    int   current_input_mask = 0;
    bool  attack_triggered = false;        // 近战 tap

    // ---- 瞄准角锁存（量化 0..254；-1 = 尚未设置）----
    // 只在本机计算后量化成整数随输入上传，两端按同一整数还原角度（浮点不跨端）。
    int   aim_keyboard_yaw_q = -1;         // 键盘方向锁存角（松手后保留）
    int   aim_stick_yaw_q    = -1;         // 摇杆方向锁存角（移动端）
    int   last_aim_yaw_sent  = -1;         // 上次随输入上传的瞄准角（-1=从未发过；变化时补发一帧）

    // ---- 网络 ----
    bool  ready = false;
    int   server_frameid = 0;
    std::queue<std::string> command_queue; // 网络命令帧缓冲（网络推入，game_scene 弹出模拟）
};
#endif
