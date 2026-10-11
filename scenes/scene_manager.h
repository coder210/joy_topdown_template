#pragma once
#include <memory>
#include <joy/joy.h>

class Scene;   // 前向声明：方法仅用 Scene*，实现在 cpp 实例化（避免与 scene.h 循环 include）

// C++ 场景管理器（阶段 3）：完全自管场景栈与生命周期，不再转调 joy 的 scene_manager_*。
// 事件直接来自 SDL_Event，鼠标事件解析后转调 Scene 的 OnMouse*，其余转调 HandleEvent。
class SceneManager {
public:
    SceneManager() = default;
    ~SceneManager() = default;

    // 进入场景：加载并接管为当前场景（替换旧场景，先清理其资源）
    void push(Scene* next);
    void change(Scene* next);

    // 仅登记过渡目标到 pending_（不推送事件），由调用方自行推送对应事件
    void setPending(Scene* next);
    // 登记过渡目标并推送 TRANSITION_EVENT（多数场景的延迟切换入口）
    void requestTransition(Scene* next, int code = 0);
    // 消费 pending_（SDL_AppEvent 收到 TRANSITION_EVENT / VICTORY_EVENT 时调用）
    void processPending();

    void HandleEvent(const SDL_Event* e);
    void Update(float dt);
    void Render(jr2d_p r2d);

    void reset();
    const char* currentName() const;

private:
    std::unique_ptr<Scene> current_;
    std::unique_ptr<Scene> pending_;
};
