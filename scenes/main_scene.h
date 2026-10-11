#ifndef MAIN_SCENE_H
#define MAIN_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"
#include "../layers/menu_layer.h"

// 主场景模式测试按钮事件（main.cpp 处理，切换运行模式后进入游戏场景）
#define MODE_EVENT (0x3)

enum mode_event_code {
        MODE_EVENT_SINGLE = 1,
        MODE_EVENT_HOST = 2,
        MODE_EVENT_CLIENT = 3,
        MODE_EVENT_SERVER = 4,
};

#define MAX_PARTICLES 30
struct particle {
        float x, y;
        float vx, vy;
        float alpha;
        float size;
        float phase;
        float twinkle_speed;
};

// 主场景：继承 Scene 基类。菜单 menu_layer 为纯 button 层（不依赖 joy scene_node），
// 由 HandleUiEvent 转发事件、Render 手动渲染。
class MainScene : public Scene {
public:
    explicit MainScene(Context* ctx);
    void Load() override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* ev) override;
private:
    struct particle particles[MAX_PARTICLES];
    float elapsed = 0;
    MenuLayer* menu = NULL;   // 纯 button 菜单层（不依赖 joy scene_node）
};

#endif
