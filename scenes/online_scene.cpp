#include "online_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"

OnlineScene::OnlineScene(Context* ctx) : Scene(ctx, "Online") {}

void OnlineScene::Load() {
    auto* self = this;
    Context* ctx = self->ctx();
    log_info("[online scene] start...");

    int w, h;
    r2d_get_logical_presentation(ctx->r2d, &w, &h, NULL);
    font_p font = AssetManager::GetInstance()->GetSimheiFont24();

    // 按钮尺寸/间距按设计稿 480x320 换算：原来是直接用 120/50/20 这些设计稿像素，
    // 再除以"当前" w/h —— 逻辑分辨率提高后分数被稀释，按钮会变小且偏移。
    const float sx = (float)w / 480.0f, sy = (float)h / 320.0f;
    float btn_w = 120.0f * sx, btn_h = 50.0f * sy, gap = 20.0f * sy;
    float btn_x = ((float)w - btn_w) * 0.5f;
    float start_y = ((float)h - btn_h * 4.0f - gap * 3.0f) * 0.5f;

    // 四个按钮：直接创建并设置位置（不再依赖 joy 的 scene_node 树；渲染在 Render() 中手动完成）
    self->btn_host = button_create(ctx->r2d,
                ((float)btn_x + (float)btn_w * 0.5f) / (float)w,
                ((float)start_y + (float)btn_h * 0.5f) / (float)h,
                (float)btn_w / (float)w, (float)btn_h / (float)h);
    UIStyle::applyOnlineButton(self->btn_host);
    button_set_text_offset(self->btn_host, 0, 3.0f * sy);
    button_set_textx(self->btn_host, font, "房主", (int)strlen("房主"), { 0, 0, 0, 255 });
    
    self->btn_client = button_create(ctx->r2d,
                ((float)btn_x + (float)btn_w * 0.5f) / (float)w,
                ((float)(start_y + (btn_h + gap)) + (float)btn_h * 0.5f) / (float)h,
                (float)btn_w / (float)w, (float)btn_h / (float)h);
    UIStyle::applyOnlineButton(self->btn_client);
    button_set_text_offset(self->btn_client, 0, 3.0f * sy);
    button_set_textx(self->btn_client, font, "客户端", (int)strlen("客户端"), { 0, 0, 0, 255 });
    
    self->btn_server = button_create(ctx->r2d,
                ((float)btn_x + (float)btn_w * 0.5f) / (float)w,
                ((float)(start_y + (btn_h + gap) * 2) + (float)btn_h * 0.5f) / (float)h,
                (float)btn_w / (float)w, (float)btn_h / (float)h);
    UIStyle::applyOnlineButton(self->btn_server);
    button_set_text_offset(self->btn_server, 0, 3.0f * sy);
    button_set_textx(self->btn_server, font, "服务端", (int)strlen("服务端"), { 0, 0, 0, 255 });
    
    self->btn_back = button_create(ctx->r2d,
                ((float)btn_x + (float)btn_w * 0.5f) / (float)w,
                ((float)(start_y + (btn_h + gap) * 3) + (float)btn_h * 0.5f) / (float)h,
                (float)btn_w / (float)w, (float)btn_h / (float)h);
    UIStyle::applyOnlineButton(self->btn_back);
    button_set_text_offset(self->btn_back, 0, 3.0f * sy);
    button_set_textx(self->btn_back, font, "返回", (int)strlen("返回"), { 0, 0, 0, 255 });
    }

void OnlineScene::HandleUiEvent(const void* e) {
    auto* self = this;
    jevent_callbacks_t cbs = {};
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        OnlineScene* self = (OnlineScene*)arg;
        auto hit = [&](button_p b) { return ui_button_hit(b, x, y); };
        if (self->btn_host && hit(self->btn_host))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_HOST);
        if (self->btn_client && hit(self->btn_client))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_CLIENT);
        if (self->btn_server && hit(self->btn_server))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_SERVER);
        if (self->btn_back && hit(self->btn_back))
            push_user_event(ONLINE_EVENT, ONLINE_EVENT_BACK);
    };
    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)x; (void)y; (void)btn;
        OnlineScene* self = (OnlineScene*)arg;
        if (self->btn_host)    button_set_pressed(self->btn_host, false);
        if (self->btn_client)  button_set_pressed(self->btn_client, false);
        if (self->btn_server)  button_set_pressed(self->btn_server, false);
        if (self->btn_back)    button_set_pressed(self->btn_back, false);
    };
    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        OnlineScene* self = (OnlineScene*)arg;
        if (self->btn_host)    button_is_hovered(self->btn_host, x, y);
        if (self->btn_client)  button_is_hovered(self->btn_client, x, y);
        if (self->btn_server)  button_is_hovered(self->btn_server, x, y);
        if (self->btn_back)    button_is_hovered(self->btn_back, x, y);
    };
    dispatch_event(cbs, e, self);
}

void OnlineScene::Render() {
    auto* self = this;
    Context* ctx = self->ctx();
    jr2d_p r2d = ctx->r2d;
    font_p font = AssetManager::GetInstance()->GetSimheiFont24();
    const char* text = "选择联机模式";
    int len = (int)strlen(text);
    int w_est = len * 14;
    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);
    jcolor_t c = { 200, 200, 210, 255 };
    // 水平居中（动态），垂直位置比例化：30/320
    jtext_printf(r2d, font, text, len, c,
                 ((float)w - (float)w_est) * 0.5f / (float)w, 30.0f / 320.0f);
    if (self->btn_host)   button_draw(self->btn_host);
    if (self->btn_client) button_draw(self->btn_client);
    if (self->btn_server) button_draw(self->btn_server);
    if (self->btn_back)   button_draw(self->btn_back);
}

void OnlineScene::Destroy() {
    auto* self = this;
    // 原 C 版本漏销毁按钮，这里补上以消除泄漏
    if (self->btn_host)   button_destroy(self->btn_host);
    if (self->btn_client) button_destroy(self->btn_client);
    if (self->btn_server) button_destroy(self->btn_server);
    if (self->btn_back)   button_destroy(self->btn_back);
}




