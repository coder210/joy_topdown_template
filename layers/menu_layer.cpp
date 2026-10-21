#include <string.h>
#include <stdlib.h>
#include "menu_layer.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"

// 白底黑框按钮：无贴图，白色填充 + 黑色描边（button_draw 自带线框）+ 黑色文字
// 悬停和按下时平滑过渡到浅灰色
void MenuLayer::sendMenuEvent(int event_code)
{
        push_user_event(MENU_EVENT, event_code);
}

MenuLayer::MenuLayer(Context* ctx) : UILayer(ctx)
{
        load_menu_buttons();
}

MenuLayer::~MenuLayer() = default;

void MenuLayer::load_menu_buttons()
{
        int lw, lh;
        r2d_get_logical_presentation(ctx_->r2d, &lw, &lh, NULL);
        const float LW = (lw > 0) ? (float)lw : 960.0f;
        const float LH = (lh > 0) ? (float)lh : 640.0f;
        const float s  = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
        font_p simhei_font = AssetManager::GetInstance()->GetSimheiFont24();

        // 按钮尺寸/间距按设计稿 480x320 换算（高度基准），分辨率改变观感不变。
        const float btn_w = 120.0f * s, btn_h = 44.0f * s, gap = 12.0f * s;
        const float block = btn_h * 4 + gap * 3;
        const float start_y = (LH - block) * 0.5f + 20.0f * s;

        // 1 列栅格：四枚按钮垂直居中堆叠（与原来手动坐标结果一致）。
        // pad=0 时单元格顶边即行顶，按钮在单元格内顶部对齐 → 与原先逐行偏移等价。
        float cols[1] = { 1.0f };
        jlayout_grid_t* g = jlayout_grid_create((LW - btn_w) * 0.5f, start_y, btn_w, btn_h + gap, 1, cols, 0.0f);
        float cx, cy, cw, ch;

        auto make_btn = [&](const char* label, int len) -> button_p {
                jlayout_grid_cell(g, 0, &cx, &cy, &cw, &ch);
                button_p b = button_create(ctx_->r2d,
                        (cx + cw * 0.5f) / LW, (cy + btn_h * 0.5f) / LH,
                        cw / LW, btn_h / LH);
                UIStyle::applyPaperButton(b);
                button_set_text_offset(b, 0, 3.0f * s);
                button_set_textx(b, simhei_font, label, len, { 0, 0, 0, 255 });
                jlayout_grid_next_row(g);
                return b;
        };

        btn_start    = make_btn("单人游戏", (int)strlen("单人游戏"));
        btn_online   = make_btn("联机游戏", (int)strlen("联机游戏"));
        btn_settings = make_btn("设置",     (int)strlen("设置"));
        btn_exit     = make_btn("退出",     (int)strlen("退出"));
        jlayout_grid_destroy(g);
}

void MenuLayer::HandleEvent(const void* ev)
{
        jevent_callbacks_t cbs = {};
        cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
                (void)btn;
                auto* self = (MenuLayer*)arg;
                auto hit = [&](button_p b) { return ui_button_hit(b, x, y); };
                // 必须互斥：若一次触摸同时命中多个按钮，连发会造成"点哪个都退出"
                //（MENU_EVENT_EXIT 是最后一条，一旦也命中就退出程序）。
                if      (self->btn_start    && hit(self->btn_start))    MenuLayer::sendMenuEvent(MENU_EVENT_START);
                else if (self->btn_online   && hit(self->btn_online))   MenuLayer::sendMenuEvent(MENU_EVENT_ONLINE);
                else if (self->btn_settings && hit(self->btn_settings)) MenuLayer::sendMenuEvent(MENU_EVENT_SETTINGS);
                else if (self->btn_exit     && hit(self->btn_exit))     MenuLayer::sendMenuEvent(MENU_EVENT_EXIT);
        };
        cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
                (void)x; (void)y; (void)btn;
                auto* self = (MenuLayer*)arg;
                if (self->btn_start)   button_set_pressed(self->btn_start, false);
                if (self->btn_online)  button_set_pressed(self->btn_online, false);
                if (self->btn_settings) button_set_pressed(self->btn_settings, false);
                if (self->btn_exit)    button_set_pressed(self->btn_exit, false);
        };
        cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
                (void)dx; (void)dy;
                auto* self = (MenuLayer*)arg;
                if (self->btn_start)   button_is_hovered(self->btn_start, x, y);
                if (self->btn_online)  button_is_hovered(self->btn_online, x, y);
                if (self->btn_settings) button_is_hovered(self->btn_settings, x, y);
                if (self->btn_exit)    button_is_hovered(self->btn_exit, x, y);
        };
        dispatch_event(cbs, ev, this);
}

void MenuLayer::Destroy()
{
        if (btn_start)   { button_destroy(btn_start);   btn_start = NULL; }
        if (btn_online)  { button_destroy(btn_online);  btn_online = NULL; }
        if (btn_settings){ button_destroy(btn_settings);btn_settings = NULL; }
        if (btn_exit)    { button_destroy(btn_exit);    btn_exit = NULL; }
}

void MenuLayer::Render()
{
        if (!ctx_) return;
        jr2d_p r2d = ctx_->r2d;
        if (btn_start)   button_draw(btn_start);
        if (btn_online)  button_draw(btn_online);
        if (btn_settings) button_draw(btn_settings);
        if (btn_exit)    button_draw(btn_exit);
}
