#include "main_scene.h"
#include <joy/joy.h>
#include <stdio.h>
#include <math.h>
#include <stdlib.h>
#include "../layers/menu_layer.h"
#include "../asset_manager.h"

MainScene::MainScene(Context* ctx) : Scene(ctx, "Main") {}

void MainScene::Load() {
    auto* self = this;
    log_info("[main scene] start...");

    int logical_w, logical_h;
    r2d_get_logical_presentation(self->ctx()->r2d, &logical_w, &logical_h, NULL);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        self->particles[i].x = (float)(rand() % logical_w);
        self->particles[i].y = (float)(rand() % logical_h);
        self->particles[i].vx = ((float)rand() / RAND_MAX - 0.5f) * 6.0f;
        self->particles[i].vy = ((float)rand() / RAND_MAX - 0.5f) * 6.0f;
        self->particles[i].alpha = 0.08f + (float)rand() / RAND_MAX * 0.15f;
        self->particles[i].size = 1.5f + (float)rand() / RAND_MAX * 1.5f;
        self->particles[i].phase = (float)rand() / RAND_MAX * 6.2832f;
        self->particles[i].twinkle_speed = 0.8f + (float)rand() / RAND_MAX * 1.5f;
    }

    self->menu = new MenuLayer(self->ctx());   // 纯 button 菜单层，由 Render/HandleUiEvent 桥接
}

void MainScene::Update(float dt) {
    auto* self = this;
    self->elapsed += dt;

    int logical_w, logical_h;
    r2d_get_logical_presentation(self->ctx()->r2d, &logical_w, &logical_h, NULL);

    for (int i = 0; i < MAX_PARTICLES; i++) {
        self->particles[i].x += self->particles[i].vx * dt;
        self->particles[i].y += self->particles[i].vy * dt;
        if (self->particles[i].x < 0) self->particles[i].x += (float)logical_w;
        if (self->particles[i].x > logical_w) self->particles[i].x -= (float)logical_w;
        if (self->particles[i].y < 0) self->particles[i].y += (float)logical_h;
        if (self->particles[i].y > logical_h) self->particles[i].y -= (float)logical_h;
    }
}

void MainScene::Render() {
    auto* self = this;
    jr2d_p r2d = self->ctx()->r2d;
    font_p font = AssetManager::GetInstance()->GetSimheiFont24();

    int w, h;
    r2d_get_logical_presentation(r2d, &w, &h, NULL);

    // 背景：白色，与 splash / 游戏内统一
    jcolor_t bg = { 255, 255, 255, 255 };
    r2d_set_color(r2d, bg.r, bg.g, bg.b, bg.a);
    jrect_t full = { 0, 0, (float)w, (float)h };
    shape_draw_rectangle(r2d, "fill", full);

    // ---- 细微粒子（深色，白底下可见，保持极淡）----
    jcolor_t pc = { 70, 70, 85, 0 };
    for (int i = 0; i < MAX_PARTICLES; i++) {
        struct particle* p = &self->particles[i];
        float twinkle = 0.5f + 0.5f * sinf(self->elapsed * p->twinkle_speed + p->phase);
        float a = p->alpha * twinkle;
        if (a < 0.01f) continue;
        pc.a = (uint8_t)(a * 255.0f);
        r2d_set_color(r2d, pc.r, pc.g, pc.b, pc.a);
        jrect_t pr = { p->x, p->y, p->size, p->size };
        shape_draw_rectangle(r2d, "fill", pr);
    }

    // ---- 标题（顶部，水平居中）----
    float pulse = 0.8f + 0.2f * sinf(self->elapsed * 1.8f);
    const char* title = "Joy Topdown";
    int title_len = (int)strlen(title);
    jcolor_t tc = { 35, 35, 48, (uint8_t)(pulse * 255.0f) };
    int tw = 0, th = 0;
    jtext_measurex(r2d, font, title, title_len, tc, &tw, &th);
    // 位置比例化：22/320 是相对设计稿 480x320 的顶部留白，逻辑分辨率改变时自动适配
    jtext_printf(r2d, font, title, title_len, tc,
                 ((float)w - (float)tw) * 0.5f / (float)w, 22.0f / 320.0f);

    // 版本号（底部居中）
    font_p font_small = AssetManager::GetInstance()->GetSimheiFont12();
    if (font_small) {
        const char* ver = "v1.0.0";
        jcolor_t vc = { 90, 90, 100, 255 };
        int vw = 0, vh = 0;
        jtext_measurex(r2d, font_small, ver, (int)strlen(ver), vc, &vw, &vh);
        jtext_printf(r2d, font_small, ver, (int)strlen(ver), vc,
                     ((float)w - (float)vw) * 0.5f / (float)w, 1.0f - 20.0f / 320.0f);
    }
    // 渲染菜单层（按钮/标题等）：纯 button 层直接绘制
    if (this->menu) this->menu->Render();
}

void MainScene::Destroy() {
    if (this->menu) { delete this->menu; this->menu = NULL; }
}

void MainScene::HandleUiEvent(const void* ev) {
    // 转发事件给 menu_layer（纯 button，内部用 dispatch_event 解析命中）
    if (this->menu) this->menu->HandleEvent(ev);
}




