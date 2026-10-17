#include <string.h>
#include <cstdlib>
#include "settings_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"

static void apply_fullscreen(Context* ctx) {
    window_set_fullscreen(ctx->window, ctx->settings.fullscreen);
}

// 把当前 UI 值同步到 settings 并自动保存（变化即写盘）
void SettingsScene::sync_settings() {
    Context* ctx = this->ctx();
    bool changed = false;

    const char* ip = inputbox_get_text(ip_input);
    if (strcmp(ip, ctx->settings.server_ip) != 0) {
        snprintf(ctx->settings.server_ip, sizeof(ctx->settings.server_ip), "%s", ip);
        changed = true;
    }

    int port = atoi(inputbox_get_text(port_input));
    if (port != ctx->settings.server_port) {
        ctx->settings.server_port = port;
        changed = true;
    }

    float bgm = slider_get_value(bgm_slider);
    if (bgm != ctx->settings.bgm_volume) {
        ctx->settings.bgm_volume = bgm;
        if (ctx->bgm) audio_bgm_set_gain(ctx->bgm, bgm);
        changed = true;
    }

    float sfx = slider_get_value(sfx_slider);
    if (sfx != ctx->settings.sfx_volume) {
        ctx->settings.sfx_volume = sfx;
        if (ctx->step_sound) audio_shot_set_gain(ctx->step_sound, sfx);
        UiSfx::Get().setGain(sfx);   // UI 点击音跟随"音效"滑块实时生效
        changed = true;
    }

    bool fs = (checkbox_get_state(fullscreen_cb) == JOY_CHECKBOX_CHECKED);
    if (fs != ctx->settings.fullscreen) {
        ctx->settings.fullscreen = fs;
        apply_fullscreen(ctx);
        changed = true;
    }

    if (changed) settings_save(&ctx->settings);
}

SettingsScene::SettingsScene(Context* ctx) : Scene(ctx, "Settings") {}

void SettingsScene::HandleUiEvent(const void* e) {
    auto* self = this;
    jevent_callbacks_t cbs = {};
    cbs.on_process_event = [](const void* ev, void* arg) {
        SettingsScene* self = (SettingsScene*)arg;
        if (self->ip_input)    inputbox_process_event(self->ip_input, ev);
        if (self->port_input)  inputbox_process_event(self->port_input, ev);
        if (self->bgm_slider)  slider_process_event(self->bgm_slider, ev);
        if (self->sfx_slider)  slider_process_event(self->sfx_slider, ev);
        // 复选框：仅在"勾选状态发生变化"时响一声 switch（悬停/重复事件不响）
        auto cb_event = [](checkbox_p cb, const void* e) {
                if (!cb) return;
                int before = (int)checkbox_get_state(cb);
                checkbox_process_event(cb, e);
                if ((int)checkbox_get_state(cb) != before) UiSfx::Get().play(UiSfx::kSwitch);
        };
        cb_event(self->fullscreen_cb, ev);
        cb_event(self->collision_cb, ev);
    };
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        SettingsScene* self = (SettingsScene*)arg;
        if (self->btn_back && ui_button_hit(self->btn_back, x, y))
            push_user_event(SETTINGS_EVENT, SETTINGS_EVENT_BACK);
    };
    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)x; (void)y; (void)btn;
        SettingsScene* self = (SettingsScene*)arg;
        if (self->btn_back) button_set_pressed(self->btn_back, false);
    };
    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        SettingsScene* self = (SettingsScene*)arg;
        if (self->btn_back) button_is_hovered(self->btn_back, x, y);
    };
    dispatch_event(cbs, e, self);
}

void SettingsScene::Update(float dt) {
    (void)dt;
    sync_settings();
}

void SettingsScene::Render() {
    auto* self = this;
    Context* ctx = self->ctx();
    jr2d_p r2d = ctx->r2d;
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font28 = AssetManager::GetInstance()->GetSimheiFont28();

    // 标题
    jcolor_t title_c = { 30, 30, 40, 255 };
    jtext_printx(r2d, font28, "设置", (int)strlen("设置"), title_c, 216, 12);

    // 行标签
    jcolor_t label_c = { 50, 50, 60, 255 };
    jtext_printx(r2d, font24, "服务器 IP", (int)strlen("服务器 IP"), label_c, 20, 52);
    jtext_printx(r2d, font24, "端口", (int)strlen("端口"), label_c, 20, 92);
    jtext_printx(r2d, font24, "音乐", (int)strlen("音乐"), label_c, 20, 132);
    jtext_printx(r2d, font24, "音效", (int)strlen("音效"), label_c, 20, 172);

    // 音量百分比
    char pct[16];
    jcolor_t pct_c = { 50, 50, 60, 255 };
    snprintf(pct, sizeof(pct), "%d%%", (int)(slider_get_value(self->bgm_slider) * 100.0f));
    jtext_printx(r2d, font24, pct, (int)strlen(pct), pct_c, 330, 132);
    snprintf(pct, sizeof(pct), "%d%%", (int)(slider_get_value(self->sfx_slider) * 100.0f));
    jtext_printx(r2d, font24, pct, (int)strlen(pct), pct_c, 330, 172);

    // 控件（先画所有普通控件，下拉列表最后画以保证层级在最上层）
    if (self->ip_input) inputbox_draw(self->ip_input);
    if (self->port_input) inputbox_draw(self->port_input);
    if (self->bgm_slider) slider_draw(self->bgm_slider);
    if (self->sfx_slider) slider_draw(self->sfx_slider);
    if (self->fullscreen_cb) checkbox_draw(self->fullscreen_cb, r2d, font24);
    if (self->collision_cb) checkbox_draw(self->collision_cb, r2d, font24);
    if (self->btn_back) button_draw(self->btn_back);
}

void SettingsScene::Destroy() {
    auto* self = this;
    if (self->ip_input) inputbox_destroy(self->ip_input);
    if (self->port_input) inputbox_destroy(self->port_input);
    if (self->bgm_slider) slider_destroy(self->bgm_slider);
    if (self->sfx_slider) slider_destroy(self->sfx_slider);
    if (self->fullscreen_cb) checkbox_destroy(self->fullscreen_cb);
    if (self->collision_cb) checkbox_destroy(self->collision_cb);
    if (self->btn_back) button_destroy(self->btn_back);
}

void SettingsScene::Load() {
    auto* self = this;
    Context* ctx = self->ctx();
    log_info("[settings scene] start...");
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();


    // 服务器 IP 输入框（自由文本输入，彻底脱离 scene_node 弹层）
    self->ip_input = inputbox_create(ctx->r2d, 0.22917f, 0.15f, 0.52083f, 0.075f, font24);
    inputbox_set_text(self->ip_input, ctx->settings.server_ip);

    // 端口输入框
    self->port_input = inputbox_create(ctx->r2d, 0.22917f, 0.275f, 0.20833f, 0.075f, font24);
    char port_str[16];
    snprintf(port_str, sizeof(port_str), "%d", ctx->settings.server_port);
    inputbox_set_text(self->port_input, port_str);

    // 音量滑块
    self->bgm_slider = slider_create(ctx->r2d, 0.22917f, 0.42813f, 0.41667f, 0.04375f, 0.0f, 1.0f, ctx->settings.bgm_volume);
    self->sfx_slider = slider_create(ctx->r2d, 0.22917f, 0.55313f, 0.41667f, 0.04375f, 0.0f, 1.0f, ctx->settings.sfx_volume);
    slider_set_colors(self->bgm_slider, {200,200,200,255}, {50,50,60,255}, {80,80,80,255}, {100,100,100,255});
    slider_set_colors(self->sfx_slider, {200,200,200,255}, {50,50,60,255}, {80,80,80,255}, {100,100,100,255});

    // 全屏复选框
    self->fullscreen_cb = checkbox_create(0.22917f, 0.66250f, 0.04167f, "全屏");
    checkbox_set_r2d(self->fullscreen_cb, ctx->r2d);
    checkbox_set_colors(self->fullscreen_cb, {200,200,200,255}, {50,50,60,255}, {180,180,180,255});
    checkbox_set_state(self->fullscreen_cb,
            ctx->settings.fullscreen ? JOY_CHECKBOX_CHECKED : JOY_CHECKBOX_UNCHECKED);

    // 显示测试碰撞框复选框
    self->collision_cb = checkbox_create(0.22917f, 0.72500f, 0.04167f, "显示碰撞框");
    checkbox_set_r2d(self->collision_cb, ctx->r2d);
    checkbox_set_colors(self->collision_cb, {200,200,200,255}, {50,50,60,255}, {180,180,180,255});
    checkbox_set_state(self->collision_cb,
            ctx->settings.show_collision ? JOY_CHECKBOX_CHECKED : JOY_CHECKBOX_UNCHECKED);

    // 返回按钮
    self->btn_back = button_create(ctx->r2d,
            (190.0f + 50.0f) / 480.0f, (270.0f + 18.0f) / 320.0f,
            100.0f / 480.0f, 36.0f / 320.0f);
    UIStyle::applyFlatButton(self->btn_back);
    button_set_text_offset(self->btn_back, 0, 3.0f);
    button_set_textx(self->btn_back, font24, "返回", (int)strlen("返回"), { 0, 0, 0, 255 });
    }


