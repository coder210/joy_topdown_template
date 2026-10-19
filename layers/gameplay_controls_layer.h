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
        GAMEPLAY_CONTROL_RELEASE_UP = 1 << 9,
        GAMEPLAY_CONTROL_RELEASE_DOWN = 1 << 10,
        GAMEPLAY_CONTROL_RELEASE_LEFT = 1 << 11,
        GAMEPLAY_CONTROL_RELEASE_RIGHT = 1 << 12,
};

struct gc_button_meta {
        button_p btn;
        Context* ctx;
        float w, h, x, y;
        const char* label;      // 圆心文字（攻）；同时用于选择按钮纹理
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
    button_p btn_attack = NULL;       // 右下攻击键（保持原样）
    gc_button_meta attack_meta = {};
    joystick_p move_stick = NULL;     // 左下左摇杆：同时用于移动方向 + 瞄准方向
    int      stick_active = 0;        // 当前触点是否正在驱动摇杆
    int      last_set_ev = 0;         // 上次发送的 SET 方向事件（方向变化发 RELEASE 用）
    int      last_rel_ev = 0;
    button_p pressed_btn = NULL;      // 当前按住中的按钮（仅攻击键）
    int      pressed_code = 0;
    int      layout_lw = 0;           // 上次同步布局时的逻辑分辨率（变化才重算）
    int      layout_lh = 0;

    void add_button(button_p* slot, gc_button_meta* meta,
                    float cx, float cy, float wf, float hf,
                    const char* label, int coolType);
    void load_controls();             // 创建左摇杆 + 攻击键
    void syncLayout();                // 按当前逻辑分辨率锚定右下/左下布局
    void updateStick();               // 摇杆向量 → 移动位 + 锁存瞄准角

    // ---- 实现辅助 ----
    static void sendGameplayControlEvent(int event_code);
    float localCoolRatio(int coolType);
    void drawCoolRing(float cx, float cy, float R, float thick, float frac);
    void drawButton(const gc_button_meta* m);
};

#endif
