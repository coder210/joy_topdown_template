#ifndef GAME_SCENE_H
#define GAME_SCENE_H
#include <memory>
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"
#include "game_scene_common.h"
#include "../network/network_manager.h"
#include "../layers/gameplay_controls_layer.h"
#include "../layers/pause_menu_layer.h"
#include "../layers/debug_layer.h"

// 游戏场景：继承 Scene，单一类承载全部运行模式。
// 单机/Host/客户端/服务端的差异由 cfg_（kModeConfigs 中按 ctx->mode 取的能力配置）
// 驱动——4 种模式本质是"权威端 / 本地玩家 / 传输角色 / 校验"这几个正交开关的组合，
// 故用"一份行为 + 一张配置表"取代原先 4 个子类 × 10 个虚函数的方案。
// 原 game_scene struct 的字段已内联为本类的公有成员（ctx 复用基类 Scene::ctx()）。
class GameScene : public Scene {
public:
    explicit GameScene(Context* ctx);

    void Load() override;
    void HandleEvent(const void* ev) override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;

    // 消费一条命令帧（通用解包 + 写 FrameCommandBuffer；模式特定的跳过/校验经虚函数）
    bool consumeCommandFrame();

    // ---- 本类只保留"装配 + 驱动"所需的对象引用 ----
    GameplayControlsLayer* gcl = NULL;       // 操作层（纯 button，不依赖 joy scene_node）
    PauseMenuLayer* pause_layer = NULL;      // 暂停菜单（继续/设置/返回主菜单/退出）
    DebugLayer* debug_layer = NULL;          // 调试 overlay（纯绘制，不依赖 joy scene_node）

    // 世界运行时态（帧时钟 / 关卡进度 / 本地输入 / 瞄准锁存）统一放在 GameRuntime，
    // 随 registry 重建而重置；本类不再持有玩法运行态，只保留一个绑定引用。
    GameRuntime* rt_ = nullptr;

    NetworkManager net_;   // 网络收发/帧同步辅助（持有 ctx_，供 this->net_ 调用）

private:
    // 模式能力：构造时按 ctx->mode 从 kModeConfigs 取一行，之后不变。
    // 全部模式差异由这 4 个开关驱动，故不再需要 Single/Host/Client/Server 四个子类。
    const ModeConfig& cfg_;

    // ---- 模式步骤（原名保留，仅由虚函数改为普通私有成员，调用点不变）----
    void init();
    void handleEvent(const void* ev);
    void networkUpdate();
    void tick();
    void render();
    void destroy();

    // ---- 帧同步 / 模式特征（消除 ctx->mode 分支判断）----
    bool shouldSkipFrame(int frame_id) const;
    void verifyFrameSync(int frame_id,
                         S2CPlayerInput* inputs, int in, const char* server_checksum);
    bool shouldSimulate(bool got_command) const;
    int  currentLogicFrame() const;
    bool shouldCheckVictory() const;
    const char* logTag() const { return cfg_.logTag; }

    // 独立服务端的键盘回调：只响应退出键 q
    static void onServerKeyPressed(const char* key, int scancode, bool repeat,
                                   bool ctrl, bool shift, bool alt, void* arg);
};

#endif
