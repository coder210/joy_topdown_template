#ifndef UI_STYLE_H
#define UI_STYLE_H
#include <joy/joy.h>

// 跨 layer / scene 共享的按钮配色。
// 原先 setup_button_style 自由函数在 menu_layer / gameplay_controls_layer /
// level_select_scene / character_select_scene / victory_scene /
// settings_scene / online_scene 七处各存一份（且配色有 4 种变体），
// 此处收敛为唯一真源：一份赋值实现 + 若干命名预设。
class UIStyle {
public:
    // 宣纸白底：在白底场景里常态为描边按钮（白填充与白底同色，靠黑边/黑字识别），
    // 悬停/按下用可见的浅灰填充，保证白底上有反馈（菜单 / 选关 / 选角 / 结算）
    static void applyPaperButton(button_p btn) {
        setColors(btn, { 255, 255,255, 255 }, { 232, 234, 240, 255 }, { 214, 217, 227, 255 });
    }

    // 水墨：悬停保持纯白，按下略深以体现落笔（操作层）
    static void applyInkButton(button_p btn) {
        setColors(btn, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 230, 228, 222, 255 });
    }

    // 纯白：三种状态同色（设置页）
    static void applyFlatButton(button_p btn) {
        setColors(btn, { 255, 255, 255, 255 }, { 255, 255, 255, 255 }, { 255, 255, 255, 255 });
    }

    // 联机：悬停/按下偏蓝（联机页）
    static void applyOnlineButton(button_p btn) {
        setColors(btn, { 255, 255, 255, 255 }, { 220, 225, 235, 255 }, { 200, 200, 210, 255 });
    }

    // 大厅次按钮：浅灰填充（白底上清晰可见，触屏友好）
    static void applyLightButton(button_p btn) {
        setColors(btn, { 236, 238, 244, 255 }, { 220, 224, 233, 255 }, { 202, 207, 219, 255 });
    }

    // 大厅主行动按钮（创建房间）：蓝色实心 + 白字，形成明确主次
    static void applyPrimaryButton(button_p btn) {
        setColors(btn, { 66, 133, 244, 255 }, { 90, 152, 250, 255 }, { 48, 110, 214, 255 });
    }

private:
    static void setColors(button_p btn, jcolor_t normal, jcolor_t hover, jcolor_t pressed) {
        button_set_normal_color(btn, normal);
        button_set_hover_color(btn, hover);
        button_set_pressed_color(btn, pressed);
    }
};

#endif
