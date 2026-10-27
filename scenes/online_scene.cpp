#include "online_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"
#include "../network/lan_discovery.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

// 网页（Emscripten）无法开服 / 做局域网发现：创建房间置灰，提示改用直连。
static constexpr bool kIsWeb =
#if defined(JOY_EMSCRIPTEN) || defined(__EMSCRIPTEN__)
        true;
#else
        false;
#endif

// 设计稿 480x320（与选角页同一套基准），s = 逻辑高/320
static constexpr float BAND_H   = 36.0f;    // 顶部墨色标题带
static constexpr float LIST_X   = 12.0f;    // 左侧房间列表
static constexpr float LIST_Y   = 44.0f;
static constexpr float LIST_W   = 180.0f;
static constexpr float LIST_H   = 230.0f;
static constexpr float ROW_H    = 44.0f;
static constexpr float ROW_GAP  = 6.0f;
static constexpr float PANEL_X  = 200.0f;   // 右侧详情面板
static constexpr float PANEL_Y  = 44.0f;
static constexpr float PANEL_H  = 230.0f;
static constexpr float BTN_Y    = 280.0f;   // 底部按钮条（底部留边距）
static constexpr float BTN_H    = 30.0f;
static constexpr float BTN_W    = 108.0f;
static constexpr float BTN_GAP  = 6.0f;
static constexpr float BTN_START= 20.0f;

// 顶部右上角「直连」小按钮
static constexpr float DOPEN_W = 68.0f, DOPEN_H = 24.0f, DOPEN_RIGHT = 42.0f;

// 直连弹窗（居中卡片）
static constexpr float DLG_X = 90.0f, DLG_Y = 85.0f, DLG_W = 300.0f, DLG_H = 150.0f;
static constexpr float DLG_IP_X = 150.0f, DLG_IP_Y = 128.0f, DLG_IP_W = 220.0f, DLG_ROW_H = 28.0f;
static constexpr float DLG_PORT_X = 150.0f, DLG_PORT_Y = 166.0f, DLG_PORT_W = 110.0f;
static constexpr float DLG_BTN_Y = 212.0f, DLG_BTN_W = 90.0f, DLG_BTN_H = 26.0f;
static constexpr float DLG_CANCEL_X = 196.0f, DLG_CONNECT_X = 316.0f;

OnlineScene::OnlineScene(Context* ctx) : Scene(ctx, "Online") {}

void OnlineScene::layout() {
    int w, h;
    r2d_get_logical_presentation(this->ctx()->r2d, &w, &h, NULL);
    const float LH = (float)h, s = LH / 320.0f;
    list_x = LIST_X * s;
    list_y = LIST_Y * s;
    list_w = LIST_W * s;
    list_h = LIST_H * s;
    row_x = list_x + 6.0f * s;
    row_w = list_w - 12.0f * s;
    row_top = list_y + 6.0f * s;
    row_h = ROW_H * s;
    row_pitch = (ROW_H + ROW_GAP) * s;
}

void OnlineScene::Load() {
    Context* ctx = this->ctx();
    log_info("[lobby] start...");

    // 开始监听局域网广播（离开大厅时 Destroy 停止）
    LanDiscovery::Get().start();

    int w, h;
    r2d_get_logical_presentation(ctx->r2d, &w, &h, NULL);
    const float LW = (float)w, LH = (float)h, s = LH / 320.0f;
    AssetManager* am = AssetManager::GetInstance();
    font_p font24 = am->GetSimheiFont24();   // 48px
    font_p font12 = am->GetSimheiFont12();   // 28px

    // ---- 底部操作条：创建房间 / 加入房间 / 刷新 / 返回 ----
    auto make = [&](const char* label, int i, bool dark) -> button_p {
        float cx = BTN_START + (BTN_W + BTN_GAP) * (float)i + BTN_W * 0.5f;
        float cy = BTN_Y + BTN_H * 0.5f;
        button_p b = button_create(ctx->r2d,
                                   (cx * s) / LW, (cy * s) / LH,
                                   (BTN_W * s) / LW, (BTN_H * s) / LH);
        if (dark) {
            button_set_normal_color(b, { 40, 40, 48, 255 });
            button_set_hover_color(b, { 64, 64, 74, 255 });
            button_set_pressed_color(b, { 24, 24, 30, 255 });
            button_set_textx(b, font24, label, (int)strlen(label), { 245, 243, 237, 255 });
        } else {
            UIStyle::applyPaperButton(b);
            button_set_textx(b, font24, label, (int)strlen(label), { 0, 0, 0, 255 });
        }
        button_set_text_offset(b, 0, 3.0f * s);
        return b;
    };
    btn_create  = make(kIsWeb ? "不能开房" : "创建房间", 0, !kIsWeb);
    btn_join    = make("加入房间", 1, false);
    btn_refresh = make("刷新",     2, false);
    btn_back    = make("返回",     3, false);

    if (kIsWeb) {
        jcolor_t dis = { 214, 216, 222, 255 };
        button_set_normal_color(btn_create, dis);
        button_set_hover_color(btn_create, dis);
        button_set_pressed_color(btn_create, dis);
        button_set_textx(btn_create, font12, "不能开房", (int)strlen("不能开房"), { 150, 153, 160, 255 });
    }

    // ---- 顶部右上角「直连」 ----
    btn_direct_open = button_create(ctx->r2d,
                                    ((LW / s) - DOPEN_RIGHT) * s / LW,
                                    (18.0f * s) / LH,
                                    (DOPEN_W * s) / LW, (DOPEN_H * s) / LH);
    UIStyle::applyPaperButton(btn_direct_open);
    button_set_textx(btn_direct_open, font12, "直连", (int)strlen("直连"), { 0, 0, 0, 255 });
    button_set_text_offset(btn_direct_open, 0, 2.0f * s);

    // ---- 直连弹窗的输入与按钮 ----
    ip_input = inputbox_create(ctx->r2d,
                               (DLG_IP_X * s) / LW, (DLG_IP_Y * s) / LH,
                               (DLG_IP_W * s) / LW, (DLG_ROW_H * s) / LH, font12);
    inputbox_set_text(ip_input, ctx->settings.server_ip);

    port_input = inputbox_create(ctx->r2d,
                                 (DLG_PORT_X * s) / LW, (DLG_PORT_Y * s) / LH,
                                 (DLG_PORT_W * s) / LW, (DLG_ROW_H * s) / LH, font12);
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%d", ctx->settings.server_port);
    inputbox_set_text(port_input, pbuf);

    btn_cancel = button_create(ctx->r2d,
                               (DLG_CANCEL_X * s) / LW, (DLG_BTN_Y * s) / LH,
                               (DLG_BTN_W * s) / LW, (DLG_BTN_H * s) / LH);
    UIStyle::applyPaperButton(btn_cancel);
    button_set_textx(btn_cancel, font24, "取消", (int)strlen("取消"), { 0, 0, 0, 255 });
    button_set_text_offset(btn_cancel, 0, 3.0f * s);

    btn_connect = button_create(ctx->r2d,
                                (DLG_CONNECT_X * s) / LW, (DLG_BTN_Y * s) / LH,
                                (DLG_BTN_W * s) / LW, (DLG_BTN_H * s) / LH);
    button_set_normal_color(btn_connect, { 40, 40, 48, 255 });
    button_set_hover_color(btn_connect, { 64, 64, 74, 255 });
    button_set_pressed_color(btn_connect, { 24, 24, 30, 255 });
    button_set_textx(btn_connect, font24, "连接", (int)strlen("连接"), { 245, 243, 237, 255 });
    button_set_text_offset(btn_connect, 0, 3.0f * s);
}

void OnlineScene::Update(float dt) {
    (void)dt;
    LanDiscovery::Get().update();   // 收包 + 过期清理（非阻塞）

    const auto& rooms = LanDiscovery::Get().rooms();
    if (selected_ >= (int)rooms.size()) selected_ = (int)rooms.size() - 1;
    if (selected_ < 0 && !rooms.empty()) selected_ = 0;   // 默认选第一个
}

void OnlineScene::join_selected() {
    Context* ctx = this->ctx();
    const auto& rooms = LanDiscovery::Get().rooms();
    if (selected_ < 0 || selected_ >= (int)rooms.size()) {
        log_info("[lobby] no room selected");
        return;
    }
    const RoomInfo& r = rooms[selected_];
    snprintf(ctx->settings.server_ip, sizeof(ctx->settings.server_ip), "%s", r.ip.c_str());
    ctx->settings.server_port = r.port;
    settings_save(&ctx->settings);
    log_info("[lobby] join %s:%d", ctx->settings.server_ip, ctx->settings.server_port);
    push_user_event(ONLINE_EVENT, ONLINE_EVENT_CLIENT);
}

void OnlineScene::direct_connect() {
    Context* ctx = this->ctx();
    const char* ip = ip_input ? inputbox_get_text(ip_input) : "";
    int port = port_input ? atoi(inputbox_get_text(port_input)) : 0;
    if (!ip || !ip[0] || port <= 0 || port > 65535) {
        log_info("[lobby] direct connect invalid ip/port");
        return;   // 无效则保持弹窗打开
    }
    snprintf(ctx->settings.server_ip, sizeof(ctx->settings.server_ip), "%s", ip);
    ctx->settings.server_port = port;
    settings_save(&ctx->settings);
    direct_open_ = false;
    log_info("[lobby] direct connect %s:%d", ctx->settings.server_ip, ctx->settings.server_port);
    push_user_event(ONLINE_EVENT, ONLINE_EVENT_CLIENT);
}

void OnlineScene::HandleUiEvent(const void* e) {
    jevent_callbacks_t cbs = {};
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        OnlineScene* self = (OnlineScene*)arg;
        Context* ctx = self->ctx();

        // ---------- 直连弹窗打开时：只处理弹窗 ----------
        if (self->direct_open_) {
            bool hit_ip   = self->ip_input   && inputbox_is_hovered(self->ip_input, x, y);
            bool hit_port = self->port_input && inputbox_is_hovered(self->port_input, x, y);
            if (hit_ip)        inputbox_is_pressed(self->ip_input, x, y);
            else if (hit_port) inputbox_is_pressed(self->port_input, x, y);
            else {
                if (self->ip_input)   inputbox_defocus(self->ip_input);
                if (self->port_input) inputbox_defocus(self->port_input);
            }

            if (self->btn_cancel && ui_button_hit(self->btn_cancel, x, y)) {
                self->direct_open_ = false;
                return;
            }
            if (self->btn_connect && ui_button_hit(self->btn_connect, x, y)) {
                self->direct_connect();
                return;
            }

            // 点击卡片外 → 关闭
            int lw = 0, lh = 0;
            r2d_get_logical_presentation(ctx->r2d, &lw, &lh, NULL);
            const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
            vec2_t logic = r2d_coordinates_from_window(ctx->r2d, x, y);
            float dx = DLG_X * s, dy = DLG_Y * s;
            float dw = DLG_W * s, dh = DLG_H * s;
            if (logic.x < dx || logic.x > dx + dw || logic.y < dy || logic.y > dy + dh)
                self->direct_open_ = false;
            return;
        }

        // ---------- 大厅常规 ----------
        if (!kIsWeb && self->btn_create && ui_button_hit(self->btn_create, x, y))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_HOST);
        else if (self->btn_join && ui_button_hit(self->btn_join, x, y))
            self->join_selected();
        else if (self->btn_refresh && ui_button_hit(self->btn_refresh, x, y))
            LanDiscovery::Get().update();
        else if (self->btn_direct_open && ui_button_hit(self->btn_direct_open, x, y))
            self->direct_open_ = true;
        else if (self->btn_back && ui_button_hit(self->btn_back, x, y))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_BACK);
        else {
            // 房间行点选：窗口像素 -> 逻辑坐标
            vec2_t logic = r2d_coordinates_from_window(ctx->r2d, x, y);
            self->layout();
            const auto& rooms = LanDiscovery::Get().rooms();
            int max_rows = (int)((self->list_y + self->list_h - self->row_top) / self->row_pitch);
            if (max_rows < 0) max_rows = 0;
            if (logic.x >= self->row_x && logic.x <= self->row_x + self->row_w &&
                logic.y >= self->row_top && logic.y <= self->row_top + self->row_pitch * (float)max_rows) {
                int idx = (int)((logic.y - self->row_top) / self->row_pitch);
                if (idx >= 0 && idx < max_rows && idx < (int)rooms.size()) {
                    if (self->selected_ != idx) UiSfx::Get().play(UiSfx::kClick);
                    self->selected_ = idx;
                }
            }
        }
    };

    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)x; (void)y; (void)btn;
        OnlineScene* self = (OnlineScene*)arg;
        if (self->btn_create)      button_set_pressed(self->btn_create, false);
        if (self->btn_join)        button_set_pressed(self->btn_join, false);
        if (self->btn_refresh)     button_set_pressed(self->btn_refresh, false);
        if (self->btn_back)        button_set_pressed(self->btn_back, false);
        if (self->btn_direct_open) button_set_pressed(self->btn_direct_open, false);
        if (self->btn_cancel)      button_set_pressed(self->btn_cancel, false);
        if (self->btn_connect)     button_set_pressed(self->btn_connect, false);
    };

    cbs.on_process_event = [](const void* ev, void* arg) {
        OnlineScene* self = (OnlineScene*)arg;
        if (!self->direct_open_) return;
        if (self->ip_input)   inputbox_process_event(self->ip_input, ev);
        if (self->port_input) inputbox_process_event(self->port_input, ev);
    };

    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        OnlineScene* self = (OnlineScene*)arg;
        if (self->btn_create)      button_is_hovered(self->btn_create, x, y);
        if (self->btn_join)        button_is_hovered(self->btn_join, x, y);
        if (self->btn_refresh)     button_is_hovered(self->btn_refresh, x, y);
        if (self->btn_back)        button_is_hovered(self->btn_back, x, y);
        if (self->btn_direct_open) button_is_hovered(self->btn_direct_open, x, y);
        if (self->btn_cancel)      button_is_hovered(self->btn_cancel, x, y);
        if (self->btn_connect)     button_is_hovered(self->btn_connect, x, y);
        if (self->direct_open_) {
            if (self->ip_input)   inputbox_is_hovered(self->ip_input, x, y);
            if (self->port_input) inputbox_is_hovered(self->port_input, x, y);
        }
    };

    dispatch_event(cbs, e, this);
}

void OnlineScene::Render() {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    AssetManager* am = AssetManager::GetInstance();
    font_p font24 = am->GetSimheiFont24();
    font_p font12 = am->GetSimheiFont12();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);
    const float LW = (float)w, LH = (float)h, s = LH / 320.0f;

    // 米色纸底（与选角页一致）
    r2d_set_color(r2d, 238, 234, 226, 255);
    shape_draw_rectangle(r2d, "fill", { 0, 0, LW, LH });

    // 顶部墨色标题带
    r2d_set_color(r2d, 40, 40, 48, 255);
    shape_draw_rectangle(r2d, "fill", { 0, 0, LW, BAND_H * s });
    jtext_printx(r2d, font24, "联机大厅", (int)strlen("联机大厅"),
                 { 245, 243, 237, 255 }, 16.0f * s, 6.0f * s);

    layout();
    const auto& rooms = LanDiscovery::Get().rooms();

    // ---- 左侧房间列表 ----
    {
        float px = list_x, py = list_y, pw = list_w, ph = list_h;
        r2d_set_color(r2d, 0, 0, 0, 22);
        shape_draw_rectangle(r2d, "fill", { px + 2.0f * s, py + 2.0f * s, pw, ph });
        r2d_set_color(r2d, 250, 248, 242, 255);
        shape_draw_rectangle(r2d, "fill", { px, py, pw, ph });
        r2d_set_color(r2d, 150, 146, 138, 255);
        shape_draw_rectangle(r2d, "line", { px, py, pw, ph });

        int max_rows = (int)((list_y + list_h - row_top) / row_pitch);
        if (max_rows < 0) max_rows = 0;

        if (rooms.empty()) {
            const char* t1 = kIsWeb ? "网页版无列表" : "暂无房间";
            jtext_printx(r2d, font12, t1, (int)strlen(t1), { 150, 146, 138, 255 },
                         px + 12.0f * s, py + ph * 0.5f - 10.0f * s);
        } else {
            for (int i = 0; i < (int)rooms.size() && i < max_rows; ++i) {
                float rx = row_x, ry = row_top + row_pitch * (float)i;
                float rw = row_w, rh = row_h;
                bool sel = (i == selected_);

                r2d_set_color(r2d, 0, 0, 0, 30);
                shape_draw_rectangle(r2d, "fill", { rx + 2.0f * s, ry + 2.0f * s, rw, rh });
                jcolor_t bc = sel ? jcolor_t{ 38, 38, 46, 255 } : jcolor_t{ 176, 172, 164, 255 };
                r2d_set_color(r2d, bc.r, bc.g, bc.b, 255);
                shape_draw_rectangle(r2d, "fill", { rx, ry, rw, rh });
                float bt = sel ? 2.5f * s : 1.0f * s;
                r2d_set_color(r2d, 252, 250, 244, 255);
                shape_draw_rectangle(r2d, "fill", { rx + bt, ry + bt, rw - 2 * bt, rh - 2 * bt });
                if (sel) {
                    r2d_set_color(r2d, 200, 60, 50, 255);
                    shape_draw_rectangle(r2d, "fill", { rx + bt, ry + bt, 4.0f * s, rh - 2 * bt });
                }

                const RoomInfo& r = rooms[i];
                jcolor_t nc = sel ? jcolor_t{ 28, 28, 32, 255 } : jcolor_t{ 40, 40, 45, 255 };
                jtext_printx(r2d, font12, r.name.c_str(), (int)r.name.size(), nc,
                             rx + 12.0f * s, ry + 5.0f * s);

                char buf[96];
                snprintf(buf, sizeof(buf), "%s:%d", r.ip.c_str(), r.port);
                jtext_printx(r2d, font12, buf, (int)strlen(buf), { 120, 120, 128, 255 },
                             rx + 12.0f * s, ry + 24.0f * s);

                snprintf(buf, sizeof(buf), "%d/%d", r.players, r.max_players);
                int pw2 = 0, ph2 = 0;
                jtext_measurex(r2d, font12, buf, (int)strlen(buf), { 120, 120, 128, 255 }, &pw2, &ph2);
                jtext_printx(r2d, font12, buf, (int)strlen(buf), { 120, 120, 128, 255 },
                             rx + rw - 10.0f * s - (float)pw2, ry + 5.0f * s);
            }
        }
    }

    // ---- 右侧详情面板 ----
    {
        float px = PANEL_X * s, py = PANEL_Y * s;
        float pw = LW - px - 12.0f * s;
        float ph = PANEL_H * s;
        r2d_set_color(r2d, 0, 0, 0, 26);
        shape_draw_rectangle(r2d, "fill", { px + 3.0f * s, py + 3.0f * s, pw, ph });
        r2d_set_color(r2d, 250, 248, 242, 255);
        shape_draw_rectangle(r2d, "fill", { px, py, pw, ph });
        r2d_set_color(r2d, 150, 146, 138, 255);
        shape_draw_rectangle(r2d, "line", { px, py, pw, ph });

        const RoomInfo* sel = (selected_ >= 0 && selected_ < (int)rooms.size())
            ? &rooms[selected_] : nullptr;
        if (sel) {
            jtext_printx(r2d, font24, sel->name.c_str(), (int)sel->name.size(),
                         { 28, 28, 32, 255 }, px + 10.0f * s, py + 10.0f * s);
            char buf[128];
            snprintf(buf, sizeof(buf), "主机  %s:%d", sel->ip.c_str(), sel->port);
            jtext_printx(r2d, font12, buf, (int)strlen(buf), { 90, 90, 98, 255 },
                         px + 10.0f * s, py + 48.0f * s);
            snprintf(buf, sizeof(buf), "人数  %d/%d", sel->players, sel->max_players);
            jtext_printx(r2d, font12, buf, (int)strlen(buf), { 90, 90, 98, 255 },
                         px + 10.0f * s, py + 68.0f * s);
            snprintf(buf, sizeof(buf), "地图  %d", sel->map_id);
            jtext_printx(r2d, font12, buf, (int)strlen(buf), { 90, 90, 98, 255 },
                         px + 10.0f * s, py + 88.0f * s);
        } else {
            const char* t = kIsWeb ? "网页版请用右上角『直连』加入服务器" : "选择左侧房间查看详情";
            jtext_printx(r2d, font12, t, (int)strlen(t), { 150, 146, 138, 255 },
                         px + 10.0f * s, py + 20.0f * s);
        }

        // 底部提示（替代原先面板里的直连行，保持面板干净）
        const char* tip = kIsWeb ? "网页版：请用右上角『直连』加入"
                                 : "同网段房主开房后自动出现";
        jtext_printx(r2d, font12, tip, (int)strlen(tip), { 165, 161, 152, 255 },
                     px + 10.0f * s, py + 190.0f * s);
    }

    // ---- 控件 ----
    if (btn_direct_open) button_draw(btn_direct_open);
    if (btn_create) button_draw(btn_create);
    if (btn_join)   button_draw(btn_join);
    if (btn_refresh)button_draw(btn_refresh);
    if (btn_back)   button_draw(btn_back);

    // ---- 直连弹窗（最上层） ----
    if (direct_open_) {
        // 背板
        r2d_set_color(r2d, 0, 0, 0, 120);
        shape_draw_rectangle(r2d, "fill", { 0, 0, LW, LH });

        float dx = DLG_X * s, dy = DLG_Y * s, dw = DLG_W * s, dh = DLG_H * s;
        r2d_set_color(r2d, 0, 0, 0, 40);
        shape_draw_rectangle(r2d, "fill", { dx + 3.0f * s, dy + 3.0f * s, dw, dh });
        r2d_set_color(r2d, 250, 248, 242, 255);
        shape_draw_rectangle(r2d, "fill", { dx, dy, dw, dh });
        r2d_set_color(r2d, 150, 146, 138, 255);
        shape_draw_rectangle(r2d, "line", { dx, dy, dw, dh });

        jtext_printx(r2d, font24, "直连服务器", (int)strlen("直连服务器"),
                     { 28, 28, 32, 255 }, (DLG_X + 16.0f) * s, (DLG_Y + 12.0f) * s);
        jtext_printx(r2d, font12, "IP 地址", (int)strlen("IP 地址"),
                     { 90, 90, 98, 255 }, (DLG_X + 16.0f) * s, (DLG_Y + 49.0f) * s);
        jtext_printx(r2d, font12, "端口", (int)strlen("端口"),
                     { 90, 90, 98, 255 }, (DLG_X + 16.0f) * s, (DLG_Y + 87.0f) * s);

        if (ip_input)   inputbox_draw(ip_input);
        if (port_input) inputbox_draw(port_input);
        if (btn_cancel) button_draw(btn_cancel);
        if (btn_connect)button_draw(btn_connect);
    }
}

void OnlineScene::Destroy() {
    LanDiscovery::Get().stop();
    if (btn_create)      button_destroy(btn_create);
    if (btn_join)        button_destroy(btn_join);
    if (btn_refresh)     button_destroy(btn_refresh);
    if (btn_back)        button_destroy(btn_back);
    if (btn_direct_open) button_destroy(btn_direct_open);
    if (btn_connect)     button_destroy(btn_connect);
    if (btn_cancel)      button_destroy(btn_cancel);
    if (ip_input)        inputbox_destroy(ip_input);
    if (port_input)      inputbox_destroy(port_input);
}
