#include <cstring>
#include <cstdio>
#include "level_select_scene.h"
#include "character_select_scene.h"
#include "loading_scene.h"
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"
#include "../systems/character_catalog.h"
#include "../systems/map_def.h"   // level_map_available / map_load_level / g_map_grid
#include "../systems/spawn_def.h" // level_enemy_total（本关敌人总数配额）
#include "../logical_presentation.h" // 选关页用自适应宽度（铺满、无左右黑边）

// 关卡元信息 —— 条数须与 map_def.h 的 LEVEL_COUNT、resources.json 的 maps 段一致。
// 地图数据本体在 assets/.../levels/levelN.lua（Tiled 导出），缺图的关卡标记为"制作中"。
// 注：第 6~8 关目前是"占位测试关卡"——地图直接复制 level1.lua，正式版应在 Tiled 各自作图。
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

// ---- Master-Detail 布局（设计稿 480x320；s = 逻辑宽/480） ----
static const float BAND_H       = 36.0f;    // 顶部标题带
static const float LIST_X       = 12.0f;    // 左侧列表
static const float LIST_Y       = 44.0f;
static const float LIST_W       = 200.0f;
static const float LIST_H       = 264.0f;
static const float ROW_H        = 50.0f;
static const float ROW_SPACING  = 54.0f;
static const float PANEL_X      = 224.0f;   // 右侧面板
static const float PANEL_Y      = 44.0f;
static const float PANEL_W      = 244.0f;
static const float PANEL_H      = 264.0f;
static const float PREVIEW_X    = 232.0f;   // 预览框（面板内）
static const float PREVIEW_Y    = 52.0f;
static const float PREVIEW_W    = 228.0f;
static const float PREVIEW_H    = 140.0f;

// 延迟切换：登记目标场景并推送 TRANSITION_EVENT，由 SceneManager 在本次事件分发结束后执行替换
static void request_scene(Context* ctx, Scene* s) {
    if (!s) return;
    ctx->scene_mgr->requestTransition(s);
}

// 用该关真实玩法网格画迷你地图（墙=墨块，出生点=朱砂块）。零美术资源。
void LevelSelectScene::draw_mini_map(Context* ctx, int i, float mx, float my,
                                     float mw, float mh, float s, bool ready) {
    jr2d_p r2d = ctx->r2d;
    (void)s;

    r2d_set_color(r2d, 232, 230, 224, 255);
    shape_draw_rectangle(r2d, "fill", { mx, my, mw, mh });

    if (!ready || !preview_has_[i] || preview_w_[i] <= 0 || preview_h_[i] <= 0) {
        font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
        char q = '?';
        int tw = 0, th = 0;
        if (font24 && jtext_measurex(r2d, font24, &q, 1, { 150, 150, 156, 255 }, &tw, &th))
            jtext_printx(r2d, font24, &q, 1, { 150, 150, 156, 255 },
                         mx + mw * 0.5f - (float)tw * 0.5f, my + mh * 0.5f - (float)th * 0.5f);
        r2d_set_color(r2d, 190, 188, 182, 255);
        shape_draw_rectangle(r2d, "line", { mx, my, mw, mh });
        return;
    }

    int W = preview_w_[i], H = preview_h_[i];
    float cell = mh / (float)H;
    if (cell * (float)W > mw) cell = mw / (float)W;
    float ox = mx + (mw - cell * (float)W) * 0.5f;
    float oy = my + (mh - cell * (float)H) * 0.5f;

    for (int yy = 0; yy < H; yy++) {
        for (int xx = 0; xx < W; xx++) {
            if (preview_grid_[i][yy * W + xx] == '#') {
                r2d_set_color(r2d, 60, 60, 68, 255);
                shape_draw_rectangle(r2d, "fill",
                    { ox + (float)xx * cell, oy + (float)yy * cell, cell + 0.5f, cell + 0.5f });
            }
        }
    }
    int pc = preview_player_[i];
    if (pc >= 0 && pc < W * H) {
        int px = pc % W, py = pc / W;
        float sz = cell * 0.6f;
        r2d_set_color(r2d, 200, 60, 50, 255);
        shape_draw_rectangle(r2d, "fill",
            { ox + (float)px * cell + (cell - sz) * 0.5f,
              oy + (float)py * cell + (cell - sz) * 0.5f, sz, sz });
    }
    r2d_set_color(r2d, 150, 148, 142, 255);
    shape_draw_rectangle(r2d, "line", { mx, my, mw, mh });
}

// 难度：5 格方块，从 start_x 起向右排，前 diff 个填充金色
void LevelSelectScene::draw_difficulty(Context* ctx, int i, float start_x, float y, float s) {
    jr2d_p r2d = ctx->r2d;
    int diff = 1 + i / 2; if (diff > 5) diff = 5;
    const int n = 5;
    float pip = 9.0f * s, gap = 4.0f * s;
    for (int k = 0; k < n; k++) {
        float px = start_x + (float)k * (pip + gap);
        if (k < diff) r2d_set_color(r2d, 210, 150, 60, 255);
        else          r2d_set_color(r2d, 200, 200, 206, 255);
        shape_draw_rectangle(r2d, "fill", { px, y, pip, pip });
    }
}

// 左侧列表中的一行：序号 + 名称 + 状态
void LevelSelectScene::draw_level(int i) {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    button_p btn = btn_levels[i];
    if (!btn) return;
    vec2_t p = button_get_position(btn);
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(r2d, &lw, &lh, NULL);
    (void)lw;
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    float x = p.x, y = p.y, w = LIST_W * s, h = ROW_H * s;
    bool sel = (i == selected);
    bool hov = (hovered_ == i);
    const bool ready = (i < LEVEL_SLOT_MAX) ? level_ready[i] : false;

    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    if (sel) {
        r2d_set_color(r2d, 0, 0, 0, 40);
        shape_draw_rectangle(r2d, "fill", { x + 3.0f * s, y + 3.0f * s, w, h });
    }

    // 边框 + 底
    jcolor_t bc = sel ? jcolor_t{ 38, 38, 46, 255 }
                      : (hov ? jcolor_t{ 120, 120, 128, 255 } : jcolor_t{ 176, 172, 164, 255 });
    r2d_set_color(r2d, bc.r, bc.g, bc.b, 255);
    shape_draw_rectangle(r2d, "fill", { x, y, w, h });
    jcolor_t card = ready ? jcolor_t{ 252, 250, 244, 255 } : jcolor_t{ 214, 212, 206, 255 };
    float bt = sel ? 2.5f * s : (hov ? 1.5f * s : 1.0f * s);
    r2d_set_color(r2d, card.r, card.g, card.b, 255);
    shape_draw_rectangle(r2d, "fill", { x + bt, y + bt, w - 2 * bt, h - 2 * bt });

    // 选中朱砂左边条
    if (sel) {
        r2d_set_color(r2d, 200, 60, 50, 255);
        shape_draw_rectangle(r2d, "fill", { x + bt, y + bt, 4.0f * s, h - 2 * bt });
    }

    // 序号
    char num[8]; snprintf(num, sizeof(num), "%02d", i + 1);
    jcolor_t numc = sel ? jcolor_t{ 200, 60, 50, 255 } : jcolor_t{ 130, 130, 138, 255 };
    jtext_printx(r2d, font12, num, (int)strlen(num), numc, x + 12.0f * s, y + 16.0f * s);

    // 名称
    jcolor_t namec = !ready ? jcolor_t{ 150, 150, 156, 255 }
                            : (sel ? jcolor_t{ 28, 28, 32, 255 } : jcolor_t{ 40, 40, 45, 255 });
    jtext_printx(r2d, font24, LEVEL_META[i].name, (int)strlen(LEVEL_META[i].name),
                 namec, x + 40.0f * s, y + 8.0f * s);

    // 右侧状态
    if (!ready) {
        jtext_printx(r2d, font12, "制作中", (int)strlen("制作中"), { 200, 60, 50, 255 },
                     x + w - 46.0f * s, y + 17.0f * s);
    } else {
        r2d_set_color(r2d, 120, 150, 110, 255);
        shape_draw_rectangle(r2d, "fill",
            { x + w - 15.0f * s, y + h * 0.5f - 2.5f * s, 5.0f * s, 5.0f * s });
    }
}

// 右侧面板：大缩略图 + 名称/简介/难度/敌人数
void LevelSelectScene::draw_preview_panel(Context* ctx, int index) {
    jr2d_p r2d = ctx->r2d;
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(r2d, &lw, &lh, NULL);
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;

    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    bool valid = (index >= 0 && index < LEVEL_META_COUNT);
    bool ready = (valid && index < LEVEL_SLOT_MAX) ? level_ready[index] : false;

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

    // 预览框
    // 预览框跟随面板宽度拉伸，左右各留 8*s
    float vx = PREVIEW_X * s, vy = PREVIEW_Y * s;
    float vw = px + pw - 8.0f * s - vx;
    if (vw < PREVIEW_W * s) vw = PREVIEW_W * s;
    float vh = PREVIEW_H * s;
    if (ready) {
        draw_mini_map(ctx, index, vx, vy, vw, vh, s, true);
    } else {
        r2d_set_color(r2d, 232, 230, 224, 255);
        shape_draw_rectangle(r2d, "fill", { vx, vy, vw, vh });
        r2d_set_color(r2d, 190, 188, 182, 255);
        shape_draw_rectangle(r2d, "line", { vx, vy, vw, vh });
        r2d_set_color(r2d, 20, 20, 26, 140);
        shape_draw_rectangle(r2d, "fill", { vx, vy, vw, vh });
        const char* t = "地图制作中";
        int tn = 0;
        for (const unsigned char* q = (const unsigned char*)t; *q; ++q)
            if ((*q & 0xC0) != 0x80) tn++;
        jtext_printx(r2d, font24, t, (int)strlen(t), { 245, 243, 237, 255 },
                     vx + (vw - tn * 24.0f * s) * 0.5f, vy + vh * 0.5f - 12.0f * s);
    }

    if (!valid) return;

    // 名称
    jcolor_t namec = ready ? jcolor_t{ 28, 28, 32, 255 } : jcolor_t{ 150, 150, 156, 255 };
    jtext_printx(r2d, font24, LEVEL_META[index].name, (int)strlen(LEVEL_META[index].name),
                 namec, PANEL_X * s + 8.0f * s, 200.0f * s);

    // 简介
    jcolor_t descc = ready ? jcolor_t{ 90, 90, 98, 255 } : jcolor_t{ 168, 168, 174, 255 };
    jtext_printx(r2d, font12, LEVEL_META[index].desc, (int)strlen(LEVEL_META[index].desc),
                 descc, PANEL_X * s + 8.0f * s, 226.0f * s);

    // 元信息：难度 + 敌人数 + 状态
    float metaX = PANEL_X * s + 8.0f * s;
    if (ready) {
        draw_difficulty(ctx, index, metaX, 246.0f * s, s);
        char meta[64];
        // 显示本关真正要击杀的敌人总数（LEVEL_ENEMY_TOTALS，默认 100）。
        // 地图 Enemy 层出生点数只是刷怪位置数量，对玩家没有意义，不再展示。
        snprintf(meta, sizeof(meta), "敌人 %d", level_enemy_total(index + 1));
        jtext_printx(r2d, font12, meta, (int)strlen(meta), { 110, 110, 118, 255 },
                     metaX + 70.0f * s, 244.0f * s);
    } else {
        jtext_printx(r2d, font12, "地图制作中，暂不可进入", (int)strlen("地图制作中，暂不可进入"),
                     { 200, 60, 50, 255 }, metaX, 245.0f * s);
    }
}

// 按 scrollview 的当前偏移刷新所有关卡行按钮位置（绘制与命中都基于按钮位置）
void LevelSelectScene::apply_level_scroll() {
    if (!level_view) return;
    const float oy = scrollview_get_offset_y(level_view);
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(this->ctx()->r2d, &lw, &lh, NULL);
    (void)lw;
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++)
        if (btn_levels[i])
            button_set_position(btn_levels[i], LIST_X * s, level_base_y[i] * s + oy);
}

// 键盘上下换关时，把选中行滚进左侧列表视口
void LevelSelectScene::ensure_selected_visible() {
    if (!level_view || selected < 0 || selected >= LEVEL_META_COUNT) return;
    int lw = 0, lh = 0;
    r2d_get_logical_presentation(this->ctx()->r2d, &lw, &lh, NULL);
    (void)lw;
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    float off = scrollview_get_offset_y(level_view);
    const float view_top = LIST_Y * s;
    const float view_bot = (LIST_Y + LIST_H) * s;
    float row_top = level_base_y[selected] * s + off;
    float row_bot = row_top + ROW_H * s;
    if (row_top < view_top)      off += (view_top - row_top);
    else if (row_bot > view_bot) off -= (row_bot - view_bot);
    scrollview_set_offset(level_view, scrollview_get_offset_x(level_view), off);
    apply_level_scroll();
}

// 画"此关正在制作中"模态提示：半透明遮罩 + 居中的宣纸底提示框
void LevelSelectScene::draw_tip() {
    Context* ctx = this->ctx();
    jr2d_p r2d = ctx->r2d;
    font_p font24 = AssetManager::GetInstance()->GetSimheiFont24();
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);

    r2d_set_color(r2d, 20, 20, 26, 120);
    shape_draw_rectangle(r2d, "fill", jrect_t{ 0, 0, (float)w, (float)h });

    const float s = (h > 0) ? ((float)h / 320.0f) : 1.0f;
    const float bw = 230.0f * s, bh = 86.0f * s;
    const float bx = ((float)w - bw) * 0.5f;
    const float by = ((float)h - bh) * 0.5f;
    r2d_set_color(r2d, 248, 246, 240, 255);
    shape_draw_rectangle(r2d, "fill", jrect_t{ bx, by, bw, bh });
    r2d_set_color(r2d, 40, 40, 45, 255);
    shape_draw_rectangle(r2d, "line", jrect_t{ bx, by, bw, bh });

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

    self->selected = (ctx->selected_level >= 1 && ctx->selected_level <= LEVEL_META_COUNT)
        ? ctx->selected_level - 1 : 0;

    // 探测各关地图是否已制作（Tiled 导出的 levelN.lua 是否存在）。
    self->tip_active = false;
    for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++)
        self->level_ready[i] = level_map_available(i + 1);
    if (self->selected >= 0 && self->selected < LEVEL_META_COUNT
        && !self->level_ready[self->selected]) {
        for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++) {
            if (self->level_ready[i]) { self->selected = i; break; }
        }
    }

    // 缓存每关真实地图格子 + 敌人数量，供右侧大缩略图与信息面板使用
    for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++) {
        self->preview_has_[i] = false;
        self->preview_w_[i] = self->preview_h_[i] = 0;
        self->preview_player_[i] = -1;
        if (self->level_ready[i]) {
            map_load_level(i + 1);
            int mw = MAP_W, mh = MAP_H;
            int n = mw * mh;
            if (n > 4096) n = 4096;
            std::memcpy(self->preview_grid_[i], g_map_grid, (size_t)n);
            self->preview_w_[i] = mw;
            self->preview_h_[i] = mh;
            self->preview_player_[i] = g_map_info.player_cell;
            self->preview_has_[i] = true;
        }
    }

    // 选关页改用自适应逻辑宽度：高度固定 640，宽度跟随窗口宽高比，
    // 背景铺满整屏、没有左右黑边（与游戏场景一致）。
    presentation_set_adaptive(ctx->r2d);

    int lw = 0, lh = 0;
    r2d_get_logical_presentation(ctx->r2d, &lw, &lh, NULL);
    const float s = (lh > 0) ? ((float)lh / 320.0f) : 1.0f;
    const float LW = (float)((lw > 0) ? lw : 960);
    const float LH = (float)((lh > 0) ? lh : 640);

    // 左侧列表：scrollview 纵向滚动（定宽靠左）
    self->level_view = scrollview_create(ctx->r2d, 0.0f, 0.0f, 1.0f, 1.0f);
    if (self->level_view) {
        scrollview_set_view_px(self->level_view, LIST_X * s, LIST_Y * s, LIST_W * s, LIST_H * s);
        scrollview_set_content(self->level_view, LIST_W * s,
                (LIST_Y + 4.0f + (float)LEVEL_META_COUNT * ROW_SPACING + 8.0f) * s);
    }

    // 列表行按钮（透明命中区；外观由 draw_level 自绘）。按像素换算成比例创建，
    // 这样在自适应宽度下仍精确落在 LIST_X 起的位置，而不是按 480 宽比例漂移。
    for (int i = 0; i < LEVEL_META_COUNT && i < LEVEL_SLOT_MAX; i++) {
        self->level_base_y[i] = LIST_Y + 4.0f + (float)i * ROW_SPACING;
        const float cx_px = (LIST_X + LIST_W * 0.5f) * s;
        const float cy_px = (self->level_base_y[i] + ROW_H * 0.5f) * s;
        self->btn_levels[i] = button_create(ctx->r2d,
                cx_px / LW, cy_px / LH,
                (LIST_W * s) / LW, (ROW_H * s) / LH);
        UIStyle::applyPaperButton(self->btn_levels[i]);
    }
    self->apply_level_scroll();

    // 右下「开始」：锚定右边缘（宽屏时跟着面板右边界走；设计稿下等于 398*2）
    {
        const float cx_px = LW - 82.0f * s;
        const float cy_px = 286.0f * s;
        self->btn_confirm = button_create(ctx->r2d,
                cx_px / LW, cy_px / LH,
                (116.0f * s) / LW, (32.0f * s) / LH);
        UIStyle::applyPaperButton(self->btn_confirm);
        button_set_text_offset(self->btn_confirm, 0, 2.0f);
        button_set_textx(self->btn_confirm, font24, "开始", (int)strlen("开始"), { 0, 0, 0, 255 });
    }

    // 左下「返回」：锚定面板左边界（宽屏不变；设计稿下等于 232+48）
    {
        const float cx_px = 280.0f * s;
        const float cy_px = 286.0f * s;
        self->btn_back = button_create(ctx->r2d,
                cx_px / LW, cy_px / LH,
                (96.0f * s) / LW, (32.0f * s) / LH);
        UIStyle::applyPaperButton(self->btn_back);
        button_set_text_offset(self->btn_back, 0, 2.0f);
        button_set_textx(self->btn_back, font24, "返回", (int)strlen("返回"), { 0, 0, 0, 255 });
    }
}

void LevelSelectScene::HandleUiEvent(const void* e) {
    auto* self = this;
    const SDL_Event* ev = (const SDL_Event*)e;

    // "此关正在制作中"提示为模态：任意按键/点击关闭
    if (self->tip_active) {
        if (ev->type == SDL_EVENT_KEY_DOWN) { self->tip_active = false; return; }
        jevent_callbacks_t tcbs = {};
        tcbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
            (void)x; (void)y; (void)btn;
            ((LevelSelectScene*)arg)->tip_active = false;
        };
        dispatch_event(tcbs, e, self);
        return;
    }

    // ---- 键盘导航：↑/↓ 换关，Enter 开始，Esc 返回 ----
    if (ev->type == SDL_EVENT_KEY_DOWN) {
        const int sc = ev->key.scancode;
        if (sc == SDL_SCANCODE_ESCAPE || sc == SDL_SCANCODE_AC_BACK) {
            request_scene(self->ctx(), new CharacterSelectScene(self->ctx(), self->target_mode));
            return;
        }
        if (sc == SDL_SCANCODE_UP || sc == SDL_SCANCODE_LEFT) {
            if (self->selected > 0) self->selected--;
            self->ensure_selected_visible();
            return;
        }
        if (sc == SDL_SCANCODE_DOWN || sc == SDL_SCANCODE_RIGHT) {
            if (self->selected + 1 < LEVEL_META_COUNT) self->selected++;
            self->ensure_selected_visible();
            return;
        }
        if (sc == SDL_SCANCODE_RETURN || sc == SDL_SCANCODE_KP_ENTER ||
            sc == SDL_SCANCODE_SPACE) {
            if (self->selected < 0 || self->selected >= 6 || !self->level_ready[self->selected]) {
                self->tip_active = true;
            } else {
                self->ctx()->selected_level = self->selected + 1;
                request_scene(self->ctx(), new LoadingScene(self->ctx(), self->target_mode));
            }
            return;
        }
    }

    if (self->level_view && scrollview_process_event(self->level_view, e)) return;

    jevent_callbacks_t cbs = {};
    cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
        (void)btn;
        LevelSelectScene* self = (LevelSelectScene*)arg;
        // 按下只记录行，不立即选中：滑动列表时 scrollview 会消费后续事件，
        // 松手若仍在这一行上才当作点击，避免“滑动误选关卡”。
        self->press_row_ = -1;
        self->press_x_ = x;
        self->press_y_ = y;
        for (int i = 0; i < LEVEL_META_COUNT; i++) {
            if (self->btn_levels[i] && button_is_hovered(self->btn_levels[i], x, y)) {
                self->press_row_ = i;
                break;
            }
        }
        if (self->btn_confirm && ui_button_hit(self->btn_confirm, x, y)) {
            if (self->selected < 0 || self->selected >= 6 || !self->level_ready[self->selected]) {
                self->tip_active = true;
                return;
            }
            self->ctx()->selected_level = self->selected + 1;
            request_scene(self->ctx(), new LoadingScene(self->ctx(), self->target_mode));
            return;
        }
        if (self->btn_back && ui_button_hit(self->btn_back, x, y))
            request_scene(self->ctx(), new CharacterSelectScene(self->ctx(), self->target_mode));
    };
    cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
        (void)btn;
        LevelSelectScene* self = (LevelSelectScene*)arg;
        // 拖动列表时 scrollview 已把松手事件消费掉，这里不会执行；
        // 能走到这里说明是点击，且松手仍在按下的那一行上才选中。
        if (self->press_row_ >= 0) {
            button_p b = self->btn_levels[self->press_row_];
            if (b && button_is_hovered(b, x, y)) {
                UiSfx::Get().play(UiSfx::kClick);
                self->selected = self->press_row_;
            }
        }
        self->press_row_ = -1;
        for (int i = 0; i < LEVEL_META_COUNT; i++)
            if (self->btn_levels[i]) button_set_pressed(self->btn_levels[i], false);
        if (self->btn_confirm) button_set_pressed(self->btn_confirm, false);
        if (self->btn_back)    button_set_pressed(self->btn_back, false);
    };
    cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
        (void)dx; (void)dy;
        LevelSelectScene* self = (LevelSelectScene*)arg;
        self->hovered_ = -1;
        for (int i = 0; i < LEVEL_META_COUNT; i++)
            if (self->btn_levels[i] && button_is_hovered(self->btn_levels[i], x, y))
                self->hovered_ = i;
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
    font_p font12 = AssetManager::GetInstance()->GetSimheiFont12();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);

    // 米色纸底
    r2d_set_color(r2d, 238, 234, 226, 255);
    shape_draw_rectangle(r2d, "fill", { 0, 0, (float)w, (float)h });

    const float s = (h > 0) ? ((float)h / 320.0f) : 1.0f;

    // 顶部墨色标题带
    r2d_set_color(r2d, 40, 40, 48, 255);
    shape_draw_rectangle(r2d, "fill", { 0, 0, (float)w, BAND_H * s });
    jtext_printx(r2d, font24, "选择关卡", (int)strlen("选择关卡"),
                 { 245, 243, 237, 255 }, 16.0f * s, 6.0f * s);

    char chBuf[64];
    snprintf(chBuf, sizeof(chBuf), "当前角色：%s",
             g_characterCatalog.get(self->ctx()->selected_char_id).name.c_str());
    int cw = 0;
    if (jtext_measurex(r2d, font12, chBuf, (int)strlen(chBuf), { 200, 200, 208, 255 }, &cw, NULL))
        jtext_printx(r2d, font12, chBuf, (int)strlen(chBuf), { 200, 200, 208, 255 },
                     (float)w - (float)cw - 16.0f * s, 11.0f * s);

    // 左侧列表（裁剪 + 滚动条）
    self->apply_level_scroll();
    if (self->level_view) scrollview_begin(self->level_view);
    for (int i = 0; i < LEVEL_META_COUNT; i++) draw_level(i);
    if (self->level_view) scrollview_end(self->level_view);

    // 右侧详情面板（不滚动）
    self->draw_preview_panel(ctx, self->selected);

    if (self->btn_confirm) button_draw(self->btn_confirm);
    if (self->btn_back)    button_draw(self->btn_back);

    if (self->tip_active) self->draw_tip();
}

void LevelSelectScene::Destroy() {
    auto* self = this;
    for (int i = 0; i < LEVEL_META_COUNT; i++)
        if (self->btn_levels[i]) { button_destroy(self->btn_levels[i]); self->btn_levels[i] = NULL; }
    if (self->level_view) { scrollview_destroy(self->level_view); self->level_view = NULL; }
    if (self->btn_confirm) { button_destroy(self->btn_confirm); self->btn_confirm = NULL; }
    if (self->btn_back)    { button_destroy(self->btn_back);    self->btn_back = NULL; }
}
