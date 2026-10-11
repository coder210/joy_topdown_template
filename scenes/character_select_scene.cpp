#include <cstring>
#include "character_select_scene.h"
#include "level_select_scene.h"
#include "loading_scene.h"
#include "main_scene.h"
#include "online_scene.h"
#include "../ui_style.h"
#include "../asset_manager.h"
#include "../systems/character_catalog.h"

// 角色卡片尺寸（与 Load 中创建一致）
static const float CARD_W = 140.0f, CARD_H = 130.0f;

// 角色卡预览：画出角色站立图（取 idle 精灵表第 0 帧，横向等帧，帧宽 = 帧高 = 图高）。
// 纯展示用途，不做动画、不参与帧同步。
// 注：此处不画武器——选择界面只负责挑人，武器留给游戏内（含拾取）再呈现。
static void draw_card_avatar(Context* ctx, const CharacterDef& cd, float cx, float cy)
{
    jr2d_p r2d = ctx->r2d;
    AssetManager* am = AssetManager::GetInstance();
    if (!am) return;

    const std::string path = g_characterCatalog.animPath(cd, ANIM_IDLE);
    if (path.empty()) return;
    image_p img = am->GetImage(r2d, path.c_str());
    if (!img) return;

    float iw = 0.0f, ih = 0.0f;
    if (!image_get_size(img, &iw, &ih) || ih <= 0.0f) return;

    jrect_t src = { 0.0f, 0.0f, ih, ih };        // 第 0 帧
    const float s = 40.0f / ih;                   // 统一缩放到 40px 高
    vec2_t pos = { cx, cy };
    vec2_t scl = { s, s };
    vec2_t org = { ih * 0.5f, ih * 0.5f };
    image_draw_ex(img, &src, pos, 0.0f, scl, org);
}

void CharacterSelectScene::draw_card(int i) {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    const CharacterDef& cd = g_characterCatalog.get(i);
    button_p btn = btn_cards[i];
    if (!btn) return;
    vec2_t p = button_get_position(btn);
    float x = p.x, y = p.y, w = CARD_W, h = CARD_H;
    bool sel = (i == selected);
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    // 卡片背景：白底，选中用统一高亮色。
    if (sel) r2d_set_color(r2d, 214, 226, 240, 255);
    else     r2d_set_color(r2d, 248, 246, 240, 255);
    jrect_t bg = { x, y, w, h };
    shape_draw_rectangle(r2d, "fill", bg);

    // 边框：选中墨色粗边，未选中淡墨细边
    r2d_set_color(r2d, sel ? 30 : 120, sel ? 30 : 120, sel ? 35 : 128, 255);
    jrect_t b = { x, y, w, h };
    shape_draw_rectangle(r2d, "line", b);

    // 预览：角色站立图（画在文字之前，文字压在上层保证可读）
    draw_card_avatar(ctx, cd, x + 103.0f, y + 68.0f);

    // 角色名（墨色，选中白字）
    jcolor_t name_c = sel ? jcolor_t{ 255, 255, 255, 255 } : jcolor_t{ 40, 40, 45, 255 };
    int nl = (int)cd.name.size();
    int nw = nl * 12;
    jtext_printx(r2d, font24, cd.name.c_str(), nl, name_c, x + (w - nw) / 2.0f, y + 10.0f);

    // 属性（小号）：字段全部来自 characters.json
    jcolor_t attr_c = sel ? jcolor_t{ 245, 245, 245, 255 } : jcolor_t{ 90, 90, 98, 255 };
    char buf[64];
    snprintf(buf, sizeof(buf), "血量 %d", cd.max_hp);
    jtext_printx(r2d, font12, buf, (int)strlen(buf), attr_c, x + 12.0f, y + 44.0f);
    snprintf(buf, sizeof(buf), "移速 %.1f m/s", fp_to_float(cd.move_speed));
    jtext_printx(r2d, font12, buf, (int)strlen(buf), attr_c, x + 12.0f, y + 62.0f);
    snprintf(buf, sizeof(buf), "武器 %s", cd.starting_weapon.c_str());
    jtext_printx(r2d, font12, buf, (int)strlen(buf), attr_c, x + 12.0f, y + 80.0f);
    snprintf(buf, sizeof(buf), "%s", cd.key.c_str());
    jtext_printx(r2d, font12, buf, (int)strlen(buf), attr_c, x + 12.0f, y + 98.0f);
}

void CharacterSelectScene::go_to_level() {
    // 延迟切换：仅登记目标场景，由 main.cpp 在本次事件分发结束后执行替换，避免回调内释放 self
    ctx()->scene_mgr->requestTransition(new LevelSelectScene(ctx(), target_mode));
}

void CharacterSelectScene::go_to_loading() {
    ctx()->scene_mgr->requestTransition(new LoadingScene(ctx(), target_mode));
}

void CharacterSelectScene::go_back() {
    Scene* back;
    if (target_mode == MODE_SINGLE) back = new MainScene(ctx());
    else back = new OnlineScene(ctx());
    ctx()->scene_mgr->requestTransition(back);
}

CharacterSelectScene::CharacterSelectScene(Context* ctx, GameModeKind mode) : Scene(ctx, "CharSelect") {
    target_mode = mode;
}

void CharacterSelectScene::Load() {
    auto* self = this;
    Context* ctx = self->ctx();
    log_info("[character select scene] start...");
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();

    int w, h;
    r2d_get_logical_presentation(ctx->r2d, &w, &h, NULL);


    // 角色卡片（横向排列）：数量由 characters.json 决定
    memset(self->btn_cards, 0, sizeof(self->btn_cards));
    const int n = (int)g_characterCatalog.size();
    const float cw = CARD_W, ch = CARD_H, gap = 14.0f;
    const float cx0 = (w - (cw * n + gap * (n - 1))) / 2.0f;
    const float cy = 74.0f;
    for (int i = 0; i < n; i++) {
        self->btn_cards[i] = button_create(ctx->r2d,
                (cx0 + i * (cw + gap) + cw * 0.5f) / (float)w,
                (cy + ch * 0.5f) / (float)h,
                cw / (float)w, ch / (float)h);
        UIStyle::applyPaperButton(self->btn_cards[i]);
            }
    self->selected = (ctx->selected_char_id >= 0 && ctx->selected_char_id < n)
        ? ctx->selected_char_id : 0;

    // 昵称输入框（标题下方、卡片上方）
    self->nickname_input = inputbox_create(ctx->r2d, 0.11458f, 0.11875f, 0.41667f, 0.075f, font24);
    inputbox_set_text(self->nickname_input, ctx->selected_nickname);

    // 确认 / 返回
    self->btn_confirm = button_create(ctx->r2d,
            (180.0f + 60.0f) / (float)w, (244.0f + 20.0f) / (float)h,
            120.0f / (float)w, 40.0f / (float)h);
    UIStyle::applyPaperButton(self->btn_confirm);
    button_set_text_offset(self->btn_confirm, 0, 3.0f);
    button_set_textx(self->btn_confirm, font24, "开始游戏", (int)strlen("开始游戏"), { 0, 0, 0, 255 });
    
    self->btn_back = button_create(ctx->r2d,
            (15.0f + 40.0f) / (float)w, (244.0f + 20.0f) / (float)h,
            80.0f / (float)w, 40.0f / (float)h);
    UIStyle::applyPaperButton(self->btn_back);
    button_set_text_offset(self->btn_back, 0, 3.0f);
    button_set_textx(self->btn_back, font24, "返回", (int)strlen("返回"), { 0, 0, 0, 255 });
    }

void CharacterSelectScene::HandleUiEvent(const void* e) {
    auto* self = this;
    jevent_callbacks_t cbs = {};
    cbs.on_process_event = [](const void* ev, void* arg) {
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        if (self->nickname_input) inputbox_process_event(self->nickname_input, ev);
    };
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        for (int i = 0; i < (int)g_characterCatalog.size(); i++)
            if (self->btn_cards[i] && button_is_pressed(self->btn_cards[i], x, y))
                self->selected = i;
        if (self->btn_confirm && button_is_pressed(self->btn_confirm, x, y)) {
            const char* nick = self->nickname_input
                ? inputbox_get_text(self->nickname_input) : "";
            const char* s = nick;
            while (*s == ' ' || *s == '\t') s++;
            if (!*s) s = "玩家";
            snprintf(self->ctx()->selected_nickname,
                     sizeof(self->ctx()->selected_nickname), "%s", s);
            self->ctx()->selected_char_id = self->selected;
            // 客户端：关卡由房主统一下发，跳过选关页直接进 loading
            if (self->target_mode == MODE_CLIENT) self->go_to_loading();
            else self->go_to_level();
            return; // scene 已替换、self 已释放，必须立即返回
        }
        if (self->btn_back && button_is_pressed(self->btn_back, x, y))
            self->go_back();
    };
    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)x; (void)y; (void)btn;
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        for (int i = 0; i < (int)g_characterCatalog.size(); i++)
            if (self->btn_cards[i]) button_set_pressed(self->btn_cards[i], false);
        if (self->btn_confirm) button_set_pressed(self->btn_confirm, false);
        if (self->btn_back)    button_set_pressed(self->btn_back, false);
    };
    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        for (int i = 0; i < (int)g_characterCatalog.size(); i++)
            if (self->btn_cards[i]) button_is_hovered(self->btn_cards[i], x, y);
        if (self->btn_confirm) button_is_hovered(self->btn_confirm, x, y);
        if (self->btn_back)    button_is_hovered(self->btn_back, x, y);
    };
    dispatch_event(cbs, e, self);
}

void CharacterSelectScene::Render() {
    auto* self = this;
    Context* ctx = self->ctx();
    jr2d_p r2d = ctx->r2d;
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);

    // 白底蓝图风
    r2d_set_color(r2d, 255, 255, 255, 255);
    jrect_t full = { 0, 0, (float)w, (float)h };
    shape_draw_rectangle(r2d, "fill", full);

    // 标题（墨色）
    jcolor_t tc = { 60, 60, 70, 255 };
    jtext_printx(r2d, font24, "选择角色", (int)strlen("选择角色"), tc, 190, 10);

    // 昵称标签
    jcolor_t lc = { 50, 50, 60, 255 };
    jtext_printx(r2d, font24, "昵称", (int)strlen("昵称"), lc, 15, 44);
    if (self->nickname_input) inputbox_draw(self->nickname_input);

    for (int i = 0; i < (int)g_characterCatalog.size(); i++) draw_card(i);

    if (self->btn_confirm) button_draw(self->btn_confirm);
    if (self->btn_back)    button_draw(self->btn_back);
}

void CharacterSelectScene::Destroy() {
    auto* self = this;
    for (int i = 0; i < MAX_CHARACTERS; i++)
        if (self->btn_cards[i]) button_destroy(self->btn_cards[i]);
    if (self->nickname_input) inputbox_destroy(self->nickname_input);
    if (self->btn_confirm) button_destroy(self->btn_confirm);
    if (self->btn_back)    button_destroy(self->btn_back);
}




