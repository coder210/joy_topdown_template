#ifndef CONTEXT_H
#define CONTEXT_H
#include <map>
#include <vector>
#include <unordered_map>
#include <queue>
#include <string>
#include <functional>
#include <cstdio>
#include <cstdlib>
#include <chrono>
#include <memory>
#include "entt.hpp"
#include <joy/joy.h>
#include "protocol.h"
#include "components/global_state_component.h"
#include "systems/state_checksum.h"
// 游戏场景共享运行量：拆自原 context_net_state / game_scene_state，
// 存为 registry.ctx() 上下文组件（同 GlobalState 模式），经 game_runtime()/net_session() 读取。
#include "components/game_runtime_component.h"
#include "components/net_session_component.h"
#include "components/system_registry_component.h"
// 运行模式枚举（GameMode）与用户自定义事件码（TRANSITION/VICTORY_EVENT）
#include "scenes/game_scene_common.h"
#include "events.h"
// 逐系统剖析（计时/累计独立于此，调度循环仍在 run_phase）
#include "profiler.h"
// 用户设置（结构 + settings.cfg 存取，移入独立 settings.h/.cpp）
#include "settings.h"

// 运行模式枚举（GameMode）见 game_mode.h，用户自定义事件码见 events.h

class SceneManager;  // 前向声明：scene_mgr 指向 main 创建的 SceneManager（不拥有）

struct Context {
        GameModeKind mode = MODE_SINGLE;

        Settings settings;   // 用户设置（切换模式/进出场景时保留）

        bool running;
        game_timer_t game_timer;
        jwindow_p window;
        jr2d_p r2d;
        SceneManager* scene_mgr = nullptr;  // 指向 main 创建的 SceneManager；场景经其 requestTransition/setPending 登记过渡目标（不拥有）
        netclient_p netclient;
        netserver_p net_server;
        entt::registry ecs;

        // ---- 音频 ----
        uint32_t            audio_dev = 0;
        audio_shot_p        step_sound = NULL;
        audio_shot_p        bow_sound = NULL;   // 射箭音效（真正发射时播放）
        audio_bgm_p         bgm = NULL;
        // 远程玩家方向追踪
        //std::map<entt::entity, int>  remote_dirs;

        // ========== 角色/关卡选择（跨场景保留，loading 由 reset_context_for_mode 不清除）==========
        int  selected_char_id  = 0;                    // 选中角色（CHAR_WARRIOR 默认）
        int  selected_level    = 1;                    // 选中关卡（默认第 1 关）
        char selected_nickname[32] = "玩家";            // 玩家昵称

        // ========== 鼠标（仅渲染层瞄准用，不参与帧同步）==========
        // 鼠标事件给的是窗口像素，统一经 r2d_coordinates_from_window 换算到逻辑坐标(480x320)。
        // 由 GameplayControlsLayer 的 on_mousemoved 写入；无鼠标(服务端/未移动过)时 mouse_valid=false。
        vec2_t mouse_logical = { -1.0f, -1.0f };
        bool   mouse_valid   = false;

        // ========== 瞄准角（本地计算后量化成 0..254 整数随输入上传，两端一致）==========
        // 由 collect_aim_yaw_q 按优先级写入/读取；-1 表示尚未设置。
        // 来源优先级：键盘方向 > 摇杆（移动端兜底）。不再使用鼠标位置瞄准（取消"鼠标旋转"）。
        // 键盘方向现按 dx/dy 合成向量经 atan2 求角，支持八向（含斜角），再 aim_yaw_quantize 量化；
        // 该 255 档逻辑保持不变，以后改回模拟输入（如摇杆全方向）即可恢复 255 档细分。
        int aim_keyboard_yaw_q = -1;   // 键盘方向锁存角（松手后保留，便于停步后仍能朝该方向开火）
        int aim_stick_yaw_q    = -1;   // 摇杆方向锁存角（移动端）
};

// ========== System 调度基础设施 ==========
enum Phase { OnStart, PreUpdate, OnUpdate, PostUpdate };

struct SystemEntry {
        std::string name;
        Phase phase;
        std::function<void(entt::registry&, Context*, float)> fn;
};

inline void add_system(Context* ctx, Phase phase, const char* name,
        void (*fn)(entt::registry&, Context*, float))
{
        ctx->ecs.ctx().get<SystemRegistry>().systems.push_back({ name, phase, fn });
}

inline void run_phase(Context* ctx, Phase phase, float dt)
{
    // ---- 临时剖析：仅对 OnUpdate 逐个系统计时，秒级打印（不影响帧同步逻辑）----
    // 计时/累计/清零独立在 profiler.h（SystemProfiler），此处仅保留调度与打印。
    SystemProfiler& prof = system_profiler();
    if (phase == OnUpdate) prof.begin();
    for (auto& sys : ctx->ecs.ctx().get<SystemRegistry>().systems) {
        if (sys.phase != phase) continue;
        auto t0 = std::chrono::high_resolution_clock::now();
        sys.fn(ctx->ecs, ctx, dt);
        auto t1 = std::chrono::high_resolution_clock::now();
        double ms = std::chrono::duration<double, std::milli>(t1 - t0).count();
        if (phase == OnUpdate) prof.record(ms);
    }
    if (phase == OnUpdate) {
        // uint32_t now = get_ticks();
        // if (prof.due(now)) {
        //     int i = 0;
        //     for (auto& sys : ctx->ecs.ctx().get<SystemRegistry>().systems) {
        //         if (sys.phase != OnUpdate) continue;
        //         if (i < 128 && prof.calls(i) > 0) {
        //             fprintf(stderr, "[prof] %-14s calls=%ld total=%.1fms avg=%.3fms\n",
        //                 sys.name.c_str(), prof.calls(i), prof.total_ms(i), prof.total_ms(i) / prof.calls(i));
        //         }
        //         i++;
        //     }
        //     prof.reset(now);
        // }
    }
}


// 游戏场景共享运行量访问助手：GameRuntime / NetSession 存于 registry.ctx()（同 GlobalState 模式），
// ECS system / render pass / network_utils 仅持有 Context*，无法触达 game_scene，故经此读取。
inline GameRuntime& game_runtime(Context* ctx) { return ctx->ecs.ctx().get<GameRuntime>(); }
inline NetSession&  net_session(Context*  ctx) { return ctx->ecs.ctx().get<NetSession>(); }
inline SystemRegistry& system_registry(Context* ctx) { return ctx->ecs.ctx().get<SystemRegistry>(); }

// 全局实体 id 分配：统一从 GlobalState.next_entity_id 取。
// 服务端/Host/单机为权威分配；客户端在 handle_loading 时把 next_entity_id 设为
// 服务端下发的 world_eid（已跳过敌人占用的 1..N 区间），保证两端 id 序列对齐。
// 注意：两端必须在每个 tick 以相同顺序、相同次数调用 get_entity_id（玩家 join、子弹发射等），
// 否则 id 序列会错位导致帧同步 mismatch——这是帧同步的固有前提，不是 id 起点问题。
static int get_entity_id(Context* ctx)
{
    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    return gs.next_entity_id++;
}

// 运行时切换模式：重置所有瞬时状态（ECS/系统注册/网络/命令缓冲），
// 保证可反复进出游戏场景而不重复注册系统或重复 emplace 单例组件。
static void reset_context_for_mode(Context* ctx, GameModeKind mode)
{
    if (ctx->netclient) { netclient_destroy(ctx->netclient); ctx->netclient = NULL; }
    if (ctx->net_server) { netserver_destroy(ctx->net_server); ctx->net_server = NULL; }

    ctx->ecs = entt::registry();   // 全新 registry（含 ctx() 上下文）
    // 说明：GameRuntime / NetSession / SystemRegistry 存于 registry.ctx()，随全新 registry
    // 一并清空，由 game_scene on_load 重新 emplace（system 也在 on_load 重新 add_system），
    // 故无需手动清空；game_scene 每次进入都 new 出全新实例，编排态亦随之复位。

    ctx->mode = mode;
}


#endif
