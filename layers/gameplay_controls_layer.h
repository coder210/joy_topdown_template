#ifndef GAMEPLAYER_CONTROLS_LAYER_H
#define GAMEPLAYER_CONTROLS_LAYER_H
#include "layer.h"

#define GAMEPLAY_CONTROL_EVENT (0x1)

enum gameplay_control_event_k {
        GAMEPLAY_CONTROL_NONE = 1 << 0,
        GAMEPLAY_CONTROL_UP = 1 << 1,
        GAMEPLAY_CONTROL_DOWN = 1 << 2,
        GAMEPLAY_CONTROL_LEFT = 1 << 3,
        GAMEPLAY_CONTROL_RIGHT = 1 << 4,
        GAMEPLAY_CONTROL_ATTACK = 1 << 5,
        GAMEPLAY_CONTROL_RANGED = 1 << 6,
        GAMEPLAY_CONTROL_RAY = 1 << 7,
        GAMEPLAY_CONTROL_SWITCH = 1 << 8,
        GAMEPLAY_CONTROL_RELEASE_UP = 1 << 9,
        GAMEPLAY_CONTROL_RELEASE_DOWN = 1 << 10,
        GAMEPLAY_CONTROL_RELEASE_LEFT = 1 << 11,
        GAMEPLAY_CONTROL_RELEASE_RIGHT = 1 << 12,
};

struct gc_button_meta {
        button_p btn;
        Context* ctx;
        float w, h, x, y;
        const char* label;      // 圆心文字（上/下/左/右/攻/射/线）
        const char* keyLabel;   // 键盘角标字符（W/S/A/D/J/K/L）
        int coolType;
};

// 纯 C++ 类实现的操作层（继承 UILayer，不再依赖 joy scene_node）
class GameplayControlsLayer : public UILayer {
public:
    explicit GameplayControlsLayer(Context* ctx);
    ~GameplayControlsLayer() override;

    void Render() override;
    void HandleEvent(const void* ev) override;
    void Destroy() override;

private:
    button_p btn_left = NULL, btn_right = NULL;
    button_p btn_up = NULL, btn_down = NULL;
    button_p btn_attack = NULL, btn_ranged = NULL, btn_ray = NULL;
    button_p btn_switch = NULL;    // 切换武器按钮
    button_p pressed_btn = NULL;   // 当前按住中的按钮（方向键或技能键）
    int      pressed_code = 0;     // 松开时的事件码：技能=触发码，方向=RELEASE_*
    gc_button_meta metas[8] = {};
    char     switch_label_[32] = { 0 };  // 切换钮动态标签缓冲（指向武器名/空手）
    void add_button(int idx, button_p* slot,
                    float cx, float cy, float wf, float hf,
                    const char* label, const char* keyLabel, int coolType);
    void load_dpad_panel();

    // ---- 实现辅助（原文件内 static 自由函数，改为成员以直接访问 ctx_）----
    static void sendGameplayControlEvent(int event_code);
    float localCoolRatio(int coolType);
    void drawCoolRing(float cx, float cy, float R, float thick, float frac);
    void drawKeyBadge(font_p font, const char* key, float bx, float by);
    void drawWeaponIcon(float cx, float cy, float R);   // 切换钮画当前武器贴图，而非文字
    void drawButton(const gc_button_meta* m);
};

#endif
