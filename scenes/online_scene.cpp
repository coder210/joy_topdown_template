#include "online_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"
#include "../network/lan_discovery.h"
#include <cstdio>
#include <cstring>
#include <cstdlib>

OnlineScene::OnlineScene(Context* ctx) : Scene(ctx, "Online") {}

void OnlineScene::layout() {
    int w, h;
    r2d_get_logical_presentation(this->ctx()->r2d, &w, &h, NULL);
    const float LW = (float)w, LH = (float)h, s = LH / 320.0f;
    list_x = LW * 0.05f;
    list_y = LH * 0.10f;
    list_w = LW * 0.90f;
    list_h = LH * 0.54f;
    header_h = 24.0f * s;   // 表头高
    row_h = 24.0f * s;      // 行高（触屏可点）
}

void OnlineScene::Load() {
    Context* ctx = this->ctx();
    log_info("[lobby] start...");

    // 开始监听局域网广播（离开大厅时 Destroy 停止）
    LanDiscovery::Get().start();

    int w, h;
    r2d_get_logical_presentation(ctx->r2d, &w, &h, NULL);
    const float LW = (float)w, LH = (float)h, s = LH / 320.0f;
    font_p font = AssetManager::GetInstance()->GetSimheiFont24();

    // ---- 底部主操作条：创建房间(主色) / 加入选中 / 刷新 / 返回 ----
    const int nbtn = 4;
    const float gap = 12.0f * s;
    const float px = LW * 0.05f;
    const float pw = LW * 0.90f;
    const float bw = (pw - gap * (float)(nbtn - 1)) / (float)nbtn;
    const float bh = 34.0f * s;
    const float by = LH * 0.68f;

    auto make = [&](button_p& b, int i, const char* label, bool primary) {
        float x = px + (bw + gap) * (float)i;
        b = button_create(ctx->r2d,
                          (x + bw * 0.5f) / LW, (by + bh * 0.5f) / LH,
                          bw / LW, bh / LH);
        if (primary) {
            UIStyle::applyPrimaryButton(b);
            button_set_textx(b, font, label, (int)strlen(label), { 255, 255, 255, 255 });
        } else {
            UIStyle::applyLightButton(b);
            button_set_textx(b, font, label, (int)strlen(label), { 40, 44, 52, 255 });
        }
        button_set_text_offset(b, 0, 3.0f * s);
    };
    make(btn_create, 0, "创建房间", true);
    make(btn_join,   1, "加入选中", false);
    make(btn_refresh,2, "刷新",     false);
    make(btn_back,   3, "返回",     false);

    // ---- 直连行：直连 [IP] [端口] [直连] ----
    // inputbox 用左上角归一化坐标；button 用中心坐标。
    const float dy = LH * 0.79f;
    const float dh = 34.0f * s;
    const float ipx = px + LW * 0.09f;
    const float ipw = LW * 0.30f;
    const float ptx = ipx + ipw + gap;
    const float ptw = LW * 0.12f;
    const float btx = ptx + ptw + gap;
    const float btw = LW * 0.12f;

    ip_input = inputbox_create(ctx->r2d, ipx / LW, dy / LH, ipw / LW, dh / LH, font);
    inputbox_set_text(ip_input, ctx->settings.server_ip);

    port_input = inputbox_create(ctx->r2d, ptx / LW, dy / LH, ptw / LW, dh / LH, font);
    char pbuf[16];
    snprintf(pbuf, sizeof(pbuf), "%d", ctx->settings.server_port);
    inputbox_set_text(port_input, pbuf);

    btn_direct = button_create(ctx->r2d,
                               (btx + btw * 0.5f) / LW, (dy + dh * 0.5f) / LH,
                               btw / LW, dh / LH);
    UIStyle::applyLightButton(btn_direct);
    button_set_textx(btn_direct, font, "直连", (int)strlen("直连"), { 40, 44, 52, 255 });
    button_set_text_offset(btn_direct, 0, 3.0f * s);
}

void OnlineScene::Update(float dt) {
    (void)dt;
    LanDiscovery::Get().update();   // 收包 + 过期清理（非阻塞）

    const auto& rooms = LanDiscovery::Get().rooms();
    if (selected_ >= (int)rooms.size()) selected_ = (int)rooms.size() - 1;
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
        return;
    }
    snprintf(ctx->settings.server_ip, sizeof(ctx->settings.server_ip), "%s", ip);
    ctx->settings.server_port = port;
    settings_save(&ctx->settings);
    log_info("[lobby] direct connect %s:%d", ctx->settings.server_ip, ctx->settings.server_port);
    push_user_event(ONLINE_EVENT, ONLINE_EVENT_CLIENT);
}

void OnlineScene::HandleUiEvent(const void* e) {
    jevent_callbacks_t cbs = {};
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        OnlineScene* self = (OnlineScene*)arg;
        Context* ctx = self->ctx();

        // 输入框焦点：命中则聚焦，否则失焦
        bool hit_ip   = self->ip_input   && inputbox_is_hovered(self->ip_input, x, y);
        bool hit_port = self->port_input && inputbox_is_hovered(self->port_input, x, y);
        if (hit_ip)        inputbox_is_pressed(self->ip_input, x, y);
        else if (hit_port) inputbox_is_pressed(self->port_input, x, y);
        else {
            if (self->ip_input)   inputbox_defocus(self->ip_input);
            if (self->port_input) inputbox_defocus(self->port_input);
        }

        if (self->btn_create && ui_button_hit(self->btn_create, x, y))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_HOST);
        else if (self->btn_join && ui_button_hit(self->btn_join, x, y))
            self->join_selected();
        else if (self->btn_refresh && ui_button_hit(self->btn_refresh, x, y))
            LanDiscovery::Get().update();
        else if (self->btn_back && ui_button_hit(self->btn_back, x, y))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_BACK);
        else if (self->btn_direct && ui_button_hit(self->btn_direct, x, y))
            self->direct_connect();
        else {
            // 房间行点选：窗口像素 -> 逻辑坐标
            vec2_t logic = r2d_coordinates_from_window(ctx->r2d, x, y);
            self->layout();
            float inner_y = self->list_y + self->header_h;
            if (logic.x >= self->list_x && logic.x <= self->list_x + self->list_w &&
                logic.y >= inner_y && logic.y <= self->list_y + self->list_h) {
                int idx = (int)((logic.y - inner_y) / self->row_h);
                int max_rows = (int)((self->list_h - self->header_h) / self->row_h);
                const auto& rooms = LanDiscovery::Get().rooms();
                if (idx >= 0 && idx < max_rows && idx < (int)rooms.size())
                    self->selected_ = idx;
            }
        }
    };

    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)x; (void)y; (void)btn;
        OnlineScene* self = (OnlineScene*)arg;
        if (self->btn_create)  button_set_pressed(self->btn_create, false);
        if (self->btn_join)    button_set_pressed(self->btn_join, false);
        if (self->btn_refresh) button_set_pressed(self->btn_refresh, false);
        if (self->btn_back)    button_set_pressed(self->btn_back, false);
        if (self->btn_direct)  button_set_pressed(self->btn_direct, false);
    };

    cbs.on_process_event = [](const void* ev, void* arg) {
        OnlineScene* self = (OnlineScene*)arg;
        if (self->ip_input)   inputbox_process_event(self->ip_input, ev);
        if (self->port_input) inputbox_process_event(self->port_input, ev);
    };

    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        OnlineScene* self = (OnlineScene*)arg;
        if (self->btn_create)  button_is_hovered(self->btn_create, x, y);
        if (self->btn_join)    button_is_hovered(self->btn_join, x, y);
        if (self->btn_refresh) button_is_hovered(self->btn_refresh, x, y);
        if (self->btn_back)    button_is_hovered(self->btn_back, x, y);
        if (self->btn_direct)  button_is_hovered(self->btn_direct, x, y);
        if (self->ip_input)    inputbox_is_hovered(self->ip_input, x, y);
        if (self->port_input)  inputbox_is_hovered(self->port_input, x, y);
    };

    dispatch_event(cbs, e, this);
}

void OnlineScene::Render() {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font28 = AssetManager::GetInstance()->GetSimheiFont28();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);
    const float LW = (float)w, LH = (float)h, s = LH / 320.0f;

    // 背景（显式铺白，避免按钮/文字悬在透明底上）
    r2d_set_color(r2d, 255, 255, 255, 255);
    jrect_t full = { 0, 0, LW, LH };
    shape_draw_rectangle(r2d, "fill", full);

    // 标题
    const char* title = "联机大厅";
    jcolor_t tc = { 30, 32, 40, 255 };
    int tw = 0, th = 0;
    jtext_measurex(r2d, font28, title, (int)strlen(title), tc, &tw, &th);
    jtext_printf(r2d, font28, title, (int)strlen(title), tc,
                 (LW - (float)tw) * 0.5f / LW, 14.0f * s / LH);

    layout();
    const auto& rooms = LanDiscovery::Get().rooms();

    // 列表面板
    r2d_set_color(r2d, 205, 208, 216, 255);
    jrect_t frame = { list_x, list_y, list_w, list_h };
    shape_draw_rectangle(r2d, "line", frame);

    // 表头
    r2d_set_color(r2d, 240, 242, 246, 255);
    jrect_t hdr = { list_x + 1, list_y + 1, list_w - 2, header_h };
    shape_draw_rectangle(r2d, "fill", hdr);

    auto draw_col = [&](const char* t, float x, float cy, jcolor_t col) {
        int ww = 0, hh = 0;
        jtext_measurex(r2d, font24, t, (int)strlen(t), col, &ww, &hh);
        jtext_printf(r2d, font24, t, (int)strlen(t), col,
                     x / LW, (cy - (float)hh * 0.5f) / LH);
    };

    jcolor_t hc = { 90, 95, 105, 255 };
    float cy0 = list_y + header_h * 0.5f;
    draw_col("房间", list_x + list_w * 0.03f, cy0, hc);
    draw_col("主机", list_x + list_w * 0.42f, cy0, hc);
    draw_col("人数", list_x + list_w * 0.72f, cy0, hc);
    draw_col("地图", list_x + list_w * 0.87f, cy0, hc);

    if (rooms.empty()) {
        const char* empty = "未发现房间（同网段房主开房后会自动出现）";
        jcolor_t ec = { 130, 134, 142, 255 };
        int ew = 0, eh = 0;
        jtext_measurex(r2d, font24, empty, (int)strlen(empty), ec, &ew, &eh);
        jtext_printf(r2d, font24, empty, (int)strlen(empty), ec,
                     (list_x + (list_w - (float)ew) * 0.5f) / LW,
                     (list_y + header_h + (list_h - header_h - (float)eh) * 0.5f) / LH);
    } else {
        int max_rows = (int)((list_h - header_h) / row_h);
        if (max_rows < 1) max_rows = 1;
        for (int i = 0; i < (int)rooms.size() && i < max_rows; ++i) {
            float ry = list_y + header_h + row_h * (float)i;
            if (i == selected_) {
                r2d_set_color(r2d, 208, 226, 250, 255);
                jrect_t hi = { list_x + 1, ry, list_w - 2, row_h - 1 };
                shape_draw_rectangle(r2d, "fill", hi);
            }
            const RoomInfo& r = rooms[i];
            jcolor_t rc = { 40, 44, 52, 255 };
            float cyr = ry + row_h * 0.5f;
            char buf[128];

            draw_col(r.name.c_str(), list_x + list_w * 0.03f, cyr, rc);
            snprintf(buf, sizeof(buf), "%s:%d", r.ip.c_str(), r.port);
            draw_col(buf, list_x + list_w * 0.42f, cyr, rc);
            snprintf(buf, sizeof(buf), "%d/%d", r.players, r.max_players);
            draw_col(buf, list_x + list_w * 0.72f, cyr, rc);
            snprintf(buf, sizeof(buf), "地图%d", r.map_id);
            draw_col(buf, list_x + list_w * 0.87f, cyr, rc);
        }
    }

    // 直连标签
    {
        const float dy = LH * 0.79f + 34.0f * s * 0.5f;
        draw_col("直连:", LW * 0.05f, dy, hc);
    }

    if (btn_create)  button_draw(btn_create);
    if (btn_join)    button_draw(btn_join);
    if (btn_refresh) button_draw(btn_refresh);
    if (btn_back)    button_draw(btn_back);
    if (ip_input)    inputbox_draw(ip_input);
    if (port_input)  inputbox_draw(port_input);
    if (btn_direct)  button_draw(btn_direct);
}

void OnlineScene::Destroy() {
    LanDiscovery::Get().stop();
    if (btn_create)  button_destroy(btn_create);
    if (btn_join)    button_destroy(btn_join);
    if (btn_refresh) button_destroy(btn_refresh);
    if (btn_back)    button_destroy(btn_back);
    if (btn_direct)  button_destroy(btn_direct);
    if (ip_input)    inputbox_destroy(ip_input);
    if (port_input)  inputbox_destroy(port_input);
}
