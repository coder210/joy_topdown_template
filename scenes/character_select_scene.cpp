#include <cstring>
#include <cstdio>
#include "character_select_scene.h"
#include "level_select_scene.h"
#include "loading_scene.h"
#include "main_scene.h"
#include "online_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"
#include "../systems/character_catalog.h"
#include "../systems/weapon_catalog.h"
#include "../logical_presentation.h" // 选角页用自适应宽度（铺满、无左右黑边）

// Master-Detail 布局（设计稿 480x320；s = 逻辑宽/480）
static const float BAND_H       = 36.0f;    // 顶部墨色标题带
static const float LIST_X       = 12.0f;    // 左侧角色列表
static const float LIST_Y       = 44.0f;
static const float LIST_W       = 150.0f;
static const float LIST_H       = 264.0f;
static const float ROW_H        = 56.0f;
static const float ROW_SPACING  = 62.0f;
static const float PANEL_X      = 174.0f;   // 右侧详情面板
static const float PANEL_Y      = 44.0f;
static const float PANEL_W      = 294.0f;
static const float PANEL_H      = 264.0f;
static const float PREVIEW_X    = 182.0f;   // 大立绘框
static const float PREVIEW_Y    = 52.0f;
static const float PREVIEW_W    = 278.0f;
static const float PREVIEW_H    = 130.0f;

// 画角色站立图（idle 第 0 帧，横向等帧，帧宽 = 帧高 = 图高）。
// design_h 为期望的设计稿高度；纯展示，不做动画、不参与帧同步。
static void draw_card_avatar(Context* ctx, const CharacterDef& cd, float cx, float cy, float design_h)
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
    (void)iw;

    jrect_t src = { 0.0f, 0.0f, ih, ih };        // 第 0 帧
    // 立绘按“高度基准”缩放，与列表/面板一致；否则宽屏下会按宽度放大、溢出框
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(r2d, &lw, &lh, NULL);
    (void)lw;
    const float base = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    const float s = (design_h * base) / ih;
    vec2_t pos = { cx, cy };
    vec2_t scl = { s, s };
    vec2_t org = { ih * 0.5f, ih * 0.5f };
    image_draw_ex(img, &src, pos, 0.0f, scl, org);
}

// 角色定位：用武器类别 + 血量/移速推导，避免依赖不存在的描述字段
static const char* role_text(const CharacterDef& cd, const WeaponDef* wd)
{
    if (!wd) return "空手";
    if (wd->kind == 1)
        return (cd.max_hp >= 120) ? "近战 · 重装" : "近战 · 均衡";
    return (fp_to_float(cd.move_speed) >= 6.0f) ? "远程 · 游走" : "远程 · 输出";
}

// 按 scrollview 偏移刷新所有角色行按钮位置（绘制与命中都基于按钮位置）
void CharacterSelectScene::apply_card_scroll() {
    if (!card_view) return;
    const int n = (int)g_characterCatalog.size();
    const float oy = scrollview_get_offset_y(card_view);
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(this->ctx()->r2d, &lw, &lh, NULL);
    (void)lw;
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    for (int i = 0; i < n && i < MAX_CHARACTERS; i++)
        if (btn_cards[i])
            button_set_position(btn_cards[i], LIST_X * s,
                (LIST_Y + 4.0f + (float)i * ROW_SPACING) * s + oy);
}

// 把选中角色滚进左侧列表视口
void CharacterSelectScene::ensure_selected_visible() {
    if (!card_view) return;
    const int n = (int)g_characterCatalog.size();
    if (selected < 0 || selected >= n) return;
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(this->ctx()->r2d, &lw, &lh, NULL);
    (void)lw;
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    float off = scrollview_get_offset_y(card_view);
    const float view_top = LIST_Y * s;
    const float view_bot = (LIST_Y + LIST_H) * s;
    float row_top = (LIST_Y + 4.0f + (float)selected * ROW_SPACING) * s + off;
    float row_bot = row_top + ROW_H * s;
    if (row_top < view_top)      off += (view_top - row_top);
    else if (row_bot > view_bot) off -= (row_bot - view_bot);
    scrollview_set_offset(card_view, 0.0f, off);
    apply_card_scroll();
}

// 左侧列表中的一行：缩略头像 + 名字 + 主色点
void CharacterSelectScene::draw_card(int i) {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    button_p btn = btn_cards[i];
    if (!btn) return;

    const CharacterDef& cd = g_characterCatalog.get(i);
    vec2_t p = button_get_position(btn);
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(r2d, &lw, &lh, NULL);
    (void)lw;
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    float x = p.x, y = p.y, w = LIST_W * s, h = ROW_H * s;
    bool sel = (i == selected);
    bool hov = (hovered_ == i);

    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();

    if (sel) {
        r2d_set_color(r2d, 0, 0, 0, 40);
        shape_draw_rectangle(r2d, "fill", { x + 3.0f * s, y + 3.0f * s, w, h });
    }

    jcolor_t bc = sel ? jcolor_t{ 38, 38, 46, 255 }
                      : (hov ? jcolor_t{ 120, 120, 128, 255 } : jcolor_t{ 176, 172, 164, 255 });
    r2d_set_color(r2d, bc.r, bc.g, bc.b, 255);
    shape_draw_rectangle(r2d, "fill", { x, y, w, h });
    float bt = sel ? 2.5f * s : (hov ? 1.5f * s : 1.0f * s);
    r2d_set_color(r2d, 252, 250, 244, 255);
    shape_draw_rectangle(r2d, "fill", { x + bt, y + bt, w - 2 * bt, h - 2 * bt });

    if (sel) {
        r2d_set_color(r2d, 200, 60, 50, 255);
        shape_draw_rectangle(r2d, "fill", { x + bt, y + bt, 4.0f * s, h - 2 * bt });
    }

    // 缩略头像框
    float th = 34.0f * s;
    float tx = x + 10.0f * s;
    float ty = y + (h - th) * 0.5f;
    r2d_set_color(r2d, 236, 232, 224, 255);
    shape_draw_rectangle(r2d, "fill", { tx, ty, th, th });
    r2d_set_color(r2d, 150, 146, 138, 255);
    shape_draw_rectangle(r2d, "line", { tx, ty, th, th });
    draw_card_avatar(ctx, cd, tx + th * 0.5f, ty + th * 0.5f, 30.0f);

    // 名字（左对齐，天然规避中文居中按字节数算错的问题）
    jcolor_t name_c = sel ? jcolor_t{ 28, 28, 32, 255 } : jcolor_t{ 40, 40, 45, 255 };
    jtext_printx(r2d, font24, cd.name.c_str(), (int)cd.name.size(), name_c,
                 x + 54.0f * s, y + (h - 24.0f * s) * 0.5f);

    // 角色主色点
    int cr = (cd.color >> 16) & 0xFF;
    int cg = (cd.color >> 8) & 0xFF;
    int cb = cd.color & 0xFF;
    r2d_set_color(r2d, cr, cg, cb, 255);
    shape_draw_rectangle(r2d, "fill", { x + w - 14.0f * s, y + h * 0.5f - 3.0f * s, 6.0f * s, 6.0f * s });
}

// 右侧详情：大立绘 + 名字 + 血量/移速/武器/定位
void CharacterSelectScene::draw_preview_panel(Context* ctx, int index) {
    jr2d_p r2d = ctx->r2d;
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(r2d, &lw, &lh, NULL);
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;

    const int n = (int)g_characterCatalog.size();
    if (index < 0 || index >= n) return;
    const CharacterDef& cd = g_characterCatalog.get(index);

    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    // 面板按剩余宽度拉伸：设计稿(960x640)下与 PANEL_W 一致，宽屏时铺满右侧
    float px = PANEL_X * s, py = PANEL_Y * s;
    float pw = (float)lw - px - 12.0f * s;
    if (pw < PANEL_W * s) pw = PANEL_W * s;   // 极窄兜底
    float ph = PANEL_H * s;

    // 面板底 + 阴影 + 描边
    r2d_set_color(r2d, 0, 0, 0, 26);
    shape_draw_rectangle(r2d, "fill", { px + 3.0f * s, py + 3.0f * s, pw, ph });
    r2d_set_color(r2d, 250, 248, 242, 255);
    shape_draw_rectangle(r2d, "fill", { px, py, pw, ph });
    r2d_set_color(r2d, 150, 146, 138, 255);
    shape_draw_rectangle(r2d, "line", { px, py, pw, ph });

    // 大立绘框
    // 立绘框跟随面板宽度拉伸，左右各留 8*s
    float vx = PREVIEW_X * s, vy = PREVIEW_Y * s;
    float vw = px + pw - 8.0f * s - vx;
    if (vw < PREVIEW_W * s) vw = PREVIEW_W * s;
    float vh = PREVIEW_H * s;
    r2d_set_color(r2d, 236, 232, 224, 255);
    shape_draw_rectangle(r2d, "fill", { vx, vy, vw, vh });
    r2d_set_color(r2d, 190, 186, 178, 255);
    shape_draw_rectangle(r2d, "line", { vx, vy, vw, vh });

    int cr = (cd.color >> 16) & 0xFF;
    int cg = (cd.color >> 8) & 0xFF;
    int cb = cd.color & 0xFF;
    r2d_set_color(r2d, cr, cg, cb, 255);
    shape_draw_rectangle(r2d, "fill", { vx, vy, 4.0f * s, vh });

    draw_card_avatar(ctx, cd, vx + vw * 0.5f, vy + vh * 0.5f, 88.0f);

    // 名字
    jtext_printx(r2d, font24, cd.name.c_str(), (int)cd.name.size(), { 28, 28, 32, 255 },
                 (PANEL_X + 8.0f) * s, 190.0f * s);

    // 属性
    char buf[64];
    snprintf(buf, sizeof(buf), "血量 %d", cd.max_hp);
    jtext_printx(r2d, font12, buf, (int)strlen(buf), { 90, 90, 98, 255 },
                 (PANEL_X + 8.0f) * s, 222.0f * s);

    snprintf(buf, sizeof(buf), "移速 %.1f m/s", fp_to_float(cd.move_speed));
    jtext_printx(r2d, font12, buf, (int)strlen(buf), { 90, 90, 98, 255 },
                 (PANEL_X + 126.0f) * s, 222.0f * s);

    const WeaponDef* wd = cd.weapon.empty()
        ? nullptr : g_weaponCatalog.findByName(cd.weapon.c_str());
    snprintf(buf, sizeof(buf), "武器 %s", wd ? wd->name.c_str() : "空手");
    jtext_printx(r2d, font12, buf, (int)strlen(buf), { 90, 90, 98, 255 },
                 (PANEL_X + 8.0f) * s, 240.0f * s);

    snprintf(buf, sizeof(buf), "定位 %s", role_text(cd, wd));
    jtext_printx(r2d, font12, buf, (int)strlen(buf), { 110, 110, 118, 255 },
                 (PANEL_X + 8.0f) * s, 254.0f * s);
}

void CharacterSelectScene::go_to_level() {
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

    memset(self->btn_cards, 0, sizeof(self->btn_cards));

    const int n = (int)g_characterCatalog.size();
    const int nn = (n < MAX_CHARACTERS) ? n : MAX_CHARACTERS;

    // 选角页改用自适应逻辑宽度：高度固定 640，宽度跟随窗口宽高比，铺满无黑边。
    presentation_set_adaptive(ctx->r2d);

    int lw = 0, lh = 0;
    r2d_get_logical_presentation(ctx->r2d, &lw, &lh, NULL);
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    const float LW = (float)((lw > 0) ? lw : 960);
    const float LH = (float)((lh > 0) ? lh : 640);

    // 左侧角色列表：纵向滚动（定宽靠左）
    self->card_view = scrollview_create(ctx->r2d, 0.0f, 0.0f, 1.0f, 1.0f);
    if (self->card_view) {
        scrollview_set_view_px(self->card_view, LIST_X * s, LIST_Y * s, LIST_W * s, LIST_H * s);
        scrollview_set_content(self->card_view, LIST_W * s,
                (LIST_Y + 4.0f + (float)nn * ROW_SPACING + 8.0f) * s);
    }

    // 行按钮按像素换算成比例创建，自适应宽度下仍精确落在 LIST_X 起的位置
    for (int i = 0; i < nn; i++) {
        const float cx_px = (LIST_X + LIST_W * 0.5f) * s;
        const float cy_px = (LIST_Y + 4.0f + (float)i * ROW_SPACING + ROW_H * 0.5f) * s;
        self->btn_cards[i] = button_create(ctx->r2d,
                cx_px / LW, cy_px / LH,
                (LIST_W * s) / LW, (ROW_H * s) / LH);
        UIStyle::applyPaperButton(self->btn_cards[i]);
    }
    self->apply_card_scroll();

    self->selected = (ctx->selected_char_id >= 0 && ctx->selected_char_id < nn)
        ? ctx->selected_char_id : 0;
    self->ensure_selected_visible();

    // 昵称输入框：锚定右边缘（宽屏不漂移；设计稿下 x=660、宽 252）
    {
        const float ix_px = LW - 150.0f * s;
        self->nickname_input = inputbox_create(ctx->r2d,
                ix_px / LW, (8.0f * s) / LH,
                (126.0f * s) / LW, (24.0f * s) / LH, font24);
        inputbox_set_text(self->nickname_input, ctx->selected_nickname);
    }

    // 右下「开始游戏」：锚定右边缘（设计稿下 center=398）
    {
        const float cx_px = LW - 82.0f * s;
        const float cy_px = 286.0f * s;
        self->btn_confirm = button_create(ctx->r2d,
                cx_px / LW, cy_px / LH,
                (116.0f * s) / LW, (32.0f * s) / LH);
        UIStyle::applyPaperButton(self->btn_confirm);
        button_set_text_offset(self->btn_confirm, 0, 3.0f * s);
        button_set_textx(self->btn_confirm, font24, "开始游戏", (int)strlen("开始游戏"), { 0, 0, 0, 255 });
    }

    // 左下「返回」：锚定面板左边界（设计稿下 center=230）
    {
        const float cx_px = 230.0f * s;
        const float cy_px = 286.0f * s;
        self->btn_back = button_create(ctx->r2d,
                cx_px / LW, cy_px / LH,
                (96.0f * s) / LW, (32.0f * s) / LH);
        UIStyle::applyPaperButton(self->btn_back);
        button_set_text_offset(self->btn_back, 0, 3.0f * s);
        button_set_textx(self->btn_back, font24, "返回", (int)strlen("返回"), { 0, 0, 0, 255 });
    }
}

void CharacterSelectScene::HandleUiEvent(const void* e) {
    auto* self = this;
    // 先把事件交给滚动层：返回 true 说明本次手势是“滚动”，不再派发给下方控件
    if (self->card_view && scrollview_process_event(self->card_view, e)) return;

    jevent_callbacks_t cbs = {};
    cbs.on_process_event = [](const void* ev, void* arg) {
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        if (self->nickname_input) inputbox_process_event(self->nickname_input, ev);
    };
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        // 按下只记录行，不立即选中：滑动列表时 scrollview 会消费后续事件，
        // 松手若仍在这一行上才当作点击，避免“滑动误选角色”。
        self->press_row_ = -1;
        self->press_x_ = x;
        self->press_y_ = y;
        for (int i = 0; i < (int)g_characterCatalog.size() && i < MAX_CHARACTERS; i++) {
            if (self->btn_cards[i] && button_is_hovered(self->btn_cards[i], x, y)) {
                self->press_row_ = i;
                break;
            }
        }
        if (self->btn_confirm && ui_button_hit(self->btn_confirm, x, y)) {
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
            return;
        }
        if (self->btn_back && ui_button_hit(self->btn_back, x, y))
            self->go_back();
    };
    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)btn;
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        // 拖动列表时 scrollview 已把松手事件消费掉，这里不会执行；
        // 能走到这里说明是点击，且松手仍在按下的那一行上才选中。
        if (self->press_row_ >= 0) {
            button_p b = self->btn_cards[self->press_row_];
            if (b && button_is_hovered(b, x, y)) {
                UiSfx::Get().play(UiSfx::kClick);
                self->selected = self->press_row_;
            }
        }
        self->press_row_ = -1;
        for (int i = 0; i < (int)g_characterCatalog.size() && i < MAX_CHARACTERS; i++)
            if (self->btn_cards[i]) button_set_pressed(self->btn_cards[i], false);
        if (self->btn_confirm) button_set_pressed(self->btn_confirm, false);
        if (self->btn_back)    button_set_pressed(self->btn_back, false);
    };
    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        CharacterSelectScene* self = (CharacterSelectScene*)arg;
        self->hovered_ = -1;
        for (int i = 0; i < (int)g_characterCatalog.size() && i < MAX_CHARACTERS; i++)
            if (self->btn_cards[i] && button_is_hovered(self->btn_cards[i], x, y))
                self->hovered_ = i;
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

    const float s = (h > 0) ? ((float)h / 320.0f) : 1.0f;

    // 米色纸底
    r2d_set_color(r2d, 238, 234, 226, 255);
    shape_draw_rectangle(r2d, "fill", { 0, 0, (float)w, (float)h });

    // 顶部墨色标题带
    r2d_set_color(r2d, 40, 40, 48, 255);
    shape_draw_rectangle(r2d, "fill", { 0, 0, (float)w, BAND_H * s });
    jtext_printx(r2d, font24, "选择角色", (int)strlen("选择角色"),
                 { 245, 243, 237, 255 }, 16.0f * s, 6.0f * s);
    jtext_printx(r2d, font24, "昵称", (int)strlen("昵称"),
                 { 245, 243, 237, 255 }, 278.0f * s, 8.0f * s);

    if (self->nickname_input) inputbox_draw(self->nickname_input);

    // 左侧列表（裁剪 + 滚动条）
    self->apply_card_scroll();
    if (self->card_view) scrollview_begin(self->card_view);
    for (int i = 0; i < (int)g_characterCatalog.size() && i < MAX_CHARACTERS; i++)
        self->draw_card(i);
    if (self->card_view) scrollview_end(self->card_view);

    // 右侧详情
    self->draw_preview_panel(ctx, self->selected);

    if (self->btn_confirm) button_draw(self->btn_confirm);
    if (self->btn_back)    button_draw(self->btn_back);
}

void CharacterSelectScene::Destroy() {
    auto* self = this;
    for (int i = 0; i < MAX_CHARACTERS; i++)
        if (self->btn_cards[i]) { button_destroy(self->btn_cards[i]); self->btn_cards[i] = NULL; }
    if (self->card_view) { scrollview_destroy(self->card_view); self->card_view = NULL; }
    if (self->nickname_input) { inputbox_destroy(self->nickname_input); self->nickname_input = NULL; }
    if (self->btn_confirm) { button_destroy(self->btn_confirm); self->btn_confirm = NULL; }
    if (self->btn_back)    { button_destroy(self->btn_back);    self->btn_back = NULL; }
}
