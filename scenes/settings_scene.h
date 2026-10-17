#ifndef SETTINGS_SCENE_H
#define SETTINGS_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

// 设置场景事件（main.cpp 处理导航）
#define SETTINGS_EVENT (0x4)

enum settings_event_code {
        SETTINGS_EVENT_BACK = 1,
};

// 设置场景：继承 Scene 基类，事件由 SceneManager 直接分发到 HandleUiEvent（内部用
// joy 的 dispatch_event 解析）。IP 输入使用 inputbox（自由输入），彻底脱离 scene_node。
class SettingsScene : public Scene {
public:
    explicit SettingsScene(Context* ctx);
    void Load() override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;
private:
    inputbox_p ip_input = NULL;
    inputbox_p port_input = NULL;
    slider_p bgm_slider = NULL, sfx_slider = NULL;
    checkbox_p fullscreen_cb = NULL, collision_cb = NULL;
    button_p btn_back = NULL;
    uint32_t   preview_dev  = 0;   // BGM 试听用独立音频设备（与游戏场景 BGM 设备互不干扰）
    audio_bgm_p preview_bgm = NULL;
    void sync_settings();   // 原 free 函数，改为成员以访问 private 字段
};

#endif
