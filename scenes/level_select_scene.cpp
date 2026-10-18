#include <cstring>
#include <cstdio>
#include "level_select_scene.h"
#include "character_select_scene.h"
#include "loading_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"
#include "../systems/character_catalog.h"
#include "../systems/map_def.h"   // level_map_available：判断该关 Tiled 地图是否已制作

// 关卡元信息 —— 条数须与 map_def.h 的 LEVEL_COUNT、resources.json 的 maps 段一致。
// 地图数据本体在 assets/.../levels/levelN.lua（Tiled 导出），缺图的关卡标记为"制作中"。
// 注：第 6~8 关目前是"占位测试关卡"——地图直接复制 level1.lua，用来验证列表纵向滚动，
//     正式版应在 Tiled 里各自作图后覆盖这三个 .lua。
struct LevelMeta { const char* name; const char* desc; };
static const LevelMeta LEVEL_META[] = {
    { "荒野迷城", "开阔原野，篱墙成列，练手入门" },
    { "寒石遗迹", "石垣交错，迂回周旋" },
    { "幽林深谷", "林墙环绕，步步为营" },
    { "龙脊荒冢", "机关密布，险中求胜" },
    { "幽冥龙穴", "巷战兼程，大显身手" },
    { "熔岩深渊", "测试占位关 · 地图同第 1 关" },
    { "霜语冰原", "测试占位关 · 地图同第 1 关" },
    { "虚空祭坛", "测试占位关 · 地图同第 1 关" },
};
static const int LEVEL_META_COUNT = (int)(sizeof(LEVEL_META) / sizeof(LEVEL_META[0]));

// 关卡按钮尺寸（与 Load 中创建一致）
static const float LEVEL_W = 300.0f, LEVEL_H = 44.0f;

// 延迟切换：登记目标场景并推送 TRANSITION_EVENT，由 SceneManager 在本次事件分发结束后执行替换
static void request_scene(Context* ctx, Scene* s) {
    if (!s) return;
    ctx->scene_mgr->requestTransition(s);
}

void LevelSelectScene::draw_level(int i) {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    button_p btn = btn_levels[i];
    if (!btn) return;
    vec2_t p = button_get_position(btn);
    // 卡片尺寸与文字偏移按设计稿 480x320 换算（LEVEL_W/H 是设计稿像素，直接用会偏小）
    float lwp = 0.0f;
    r2d_fpx(r2d, 1.0f, 0.0f, 0.0f, 0.0f, &lwp, NULL, NULL, NULL);
    const float s = lwp / 480.0f;
    float x = p.x, y = p.y, w = LEVEL_W * s, h = LEVEL_H * s;
    bool sel = (i == selected);
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    // 背景：选中墨色，未选中宣纸色；未制作的关卡（无 Tiled 地图）压暗成灰
    const bool ready = (i < LEVEL_SLOT_MAX) ? level_ready[i] : false;
    if (!ready)   r2d_set_color(r2d, 226, 226, 231, 255);
    else if (sel) r2d_set_color(r2d, 70, 70, 78, 255);
    else          r2d_set_color(r2d, 248, 246, 240, 255);
    jrect_t bg = { x, y, w, h };
    shape_draw_rectangle(r2d, "fill", bg);

    // 边框
    r2d_set_color(r2d, 96, 96, 104, 255);
    jrect_t b = { x, y, w, h };
    shape_draw_rectangle(r2d, "line", b);

    // 文本
    char buf[64];
    jcolor_t name_c = !ready ? jcolor_t{ 150, 150, 156, 255 }
                    : sel    ? jcolor_t{ 255, 255, 255, 255 }
                             : jcolor_t{ 40, 40, 45, 255 };
    snprintf(buf, sizeof(buf), "第 %d 关 · %s", i + 1, LEVEL_META[i].name);
    jtext_printx(r2d, font24, buf, (int)strlen(buf), name_c, x + 14.0f * s, y + 5.0f * s);
    jcolor_t desc_c = !ready ? jcolor_t{ 168, 168, 174, 255 }
                    : sel    ? jcolor_t{ 220, 220, 226, 255 }
                             : jcolor_t{ 110, 110, 118, 255 };
    jtext_printx(r2d, font12, LEVEL_META[i].desc, (int)strlen(LEVEL_META[i].desc), desc_c, x + 14.0f * s, y + 26.0f * s);

    // 未制作标记：朱砂红"制作中"贴在卡片右侧
    if (!ready) {
        const char* badge = "制作中";
        int bc = 0;
        for (const unsigned char* p = (const unsigned char*)badge; *p; ++p)
            if ((*p & 0xC0) != 0x80) bc++;
        jcolor_t badge_c = { 200, 60, 50, 255 };
        jtext_printx(r2d, font12, badge, (int)strlen(badge), badge_c,
                     x + w - 12.0f * s - bc * 12.0f * s, y + 6.0f * s);
    }
}

// 按 scrollview 的当前偏移刷新所有关卡按钮位置（绘制与命中都基于按钮位置，故一并生效）
void LevelSelectScene::apply_level_scroll() {
    if (!level_view) return;
    const float oy = scrollview_get_offset_y(level_view);
    // button_set_position 收逻辑像素：bx / level_base_y 是设计稿坐标，按同一系数换算
    float lwp = 0.0f;
    r2d_fpx(this->ctx()->r2d, 1.0f, 0.0f, 0.0f, 0.0f, &lwp, NULL, NULL, NULL);
    const float s = lwp / 480.0f;
    const float bx = 40.0f * s;   // 与 Load 中一致
    for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++)
        if (btn_levels[i])
            button_set_position(btn_levels[i], bx, level_base_y[i] * s + oy);
}

// 画"此关正在制作中"模态提示：半透明遮罩 + 居中的宣纸底提示框
void LevelSelectScene::draw_tip() {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);

    // 遮罩
    r2d_set_color(r2d, 20, 20, 26, 120);
    shape_draw_rectangle(r2d, "fill", jrect_t{ 0, 0, (float)w, (float)h });

    // 提示框（尺寸按设计稿 480x320 换算）
    float lwp = 0.0f;
    r2d_fpx(r2d, 1.0f, 0.0f, 0.0f, 0.0f, &lwp, NULL, NULL, NULL);
    const float s = lwp / 480.0f;
    const float bw = 230.0f * s, bh = 86.0f * s;
    const float bx = ((float)w - bw) * 0.5f;
    const float by = ((float)h - bh) * 0.5f;
    r2d_set_color(r2d, 248, 246, 240, 255);
    shape_draw_rectangle(r2d, "fill", jrect_t{ bx, by, bw, bh });
    r2d_set_color(r2d, 40, 40, 45, 255);
    shape_draw_rectangle(r2d, "line", jrect_t{ bx, by, bw, bh });

    // 标题（朱砂红）+ 说明（灰），按 UTF-8 字符数水平居中
    const char* title = "此关正在制作中";
    int tc_n = 0;
    for (const unsigned char* p = (const unsigned char*)title; *p; ++p)
        if ((*p & 0xC0) != 0x80) tc_n++;
    jcolor_t title_c = { 200, 60, 50, 255 };
    jtext_printx(r2d, font24, title, (int)strlen(title), title_c,
                 bx + (bw - tc_n * 24.0f * s) * 0.5f, by + 22.0f * s);

    const char* sub = "请改选其他关卡，或稍后再来";
    int sc_n = 0;
    for (const unsigned char* p = (const unsigned char*)sub; *p; ++p)
        if ((*p & 0xC0) != 0x80) sc_n++;
    jcolor_t sub_c = { 110, 110, 118, 255 };
    jtext_printx(r2d, font12, sub, (int)strlen(sub), sub_c,
                 bx + (bw - sc_n * 12.0f * s) * 0.5f, by + 56.0f * s);
}

LevelSelectScene::LevelSelectScene(Context* ctx, GameModeKind mode) : Scene(ctx, "LevelSelect") {
    target_mode = mode;
}

void LevelSelectScene::Load() {
    auto* self = this;
    Context* ctx = self->ctx();
    log_info("[level select scene] start...");
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();

    int w, h;
    r2d_get_logical_presentation(ctx->r2d, &w, &h, NULL);


    self->selected = (ctx->selected_level >= 1 && ctx->selected_level <= LEVEL_META_COUNT)
        ? ctx->selected_level - 1 : 0;

    // 探测各关地图是否已制作（Tiled 导出的 levelN.lua 是否存在）。
    // 缺图的关卡在列表里标灰"制作中"，点开始也不进游戏，只弹提示。
    self->tip_active = false;
    for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++)
        self->level_ready[i] = level_map_available(i + 1);
    // 默认选中若未制作，则改选第一个已制作的关卡（没有就停留在第 1 关，由提示兜住）
    if (self->selected >= 0 && self->selected < LEVEL_META_COUNT
        && !self->level_ready[self->selected]) {
        for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++) {
            if (self->level_ready[i]) { self->selected = i; break; }
        }
    }

    // 关卡按钮：左侧竖列，确认/返回移右侧竖排，避免纵向重叠。
    // 列表交给 scrollview 做纵向滚动（关卡多到超出视口时可上下拖动；当前 5 关放得下则不滚动）。
    // scrollview 的 *_px 接口收逻辑像素，按设计稿 480x320 换算
    float lwp = 0.0f;
    r2d_fpx(ctx->r2d, 1.0f, 0.0f, 0.0f, 0.0f, &lwp, NULL, NULL, NULL);
    const float s = lwp / 480.0f;
    const float bx = 40.0f * s;
    self->level_view = scrollview_create(ctx->r2d, 0.0f, 0.0f, 1.0f, 1.0f);  // 先占位，下面改像素视口
    if (self->level_view) {
        // 视口：标题/角色提示之下到屏幕底部；确认/返回在右侧竖排，不占视口宽度
        scrollview_set_view_px(self->level_view, bx, 68.0f * s, LEVEL_W * s,
                (320.0f - 68.0f - 8.0f) * s);
        scrollview_set_content(self->level_view, LEVEL_W * s,
                (LEVEL_META_COUNT * 48.0f - (48.0f - LEVEL_H)) * s);
    }
    for (int i = 0; i < LEVEL_META_COUNT; i++) {
        self->level_base_y[i] = 74.0f + (float)i * 48.0f;
        self->btn_levels[i] = button_create(ctx->r2d,
                (bx + LEVEL_W * 0.5f) / 480.0f,
                (self->level_base_y[i] + LEVEL_H * 0.5f) / 320.0f,
                LEVEL_W / 480.0f, LEVEL_H / 320.0f);
        UIStyle::applyPaperButton(self->btn_levels[i]);
            }
    self->apply_level_scroll();

    self->btn_confirm = button_create(ctx->r2d,
            (360.0f + 60.0f) / 480.0f, (156.0f + 22.5f) / 320.0f,
            120.0f / 480.0f, 45.0f / 320.0f);
    UIStyle::applyPaperButton(self->btn_confirm);
    button_set_text_offset(self->btn_confirm, 0, 3.0f);
    button_set_textx(self->btn_confirm, font24, "开始", (int)strlen("开始"), { 0, 0, 0, 255 });
    
    self->btn_back = button_create(ctx->r2d,
            (360.0f + 45.0f) / 480.0f, (212.0f + 22.5f) / 320.0f,
            90.0f / 480.0f, 45.0f / 320.0f);
    UIStyle::applyPaperButton(self->btn_back);
    button_set_text_offset(self->btn_back, 0, 3.0f);
    button_set_textx(self->btn_back, font24, "返回", (int)strlen("返回"), { 0, 0, 0, 255 });
    }

void LevelSelectScene::HandleUiEvent(const void* e) {
    auto* self = this;

    // "此关正在制作中"提示为模态：显示期间消费所有事件，点击任意处关闭，
    // 避免提示后面的关卡按钮/开始按钮被误触。
    if (self->tip_active) {
        jevent_callbacks_t tcbs = {};
        tcbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
            (void)x; (void)y; (void)btn;
            ((LevelSelectScene*)arg)->tip_active = false;
        };
        dispatch_event(tcbs, e, self);
        return;
    }

    // 再把事件交给列表滚动层：返回 true 说明本次手势是"滚动"（拖拽中/滑动刚结束/滚轮），
    // 此时不派发给关卡按钮，避免"滑动一下"误选关卡/误点开始。
    if (self->level_view && scrollview_process_event(self->level_view, e)) return;

    jevent_callbacks_t cbs = {};
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        LevelSelectScene* self = (LevelSelectScene*)arg;
        for (int i = 0; i < LEVEL_META_COUNT; i++)
            if (self->btn_levels[i] && ui_button_hit(self->btn_levels[i], x, y))
                self->selected = i;
        if (self->btn_confirm && ui_button_hit(self->btn_confirm, x, y)) {
            // 该关地图还没做（缺 Tiled .lua）：弹"制作中"提示，不进入游戏
            if (self->selected < 0 || self->selected >= 6 || !self->level_ready[self->selected]) {
                self->tip_active = true;
                return;
            }
            self->ctx()->selected_level = self->selected + 1;
            request_scene(self->ctx(),
                new LoadingScene(self->ctx(), self->target_mode));
            return; // 已登记切换，立即返回避免处理后续回调
        }
        if (self->btn_back && ui_button_hit(self->btn_back, x, y))
            request_scene(self->ctx(),
                new CharacterSelectScene(self->ctx(), self->target_mode));
    };
    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)x; (void)y; (void)btn;
        LevelSelectScene* self = (LevelSelectScene*)arg;
        for (int i = 0; i < LEVEL_META_COUNT; i++)
            if (self->btn_levels[i]) button_set_pressed(self->btn_levels[i], false);
        if (self->btn_confirm) button_set_pressed(self->btn_confirm, false);
        if (self->btn_back)    button_set_pressed(self->btn_back, false);
    };
    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        LevelSelectScene* self = (LevelSelectScene*)arg;
        for (int i = 0; i < LEVEL_META_COUNT; i++)
            if (self->btn_levels[i]) button_is_hovered(self->btn_levels[i], x, y);
        if (self->btn_confirm) button_is_hovered(self->btn_confirm, x, y);
        if (self->btn_back)    button_is_hovered(self->btn_back, x, y);
    };
    dispatch_event(cbs, e, self);
}

void LevelSelectScene::Render() {
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

    // 标题
    jcolor_t tc = { 60, 60, 70, 255 };
    // 坐标比例化（190/20 为设计稿 480x320 下的值）
    jtext_printf(r2d, font24, "选择关卡", (int)strlen("选择关卡"), tc,
                 190.0f / 480.0f, 20.0f / 320.0f);

    // 当前角色提示
    char chBuf[64];
    snprintf(chBuf, sizeof(chBuf), "当前角色：%s",
             g_characterCatalog.get(self->ctx()->selected_char_id).name.c_str());
    jcolor_t lc = { 110, 110, 118, 255 };
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();
    jtext_printf(r2d, font12, chBuf, (int)strlen(chBuf), lc,
                 180.0f / 480.0f, 52.0f / 320.0f);

    // 关卡列表：交给 scrollview 裁剪 + 画滚动条；位置按当前偏移刷新后绘制
    self->apply_level_scroll();
    if (self->level_view) scrollview_begin(self->level_view);
    for (int i = 0; i < LEVEL_META_COUNT; i++) draw_level(i);
    if (self->level_view) scrollview_end(self->level_view);

    if (self->btn_confirm) button_draw(self->btn_confirm);
    if (self->btn_back)    button_draw(self->btn_back);

    // "此关正在制作中"提示（最上层，模态）
    if (self->tip_active) self->draw_tip();
}

void LevelSelectScene::Destroy() {
    auto* self = this;
    for (int i = 0; i < LEVEL_META_COUNT; i++)
        if (self->btn_levels[i]) button_destroy(self->btn_levels[i]);
    if (self->level_view) scrollview_destroy(self->level_view);
    self->level_view = NULL;
    if (self->btn_confirm) button_destroy(self->btn_confirm);
    if (self->btn_back)    button_destroy(self->btn_back);
}




