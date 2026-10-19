#ifndef GAME_SCENE_H
#define GAME_SCENE_H
#include <map>
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

    // 消费一条命令帧（通用解包 + 写 NetComponent；模式特定的跳过/校验经虚函数）
    bool consumeCommandFrame();

    // ---- 原 game_scene struct 字段（集中到 GameScene，供 this->xxx 访问）----
    GameplayControlsLayer* gcl = NULL;       // 操作层（纯 button，不依赖 joy scene_node）
    PauseMenuLayer* pause_layer = NULL;      // 暂停菜单（继续/设置/返回主菜单/退出）
    DebugLayer* debug_layer = NULL;          // 调试 overlay（纯绘制，不依赖 joy scene_node）
    float step_timer = 0;
    uint32_t fps_measure_time = 0;
    int      fps_measure_frame = 0;
    bool game_started = false;   // 敌人生成完毕、关卡正式开始
    float victory_delay = 0.0f;  // 全部肃清后进入胜利场景前的延时累积
    bool victory_started = false;// 已触发胜利切换（防重复）
    std::map<int, int> server_inputs;
    uint64_t lastTime = get_ticks();   // 固定逻辑步时间戳
    float heartbeatTimer = 0.0f;
    float accumulator = 0.0f;          // 固定步长累加器
    float render_dt_ = 0.0f;           // 渲染期动画推进用的每帧 dt（Update 写入，Render 读取）

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
