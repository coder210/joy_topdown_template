#include "loading_scene.h"
#include <joy/joy.h>
#include <stdio.h>
#include "game_scene.h"
#include "../asset_manager.h"

LoadingScene::LoadingScene(Context* ctx, GameModeKind mode) : Scene(ctx, "Loading") {
    target_mode = mode;
}

void LoadingScene::Load() {
    log_info("[loading scene] loading...");
}

void LoadingScene::Update(float dt) {
    auto* self = this;
    self->elapsed += dt;

    // 加点动画：每 0.4s 切换一个点
    self->dot_timer += dt;
    if (self->dot_timer >= 0.4f) {
        self->dot_count = (self->dot_count + 1) % 4;
        self->dot_timer = 0;
    }

    // 0.5s 后创建游戏场景（模拟"加载"过程）
    if (!self->loaded && self->elapsed >= 0.5f) {
        reset_context_for_mode(self->ctx(), self->target_mode);
        // 运行模式由 GameScene 构造函数按 ctx->mode 从 kModeConfigs 取能力配置
        self->target_scene.reset(new GameScene(self->ctx()));
        self->loaded = true;
    }

    // 1.5s 后切换到游戏场景
    if (self->loaded && self->elapsed >= 1.5f) {
        self->ctx()->scene_mgr->requestTransition(self->target_scene.release(), TRANSITION_LOADING_DONE);
    }
}

void LoadingScene::Render() {
    auto* self = this;
    jr2d_p r2d = self->ctx()->r2d;
    font_p font = AssetManager::GetInstance()->GetSimheiFont24();

    // 背景：白底（水墨/图纸风）
    jcolor_t bg = { 255, 255, 255, 255 };
    r2d_set_color(r2d, bg.r, bg.g, bg.b, bg.a);
    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);
    jrect_t full = { 0, 0, (float)w, (float)h };
    shape_draw_rectangle(r2d, "fill", full);

    // 加载文字（居中，墨色）
    jcolor_t c = { 40, 40, 45, 255 };
    char text[32];
    int dot_count = self->dot_count;
    if (dot_count == 0) snprintf(text, sizeof(text), "加载中");
    else if (dot_count == 1) snprintf(text, sizeof(text), "加载中.");
    else if (dot_count == 2) snprintf(text, sizeof(text), "加载中..");
    else snprintf(text, sizeof(text), "加载中...");
    jtext_printx(r2d, font, text, (int)strlen(text), c, (w - 120) / 2.0f, h / 2.0f - 12);
}

void LoadingScene::Destroy() {
}




