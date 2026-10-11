#ifndef ONLINE_SCENE_H
#define ONLINE_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

#define ONLINE_EVENT (0x5)

enum online_event_code {
        ONLINE_EVENT_HOST = 1,
        ONLINE_EVENT_CLIENT = 2,
        ONLINE_EVENT_SERVER = 3,
        ONLINE_EVENT_BACK = 4,
};

// 联机模式选择场景：继承 Scene 基类，事件由 SceneManager 直接分发到 HandleUiEvent
//（内部用 joy 的 dispatch_event 解析）；按钮由 Render 手动绘制，不依赖 joy 的 scene_node。
class OnlineScene : public Scene {
public:
    explicit OnlineScene(Context* ctx);
    void Load() override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;
private:
    button_p btn_host = NULL, btn_client = NULL, btn_server = NULL, btn_back = NULL;
};

#endif
