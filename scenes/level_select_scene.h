#ifndef LEVEL_SELECT_SCENE_H
#define LEVEL_SELECT_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

// 关卡选择场景：继承 Scene 基类，事件由 SceneManager 直接分发到 HandleUiEvent
//（内部用 joy 的 dispatch_event 解析），不再依赖 joy 的 scene_node 桥接。
class LevelSelectScene : public Scene {
public:
    explicit LevelSelectScene(Context* ctx, GameModeKind mode);
    void Load() override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;
private:
    GameModeKind target_mode = MODE_SINGLE;
    button_p btn_levels[6];
    button_p btn_confirm = NULL, btn_back = NULL;
    int selected = 0;
    void draw_level(int i);   // 原 free 函数，改为成员以访问 private 字段
};

#endif
