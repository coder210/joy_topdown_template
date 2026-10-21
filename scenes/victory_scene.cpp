#include <cstring>
#include <cstdio>
#include "victory_scene.h"
#include "level_select_scene.h"
#include "main_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"

// 延迟切换：登记目标场景并推送 TRANSITION_EVENT，由 SceneManager 在本次事件分发结束后执行替换
static void request_scene(Context* ctx, Scene* s) {
    if (!s) return;
    ctx->scene_mgr->requestTransition(s);
}

VictoryScene::VictoryScene(Context* ctx) : Scene(ctx, "Victory") {}

void VictoryScene::Load() {
    auto* self = this;
    Context* ctx = self->ctx();
    log_info("[victory scene] start...");
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();

    int w = 0, h = 0;
    r2d_get_logical_presentation(ctx->r2d, &w, &h, NULL);
    const float s = (h > 0) ? ((float)h / 320.0f) : 1.0f;
    const float LW = (float)((w > 0) ? w : 960);
    const float LH = (float)((h > 0) ? h : 640);

    // 按钮尺寸用高度基准，宽屏下不被横向拉宽；设计稿 center：(150,275) / (330,275)
    self->btn_replay = button_create(ctx->r2d,
            (150.0f * s) / LW, (275.0f * s) / LH,
            (160.0f * s) / LW, (50.0f * s) / LH);
    UIStyle::applyPaperButton(self->btn_replay);
    button_set_text_offset(self->btn_replay, 0, 3.0f);
    button_set_textx(self->btn_replay, font24, "再战一局", (int)strlen("再战一局"), { 0, 0, 0, 255 });

    self->btn_menu = button_create(ctx->r2d,
            (330.0f * s) / LW, (275.0f * s) / LH,
            (160.0f * s) / LW, (50.0f * s) / LH);
    UIStyle::applyPaperButton(self->btn_menu);
    button_set_text_offset(self->btn_menu, 0, 3.0f);
    button_set_textx(self->btn_menu, font24, "回主菜单", (int)strlen("回主菜单"), { 0, 0, 0, 255 });
}

void VictoryScene::HandleUiEvent(const void* e) {
    auto* self = this;
    jevent_callbacks_t cbs = {};
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        VictoryScene* self = (VictoryScene*)arg;
        // 再战一局：回选关页（沿用当前模式）
        if (self->btn_replay && ui_button_hit(self->btn_replay, x, y)) {
            request_scene(self->ctx(),
                    new LevelSelectScene(self->ctx(), self->ctx()->mode));
            return; // 已登记切换，立即返回避免处理后续回调
        }
        // 回主菜单
        if (self->btn_menu && ui_button_hit(self->btn_menu, x, y))
            request_scene(self->ctx(),
                    new MainScene(self->ctx()));
    };
    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)x; (void)y; (void)btn;
        VictoryScene* self = (VictoryScene*)arg;
        if (self->btn_replay) button_set_pressed(self->btn_replay, false);
        if (self->btn_menu)   button_set_pressed(self->btn_menu, false);
    };
    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        VictoryScene* self = (VictoryScene*)arg;
        if (self->btn_replay) button_is_hovered(self->btn_replay, x, y);
        if (self->btn_menu)   button_is_hovered(self->btn_menu, x, y);
    };
    dispatch_event(cbs, e, self);
}

void VictoryScene::Update(float dt) {
    auto* self = this;
    self->elapsed += dt;
}

void VictoryScene::Render() {
    auto* self = this;
    Context* ctx = self->ctx();
    jr2d_p r2d = ctx->r2d;
    font_p font28 = AssetManager::GetInstance()->GetSimheiFont28();
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);

    // 白底蓝图风
    r2d_set_color(r2d, 255, 255, 255, 255);
    jrect_t full = { 0, 0, (float)w, (float)h };
    shape_draw_rectangle(r2d, "fill", full);

    // 标题（墨绿，水墨风），按实际文字宽度水平居中
    jcolor_t title_c = { 20, 120, 60, 255 };
    int tw = 0, th = 0;
    jtext_measurex(r2d, font28, "通  关", (int)strlen("通  关"), title_c, &tw, &th);
    jtext_printf(r2d, font28, "通  关", (int)strlen("通  关"), title_c,
                 ((float)w - (float)tw) * 0.5f / (float)w, 70.0f / 320.0f);

    char buf[128];
    jcolor_t sub_c = { 60, 60, 70, 255 };
    snprintf(buf, sizeof(buf), "第 %d 关的所有敌人已被肃清", ctx->selected_level);
    jtext_measurex(r2d, font24, buf, (int)strlen(buf), sub_c, &tw, &th);
    jtext_printf(r2d, font24, buf, (int)strlen(buf), sub_c,
                 ((float)w - (float)tw) * 0.5f / (float)w, 132.0f / 320.0f);
    jcolor_t tip_c = { 110, 110, 118, 255 };
    jtext_measurex(r2d, font24, "恭喜通关！", (int)strlen("恭喜通关！"), tip_c, &tw, &th);
    jtext_printf(r2d, font24, "恭喜通关！", (int)strlen("恭喜通关！"), tip_c,
                 ((float)w - (float)tw) * 0.5f / (float)w, 168.0f / 320.0f);

    if (self->btn_replay) button_draw(self->btn_replay);
    if (self->btn_menu)   button_draw(self->btn_menu);
}

void VictoryScene::Destroy() {
    auto* self = this;
    if (self->btn_replay) button_destroy(self->btn_replay);
    if (self->btn_menu)   button_destroy(self->btn_menu);
}




