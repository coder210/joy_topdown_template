#ifndef VICTORY_SCENE_H
#define VICTORY_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

// 通关场景：继承 Scene 基类，事件由 SceneManager 直接分发到 HandleUiEvent
//（内部用 joy 的 dispatch_event 解析），不再依赖 joy 的 scene_node 桥接。
class VictoryScene : public Scene {
public:
    explicit VictoryScene(Context* ctx);
    void Load() override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;
private:
    button_p btn_replay = NULL, btn_menu = NULL;
    float elapsed = 0;
};

#endif
