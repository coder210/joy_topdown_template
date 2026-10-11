#ifndef MENU_LAYER_H
#define MENU_LAYER_H
#include "layer.h"

#define MENU_EVENT (0x2)

enum menu_event_code {
        MENU_EVENT_START = 1,
        MENU_EVENT_ONLINE = 2,
        MENU_EVENT_SETTINGS = 3,
        MENU_EVENT_EXIT = 4,
};

// 纯 C++ 类实现的菜单层（继承 UILayer，不再依赖 joy scene_node）
class MenuLayer : public UILayer {
public:
    explicit MenuLayer(Context* ctx);
    ~MenuLayer() override;

    void Render() override;
    void HandleEvent(const void* ev) override;
    void Destroy() override;

private:
    button_p btn_start = NULL, btn_online = NULL, btn_settings = NULL, btn_exit = NULL;
    void load_menu_buttons();

    // ---- 实现辅助（原文件内 static 自由函数）----
    static void sendMenuEvent(int event_code);
};

#endif
