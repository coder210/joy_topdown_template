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
        int logical_width, logical_height;
        r2d_get_logical_presentation(ctx_->r2d, &logical_width, &logical_height, NULL);
        font_p simhei_font = AssetManager::GetInstance()->GetSimheiFont24();

        // 按钮尺寸/间距按设计稿 480x320 换算：逻辑分辨率改变时观感不变。
        //（原来是直接用 120 / 44 / 12 / 20 这些设计稿像素，分辨率一改按钮就会变小并偏移）
        const float sx = (float)logical_width  / 480.0f;
        const float sy = (float)logical_height / 320.0f;
        float btn_width = 120.0f * sx, btn_height = 44.0f * sy;
        float btn_x = ((float)logical_width - btn_width) * 0.5f;
        float gap = 12.0f * sy;
        // 整体下沉：空出顶部标题区域、底部给版本号留白，使四枚按钮垂直均衡。
        float block = btn_height * 4 + gap * 3;
        float start_y = ((float)logical_height - block) * 0.5f + 20.0f * sy;

        btn_start = button_create(ctx_->r2d,
                ((float)btn_x + (float)btn_width * 0.5f) / (float)logical_width,
                ((float)start_y + (float)btn_height * 0.5f) / (float)logical_height,
                (float)btn_width / (float)logical_width, (float)btn_height / (float)logical_height);
        UIStyle::applyPaperButton(btn_start);
        button_set_text_offset(btn_start, 0, 3.0f * sy);
        button_set_textx(btn_start, simhei_font, "单人游戏", (int)strlen("单人游戏"), { 0, 0, 0, 255 });
        
        btn_online = button_create(ctx_->r2d,
                ((float)btn_x + (float)btn_width * 0.5f) / (float)logical_width,
                ((float)(start_y + (btn_height + gap)) + (float)btn_height * 0.5f) / (float)logical_height,
                (float)btn_width / (float)logical_width, (float)btn_height / (float)logical_height);
        UIStyle::applyPaperButton(btn_online);
        button_set_text_offset(btn_online, 0, 3.0f * sy);
        button_set_textx(btn_online, simhei_font, "联机游戏", (int)strlen("联机游戏"), { 0, 0, 0, 255 });
        
        btn_settings = button_create(ctx_->r2d,
                ((float)btn_x + (float)btn_width * 0.5f) / (float)logical_width,
                ((float)(start_y + (btn_height + gap) * 2) + (float)btn_height * 0.5f) / (float)logical_height,
                (float)btn_width / (float)logical_width, (float)btn_height / (float)logical_height);
        UIStyle::applyPaperButton(btn_settings);
        button_set_text_offset(btn_settings, 0, 3.0f * sy);
        button_set_textx(btn_settings, simhei_font, "设置", (int)strlen("设置"), { 0, 0, 0, 255 });
        
        btn_exit = button_create(ctx_->r2d,
                ((float)btn_x + (float)btn_width * 0.5f) / (float)logical_width,
                ((float)(start_y + (btn_height + gap) * 3) + (float)btn_height * 0.5f) / (float)logical_height,
                (float)btn_width / (float)logical_width, (float)btn_height / (float)logical_height);
        UIStyle::applyPaperButton(btn_exit);
        button_set_text_offset(btn_exit, 0, 3.0f * sy);
        button_set_textx(btn_exit, simhei_font, "退出", (int)strlen("退出"), { 0, 0, 0, 255 });
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
