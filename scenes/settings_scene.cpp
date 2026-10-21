#include <string.h>
#include <cstdlib>
#include "settings_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"

// 大小写不敏感的字符串相等判断（跨平台）。
// 不用 MSVC 专有的 _stricmp，也不用 POSIX 的 strcasecmp（三端头文件/可用性不一致），
// 自行实现只处理 ASCII 大小写，足以用于比较文件扩展名。
static bool str_ieq(const char* a, const char* b) {
    if (!a || !b) return false;
    for (; *a && *b; ++a, ++b) {
        char ca = (*a >= 'A' && *a <= 'Z') ? (char)(*a - 'A' + 'a') : *a;
        char cb = (*b >= 'A' && *b <= 'Z') ? (char)(*b - 'A' + 'a') : *b;
        if (ca != cb) return false;
    }
    return *a == '\0' && *b == '\0';
}

static void apply_fullscreen(Context* ctx) {
    window_set_fullscreen(ctx->window, ctx->settings.fullscreen);
}

// 把当前 UI 值同步到 settings 并自动保存（变化即写盘）
void SettingsScene::sync_settings() {
    Context* ctx = this->ctx();
    bool changed = false;

    const char* ip = combobox_input_get_text(ip_combo);
    if (!ip) ip = "";
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
        if (preview_bgm) audio_bgm_set_gain(preview_bgm, bgm);   // 设置页试听流同步音量
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
        // 下拉项的鼠标点选：combobox_input_process_event 内部只处理键盘
        // （Enter / 上下键）选中，点项需调用方用 hit_item 处理；而且必须先截获，
        // 否则 item 区域在框外，会被当成"点击外部"直接把列表收起。
        if (self->ip_combo) {
                const SDL_Event* e = (const SDL_Event*)ev;
                if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
                        int idx = combobox_input_hit_item(self->ip_combo,
                                (float)e->button.x, (float)e->button.y);
                        if (idx >= 0 && idx < self->ip_item_count) {
                                combobox_input_set_text(self->ip_combo, self->ip_items[idx]);
                                combobox_input_set_open(self->ip_combo, false);
                                self->sync_settings();
                                return;
                        }
                }
                combobox_input_process_event(self->ip_combo, ev);
        }
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
        // 输入框焦点切换必须在这里先处理（dispatch_event 先调 on_mousepressed，再统一调
        // on_process_event）。若交给 inputbox_process_event 处理：未命中的框会走
        // inputbox_defocus -> SDL_StopTextInput，同一帧紧接着命中框 inputbox_focus ->
        // SDL_StartTextInput；Android 的 IME hide/show 异步竞态会吞掉 show，出现
        // "从 A 直接点 B 唤不起输入法，必须先点框外让 A 失焦"的问题。
        // 这里：命中框聚焦并定位光标，另一个框仅 inputbox_blur（不动 SDL，键盘保持不收起）；
        // 只有点到两个框之外，才 inputbox_defocus 真正收起键盘。
        // IP 已改为下拉框，焦点/展开由 combobox_input_process_event 自己处理，这里只管端口框
        bool hit_port = self->port_input && inputbox_is_hovered(self->port_input, x, y);
        if (hit_port) {
            inputbox_is_pressed(self->port_input, x, y);    // 聚焦 + 定位光标
        } else {
            if (self->port_input) inputbox_defocus(self->port_input);
        }
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
    // 以下坐标全部比例化（原值为设计稿 480x320 下的像素）
    jtext_printf(r2d, font28, "设置", (int)strlen("设置"), title_c,
                 216.0f / 480.0f, 12.0f / 320.0f);

    // 用栅格逐行绘制左列标签；右列控件由 Load 按同一栅格创建，这里仅补标签与音量百分比
    int rlw = 0, rlh = 0;
    r2d_get_logical_presentation(r2d, &rlw, &rlh, NULL);
    const float s  = (rlh > 0) ? ((float)rlh / 320.0f) : 1.0f;
    const float LW = (float)((rlw > 0) ? rlw : 960);
    const float LH = (float)((rlh > 0) ? rlh : 640);
    float cols[2] = { 0.30f, 0.70f };
    jlayout_grid_t* g = jlayout_grid_create(24.0f * s, 52.0f * s,
            LW - 48.0f * s, 34.0f * s, 2, cols, 8.0f * s);
    jcolor_t label_c = { 50, 50, 60, 255 };
    float cx, cy, cw, ch;

    jlayout_grid_label(g, r2d, font24, "服务器IP", (int)strlen("服务器IP"), label_c, 0, &cx, &cy, &cw, &ch);
    jlayout_grid_next_row(g);
    jlayout_grid_label(g, r2d, font24, "端口", (int)strlen("端口"), label_c, 0, &cx, &cy, &cw, &ch);
    jlayout_grid_next_row(g);
    jlayout_grid_label(g, r2d, font24, "音乐", (int)strlen("音乐"), label_c, 0, &cx, &cy, &cw, &ch);
    {
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", (int)(slider_get_value(self->bgm_slider) * 100.0f));
        int tw = 0, th = 0;
        jtext_measurex(r2d, font24, pct, (int)strlen(pct), label_c, &tw, &th);
        jtext_printf(r2d, font24, pct, (int)strlen(pct), label_c,
                     (self->bgm_pct_rx - (float)tw) / LW, (self->bgm_pct_cy - (float)th * 0.5f) / LH);
    }
    jlayout_grid_next_row(g);
    jlayout_grid_label(g, r2d, font24, "音效", (int)strlen("音效"), label_c, 0, &cx, &cy, &cw, &ch);
    {
        char pct[16];
        snprintf(pct, sizeof(pct), "%d%%", (int)(slider_get_value(self->sfx_slider) * 100.0f));
        int tw = 0, th = 0;
        jtext_measurex(r2d, font24, pct, (int)strlen(pct), label_c, &tw, &th);
        jtext_printf(r2d, font24, pct, (int)strlen(pct), label_c,
                     (self->sfx_pct_rx - (float)tw) / LW, (self->sfx_pct_cy - (float)th * 0.5f) / LH);
    }
    jlayout_grid_destroy(g);

    // 控件（先画所有普通控件，下拉列表最后画以保证层级在最上层）
    if (self->ip_combo) combobox_input_draw(self->ip_combo);
    if (self->port_input) inputbox_draw(self->port_input);
    if (self->bgm_slider) slider_draw(self->bgm_slider);
    if (self->sfx_slider) slider_draw(self->sfx_slider);
    if (self->fullscreen_cb) checkbox_draw(self->fullscreen_cb, r2d, font24);
    if (self->collision_cb) checkbox_draw(self->collision_cb, r2d, font24);
    if (self->btn_back) button_draw(self->btn_back);

    // 下拉列表最后画，保证层级在最上层
    if (self->ip_combo) combobox_input_draw_dropdown(self->ip_combo);
}

void SettingsScene::Destroy() {
    auto* self = this;
    // 停止并释放 BGM 试听（独立设备），避免回到菜单后仍响
    if (preview_bgm) { audio_bgm_destroy(preview_bgm); preview_bgm = NULL; }
    if (preview_dev) { audio_close_device(preview_dev); preview_dev = 0; }
    if (self->ip_combo) combobox_input_destroy(self->ip_combo);
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


    // 两列栅格：左标签 / 右控件，按逻辑像素铺满宽度（左右各留 24*s）。
    // 宽屏下表单拉伸到右边界，右侧不再有大块留白；列宽比固定，标签长短不影响控件位置。
    int rlw = 0, rlh = 0;
    r2d_get_logical_presentation(ctx->r2d, &rlw, &rlh, NULL);
    const float s  = (rlh > 0) ? ((float)rlh / 320.0f) : 1.0f;
    const float LW = (float)((rlw > 0) ? rlw : 960);
    const float LH = (float)((rlh > 0) ? rlh : 640);
    float cols[2] = { 0.30f, 0.70f };
    jlayout_grid_t* g = jlayout_grid_create(24.0f * s, 52.0f * s,
            LW - 48.0f * s, 34.0f * s, 2, cols, 8.0f * s);
    jcolor_t lab_c = { 50, 50, 60, 255 };
    float cx, cy, cw, ch;

    // 服务器 IP
    jlayout_grid_cell(g, 1, &cx, &cy, &cw, &ch);
    self->ip_combo = combobox_input_create(ctx->r2d, cx / LW, cy / LH, cw / LW, ch / LH, font24, JOY_MAX_IP);
    combobox_input_set_text(self->ip_combo, ctx->settings.server_ip);
    self->ip_item_count = get_local_ips(self->ip_items, 8);
    for (int i = 0; i < self->ip_item_count; i++)
        combobox_input_add_item(self->ip_combo, self->ip_items[i],
                                (int)strlen(self->ip_items[i]));
    jlayout_grid_next_row(g);

    // 端口
    jlayout_grid_cell(g, 1, &cx, &cy, &cw, &ch);
    self->port_input = inputbox_create(ctx->r2d, cx / LW, cy / LH, cw / LW, ch / LH, font24);
    {
        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", ctx->settings.server_port);
        inputbox_set_text(self->port_input, port_str);
    }
    jlayout_grid_next_row(g);

    // 音乐
    jlayout_grid_cell(g, 1, &cx, &cy, &cw, &ch);
    self->bgm_slider = slider_create(ctx->r2d, cx / LW, cy / LH, cw / LW, ch / LH, 0.0f, 1.0f, ctx->settings.bgm_volume);
    slider_set_colors(self->bgm_slider, {200,200,200,255}, {50,50,60,255}, {80,80,80,255}, {100,100,100,255});
    self->bgm_pct_rx = cx + cw - 4.0f * s; self->bgm_pct_cy = cy + ch * 0.5f;
    jlayout_grid_next_row(g);

    // 音效
    jlayout_grid_cell(g, 1, &cx, &cy, &cw, &ch);
    self->sfx_slider = slider_create(ctx->r2d, cx / LW, cy / LH, cw / LW, ch / LH, 0.0f, 1.0f, ctx->settings.sfx_volume);
    slider_set_colors(self->sfx_slider, {200,200,200,255}, {50,50,60,255}, {80,80,80,255}, {100,100,100,255});
    self->sfx_pct_rx = cx + cw - 4.0f * s; self->sfx_pct_cy = cy + ch * 0.5f;
    jlayout_grid_next_row(g);

    // 设置页 BGM 试听：自建独立音频设备播放 bgm（与游戏场景的 BGM 设备互不干扰），
    // 进设置页即开始循环播放，使"音乐"滑块在此即可实时听到音量变化。返回时 Destroy 停止。
    {
        auto* am = AssetManager::GetInstance();
        const char* bgm_path = am ? am->GetResourcePath(AssetManager::kCatAudio, "bgm")
                                  : "joy_topdown_template/audio/bgm.wav";
        if (bgm_path) {
            preview_dev = audio_open_device();
            if (preview_dev) {
                // 按扩展名分派解码器：bgm.wav 是 WAV，必须走 audio_bgm_create(SDL_LoadWAV)；
                // 直接喂 audio_bgm_create_ogg 会因 OGG 解码器把 WAV 头当 OGG 读取而越界崩溃。
                const char* dot = strrchr(bgm_path, '.');
                if      (dot && str_ieq(dot, ".ogg"))  preview_bgm = audio_bgm_create_ogg(bgm_path, preview_dev);
                else if (dot && str_ieq(dot, ".mp3"))  preview_bgm = audio_bgm_create_mp3(bgm_path, preview_dev);
                else if (dot && str_ieq(dot, ".flac")) preview_bgm = audio_bgm_create_flac(bgm_path, preview_dev);
                else                                        preview_bgm = audio_bgm_create(bgm_path, preview_dev);
                if (preview_bgm) audio_bgm_set_gain(preview_bgm, ctx->settings.bgm_volume);
                else { audio_close_device(preview_dev); preview_dev = 0; }
            }
        }
    }

    // 全屏复选框（标签由 checkbox 自带，置于第 1 列）
    jlayout_grid_cell(g, 1, &cx, &cy, &cw, &ch);
    self->fullscreen_cb = checkbox_create(cx / LW, cy / LH, (20.0f * s) / LW, "全屏");
    checkbox_set_r2d(self->fullscreen_cb, ctx->r2d);
    checkbox_set_colors(self->fullscreen_cb, {200,200,200,255}, {50,50,60,255}, {180,180,180,255});
    checkbox_set_state(self->fullscreen_cb,
            ctx->settings.fullscreen ? JOY_CHECKBOX_CHECKED : JOY_CHECKBOX_UNCHECKED);
    jlayout_grid_next_row(g);

    // 显示测试碰撞框
    jlayout_grid_cell(g, 1, &cx, &cy, &cw, &ch);
    self->collision_cb = checkbox_create(cx / LW, cy / LH, (20.0f * s) / LW, "显示碰撞框");
    checkbox_set_r2d(self->collision_cb, ctx->r2d);
    checkbox_set_colors(self->collision_cb, {200,200,200,255}, {50,50,60,255}, {180,180,180,255});
    checkbox_set_state(self->collision_cb,
            ctx->settings.show_collision ? JOY_CHECKBOX_CHECKED : JOY_CHECKBOX_UNCHECKED);
    jlayout_grid_next_row(g);

    jlayout_grid_destroy(g);

    // 返回按钮（底部居中，固定比例坐标）
    self->btn_back = button_create(ctx->r2d,
            (190.0f + 50.0f) / 480.0f, (270.0f + 18.0f) / 320.0f,
            100.0f / 480.0f, 36.0f / 320.0f);
    UIStyle::applyFlatButton(self->btn_back);
    button_set_text_offset(self->btn_back, 0, 3.0f);
    button_set_textx(self->btn_back, font24, "返回", (int)strlen("返回"), { 0, 0, 0, 255 });
    }


