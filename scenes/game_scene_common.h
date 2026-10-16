#ifndef GAME_SCENE_COMMON_H
#define GAME_SCENE_COMMON_H
#include <vector>
#include <string>
#include <cstdio>

// 叶子头：仅做前向声明 + 枚举定义，绝不 include context.h / scene.h / network_manager.h，
// 否则会与 context.h 形成循环包含（context.h 需要 GameModeKind，而上述头又 include context.h）。

// 运行模式（单二进制，运行时通过 --mode 参数切换）
enum GameModeKind {
        MODE_SINGLE = 0,   // 单机：本地推逻辑，不连网络
        MODE_HOST,         // 房主：本地推逻辑 + 广播给其他玩家
        MODE_CLIENT,       // 客户端：连服务端/Host，消费命令 + 渲染
        MODE_SERVER,       // 独立服务端：只推逻辑 + 转发，无本地玩家
};

// 传输角色：决定建链方式与收发/转发行为
enum class Transport { None, Server, Client };

// 运行模式的能力配置（纯数据，不含任何行为）。
// 4 种运行模式本质是这几个正交能力的开关组合，而非 4 套不同算法，
// 故用"一份行为 + 一张配置表"取代原先 4 个子类 × 10 个虚函数的方案。
struct ModeConfig {
    bool        authoritative;    // 权威端：分配帧号、跑权威模拟、初始化权威世界
    bool        localPlayer;      // 本机有玩家：注入玩家 / 操作层 / 音频 / 采集本地输入
    Transport   transport;        // 建链 + 收发 + 命令转发
    bool        verifyChecksum;   // 是否做帧同步 checksum 校验
    bool        drawCollisionDebug; // 是否绘制权威碰撞网格调试（Host / Server）
    const char* logTag;           // checksum 日志文件名
};

// ===== 全部模式差异就这 4 行（唯一真源）=====
static const ModeConfig kModeConfigs[] = {
    /* MODE_SINGLE */ { true,  true,  Transport::None,   true,  false, "checksum_log.txt"        },
    /* MODE_HOST   */ { true,  true,  Transport::Server, false, false, "checksum_log_server.txt" },
    /* MODE_CLIENT */ { false, true,  Transport::Client, true,  false, "checksum_log.txt"        },
    /* MODE_SERVER */ { true,  false, Transport::Server, false, false, "checksum_log_server.txt" },
};

inline const ModeConfig& mode_config(GameModeKind kind) { return kModeConfigs[(int)kind]; }

class GameScene;

class Context;
struct S2CPlayerInput;   // 仅作指针参数，前向声明即可（完整定义见 protocol.h）

// 跨模式共享的静态工具（原自由函数封装为 GameSceneCommon 的静态成员，
// 与各模式子类 override 的虚函数风格保持一致）。
class GameSceneCommon {
public:
    // ---- 单机 / 客户端 / Host 通用：本机玩家键盘回调 ----
    static void playKeyPressed(const char* key, int scancode, bool repeat,
                                 bool ctrl, bool shift, bool alt, void* arg);
    static void playKeyReleased(const char* key, int scancode, void* arg);

    // ---- 单机 / 客户端 / Host 通用：处理操作层事件 + 键盘事件 ----
    static void playableHandleEvent(GameScene* self, const void* ev);

    // ---- 权威端初始化（单机 / Host / 服务端）：spawn_enemy + snapshot + 随机种子 ----
    static void initAuthoritative(Context* ctx);

    // ---- 单机 / 客户端 / Host：创建操作层并挂进场景 + 初始化音频 ----
    static void initControlsAndAudio(GameScene* self);

    // ---- 单机 / Host：注入本地玩家 ----
    static void injectCommonPlayer(GameScene* self);

    // ---- 单机 / 客户端 / Host：音频清理 ----
    static void destroyPlayerAudio(Context* ctx);

    // ---- 帧同步校验实现（客户端 / 单机共用）：由各模式 verifyFrameSync override 调用 ----
    static void verifyFrameSyncImpl(GameScene* self, int frame_id,
                                       S2CPlayerInput* inputs, int in, const char* server_checksum);
};

#endif
