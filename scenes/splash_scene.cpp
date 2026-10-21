#include "splash_scene.h"
#include <joy/joy.h>
#include <stdio.h>
#include "main_scene.h"
#include "../asset_manager.h"

SplashScene::SplashScene(Context* ctx) : Scene(ctx, "Splash") {}

void SplashScene::Load() {
    auto* self = this;
    // 品牌资源直接访问（不进资源索引），路径取自 AssetManager 常量，避免散落。
    // 注：原写 "textures/logo.png" 是死路径——assets 下只有 branding/，无 textures/。
    self->logo = image_create(self->ctx()->r2d, AssetManager::kPathBrandingLogo);
    log_info("[splash scene] start");
}

void SplashScene::Update(float dt) {
    auto* self = this;
    self->elapsed += dt;

    // 淡入：0~0.5s，透明度从 0→1
    if (self->elapsed < 0.5f) {
        self->alpha = self->elapsed / 0.5f;
    }
    // 保持：0.5s~2.5s，透明度 = 1
    else if (self->elapsed < 2.5f) {
        self->alpha = 1.0f;
        self->faded_in = true;
    }
    // 淡出：2.5s~3.0s，透明度从 1→0
    else if (self->elapsed < 3.0f) {
        self->alpha = 1.0f - (self->elapsed - 2.5f) / 0.5f;
        self->fading_out = true;
    }
    // 3.0s 后切换到主场景
    else {
        self->ctx()->scene_mgr->requestTransition(new MainScene(self->ctx()), TRANSITION_SPLASH_DONE);
    }
}

void SplashScene::Render() {
    auto* self = this;
    jr2d_p r2d = self->ctx()->r2d;
    font_p font = AssetManager::GetInstance()->GetSimheiFont24();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);

    // 背景：白色
    jcolor_t bg = { 255, 255, 255, 255 };
    r2d_set_color(r2d, bg.r, bg.g, bg.b, bg.a);
    jrect_t full = { 0, 0, (float)w, (float)h };
    shape_draw_rectangle(r2d, "fill", full);

    // Logo（居中）
    image_p logo = self->logo;
    if (logo) {
        // 尺寸/位置比例化：100 与 50 均相对设计稿 480x320；
        // 宽、高分别除以 480 / 320，保证缩放后仍是正方形
        // 用高度基准算边长，保证宽屏下 logo 仍是正方形（不横向拉伸）
        const float sy = (h > 0) ? ((float)h / 320.0f) : 1.0f;
        const float side = 100.0f * sy;
        const float lw_f = (w > 0) ? (side / (float)w) : 0.0f;
        const float lh_f = (h > 0) ? (side / (float)h) : 0.0f;
        image_draw1_f(r2d, logo, NULL,
                      (1.0f - lw_f) * 0.5f, 50.0f / 320.0f, lw_f, lh_f);
    }

    // 游戏标题（Logo 下方，水平居中）。白底下用深色字保证可读。
    const char* title = "Joy Topdown";
    jcolor_t title_c = { 30, 30, 40, 255 };
    int tw = 0, th = 0;
    jtext_measurex(r2d, font, title, (int)strlen(title), title_c, &tw, &th);
    jtext_printf(r2d, font, title, (int)strlen(title), title_c,
                 ((float)w - (float)tw) * 0.5f / (float)w, 170.0f / 320.0f);

    // 版权信息（底部居中）
    font_p font_small = AssetManager::GetInstance()->GetSimheiFont12();
    if (font_small) {
        jcolor_t ver_c = { 90, 90, 100, 255 };
        const char* dev = "Developed by 大懒虫工作室, 228407354@qq.com";
        int dw = 0, dh = 0;
        jtext_measurex(r2d, font_small, dev, (int)strlen(dev), ver_c, &dw, &dh);
        jtext_printf(r2d, font_small, dev, (int)strlen(dev), ver_c,
                     ((float)w - (float)dw) * 0.5f / (float)w, 1.0f - 24.0f / 320.0f);
    }
}

void SplashScene::Destroy() {
    auto* self = this;
    if (self->logo) image_destroy(self->logo);
}




