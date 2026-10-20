#ifndef PAUSE_MENU_LAYER_H
#define PAUSE_MENU_LAYER_H

#include "layer.h"
#include <joy/joy.h>

// 游戏内暂停菜单（方案 B）：
// - 右上角「||」按钮 → 打开暂停菜单：继续 / 设置 / 返回主菜单 / 退出游戏
// - 返回主菜单、退出游戏 → 二次确认（模态）
// - ESC / 安卓返回键：菜单关闭时打开；打开时逐级返回/关闭
// - 模态：菜单打开时吞掉所有游戏输入，避免操作穿透
// - 帧同步：联机模式不暂停模拟（暂停会破坏双端一致），仅屏蔽输入；
//   单机由 GameScene 根据 should_pause_sim() 跳过固定步模拟。
class PauseMenuLayer : public UILayer {
public:
    explicit PauseMenuLayer(Context* ctx);
    ~PauseMenuLayer() override;

    void Render() override;
    void HandleEvent(const void* ev) override;
    void Destroy() override;

    // GameScene 调用：返回 true 表示事件已被本层消费（不要再传给游戏操作层）
    bool handle_event(const void* ev);

    bool is_open() const { return page_ != PAGE_NONE; }
    // 只有单机允许真正暂停模拟；联机暂停会导致帧同步发散
    bool should_pause_sim() const { return page_ != PAGE_NONE && single_player_; }

private:
    enum Page { PAGE_NONE = 0, PAGE_MENU, PAGE_CONFIRM, PAGE_SETTINGS };
    enum ConfirmAction { CONFIRM_MAINMENU = 0, CONFIRM_QUIT };

    void openMenu();
    void closeMenu();
    void openSettings();
    void showConfirm(ConfirmAction a);
    void confirmOk();

    void clearGameInput();      // 打开菜单时清空移动/攻击位
    void syncLayout();          // 按当前逻辑分辨率重算控件像素几何
    void layoutSliders();       // 设置页滑块的位置/尺寸
    void applySettings();       // 读滑块 → 实时应用音量
    void setupButtons();
    void updateHover(float x, float y);
    void releasePressed();

    Page page_ = PAGE_NONE;
    int  confirm_action_ = CONFIRM_MAINMENU;
    bool single_player_ = false;
    bool settings_dirty_ = false;
    int  layout_lw_ = 0;
    int  layout_lh_ = 0;

    button_p btn_pause_ = nullptr;

    button_p btn_resume_ = nullptr;
    button_p btn_settings_ = nullptr;
    button_p btn_mainmenu_ = nullptr;
    button_p btn_quit_ = nullptr;

    button_p btn_cancel_ = nullptr;
    button_p btn_ok_ = nullptr;

    button_p btn_settings_back_ = nullptr;
    button_p btn_debug_ = nullptr;      // 设置页：显示/隐藏调试 HUD（手机端没有 F1）
    slider_p bgm_slider_ = nullptr;
    slider_p sfx_slider_ = nullptr;

    void updateDebugButtonText();       // 同步“调试信息：开/关”文案
};

#endif // PAUSE_MENU_LAYER_H
