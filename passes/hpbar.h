#ifndef JOY_PASSES_HPBAR_H
#define JOY_PASSES_HPBAR_H
// 血条（胶囊纹理）公共实现：玩家 / 敌人共用。
// 与 render_math.h 同一模式：被多个 pass 头包含，靠 include guard 保证单次展开。
//
// 为何用纹理而非 shape_draw_polygon：后者走 SDL_RenderGeometry，在本项目
// 逻辑呈现(LETTERBOX)下不输出；image_draw_ex 走 SDL_RenderTextureRotated
// （角色/武器/敌人都在用）可靠且支持旋转。
// 结构：轨道纹理 = 外 pad 像素"深墨描边 + 半透明墨底"；填充纹理 = 透明底 + 彩色胶囊，
//        每帧按血量比例重绘像素后上传。两者都 1:1 绘制、绕中心旋转。
#include <joy/joy.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

// 胶囊像素：在 buf(W×H, RGBA32) 画一条从 x=0 到 x=fillLen 的胶囊（两端半圆、1px 羽化）。
static void hpbar_gen_capsule(uint8_t* buf, int W, int H, float fillLen,
                              uint8_t R, uint8_t G, uint8_t B, uint8_t A)
{
        memset(buf, 0, (size_t)W * H * 4);
        if (fillLen <= 0.0f) return;
        if (fillLen > (float)W) fillLen = (float)W;
        float cy = H * 0.5f;
        float rad = H * 0.5f;
        float x0 = rad, x1 = fillLen - rad;
        if (x1 < x0) {                       // 太短：退化为圆帽
                rad = fillLen * 0.5f;
                if (rad < 0.5f) rad = 0.5f;
                x0 = x1 = fillLen * 0.5f;
        }
        for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                        float px = x + 0.5f, py = y + 0.5f;
                        float dx = (px < x0) ? (x0 - px) : ((px > x1) ? (px - x1) : 0.0f);
                        float dy = py - cy;
                        float cov = rad - sqrtf(dx * dx + dy * dy) + 0.5f;   // 1px 抗锯齿
                        if (cov <= 0.0f) continue;
                        if (cov > 1.0f) cov = 1.0f;
                        uint8_t* p = buf + ((size_t)y * W + x) * 4;
                        p[0] = R; p[1] = G; p[2] = B;
                        p[3] = (uint8_t)(A * cov);
                }
        }
}

// 轨道+描边：外 pad 像素深墨描边、内部半透明墨（同一墨色，靠 alpha 深浅区分）。
static void hpbar_gen_track(uint8_t* buf, int W, int H, int pad)
{
        memset(buf, 0, (size_t)W * H * 4);
        float cy = H * 0.5f;
        float rOut = H * 0.5f;
        float rIn  = rOut - (float)pad;
        float x0o = rOut, x1o = W - rOut;
        for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                        float px = x + 0.5f, py = y + 0.5f;
                        float dx = (px < x0o) ? (x0o - px) : ((px > x1o) ? (px - x1o) : 0.0f);
                        float dy = py - cy;
                        float covO = rOut - sqrtf(dx * dx + dy * dy) + 0.5f;
                        if (covO <= 0.0f) continue;
                        if (covO > 1.0f) covO = 1.0f;
                        float covI = rIn - sqrtf(dx * dx + dy * dy) + 0.5f;
                        uint8_t* p = buf + ((size_t)y * W + x) * 4;
                        p[0] = 40; p[1] = 40; p[2] = 45;
                        if (covI >= 1.0f) p[3] = 120;                    // 内部：半透明墨
                        else              p[3] = (uint8_t)(205.0f * covO); // 描边：更实
                }
        }
}

// 惰性创建两张纹理：track 尺寸 (w+2pad)×(h+2pad)（固定，只建一次）；
// fill 尺寸 w×h（流式，每帧由 hpbar_draw 更新像素）。返回两张是否都可用。
static bool hpbar_ensure(jr2d_p r2d, image_p* track, image_p* fill, int w, int h, int pad)
{
        if (!*track) {
                const int W = w + pad * 2, H = h + pad * 2;
                *track = image_create_empty(r2d, W, H);
                if (!*track) return false;
                uint8_t* buf = (uint8_t*)malloc((size_t)W * H * 4);
                if (!buf) return false;
                hpbar_gen_track(buf, W, H, pad);
                jrect_t r = { 0.0f, 0.0f, (float)W, (float)H };
                image_update(*track, &r, buf, W * 4);
                free(buf);
                image_set_blend_mode(*track, JR2D_BLEND_BLEND);
        }
        if (!*fill) {
                *fill = image_create_empty(r2d, w, h);
                if (*fill) image_set_blend_mode(*fill, JR2D_BLEND_BLEND);
        }
        return (*track != NULL && *fill != NULL);
}

// 在 (cx,cy) 画一条血条：轨道在下、填充在上（自 -长轴 端起按 ratio 填充）。
// rot 为旋转弧度（玩家传瞄准角，敌人传 0 即水平）；(fr,fg,fb) 为填充色。
static void hpbar_draw(jr2d_p r2d, image_p track, image_p fill,
                       int w, int h, int pad, float ratio,
                       float cx, float cy, float rot,
                       uint8_t fr, uint8_t fg, uint8_t fb)
{
        if (!r2d || !track || !fill) return;
        if (ratio < 0.0f) ratio = 0.0f;
        else if (ratio > 1.0f) ratio = 1.0f;

        // 填充纹理按当前比例/颜色重绘（w×h 很小，逐帧开销可忽略）
        uint8_t* buf = (uint8_t*)malloc((size_t)w * h * 4);
        if (buf) {
                hpbar_gen_capsule(buf, w, h, (float)w * ratio, fr, fg, fb, 235);
                jrect_t r = { 0.0f, 0.0f, (float)w, (float)h };
                image_update(fill, &r, buf, w * 4);
                free(buf);
        }

        jrect_t src_t = { 0.0f, 0.0f, (float)(w + pad * 2), (float)(h + pad * 2) };
        jrect_t src_f = { 0.0f, 0.0f, (float)w, (float)h };
        vec2_t pos  = { cx, cy };
        vec2_t scl  = { 1.0f, 1.0f };
        vec2_t org_t = { (w + pad * 2) * 0.5f, (h + pad * 2) * 0.5f };
        vec2_t org_f = { w * 0.5f, h * 0.5f };
        image_draw_ex(track, &src_t, pos, rot, scl, org_t);
        if (ratio > 0.0f)
                image_draw_ex(fill, &src_f, pos, rot, scl, org_f);
}
#endif
