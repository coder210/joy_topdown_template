#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "gameplay_controls_layer.h"
#include "../ui_style.h"
#include "../asset_manager.h"
#include "../components/player_component.h"
#include "../components/action_state_component.h"

static const float GC_PI = 3.14159265f;

// 本地玩家某项冷却剩余比例（0..1 = 剩余/总量）。无本地玩家或未冷却返回 0。
// coolType: 0=无冷却(方向), 1=近战, 2=射击, 3=射线。
float GameplayControlsLayer::localCoolRatio(int coolType)
{
        if (!ctx_ || net_session(ctx_).local_conv == 0) return 0.0f;
        auto view = ctx_->ecs.view<PlayerComponent, ActionStateComponent>();
        for (auto e : view) {
                auto& pc  = view.get<PlayerComponent>(e);
                if (pc.conv != (int)net_session(ctx_).local_conv) continue;
                auto& act = view.get<ActionStateComponent>(e);
                int cd = 0, max = 1;
                if (coolType == 1)      { cd = act.melee_cooldown;   max = ActionStateComponent::MELEE_COOLDOWN; }

                else return 0.0f;
                if (cd <= 0) return 0.0f;
                return (float)cd / (float)max;
        }
        return 0.0f;
}

// 墨色环形冷却进度：以 (cx,cy) 圆心、半径 R 的圆环（厚 thick），从正上方顺时针填充
// 已耗进度 frac∈[0,1]。纯表现层，不影响帧同步。
void GameplayControlsLayer::drawCoolRing(float cx, float cy,
                float R, float thick, float frac)
{
        if (!ctx_ || frac <= 0.01f) return;
        jr2d_p r2d = ctx_->r2d;
        constexpr int SEGS = 40;
        if (frac > 1.0f) frac = 1.0f;
        int filled = (int)(frac * SEGS);
        if (filled < 7) filled = 7;                 // 保证可见的一小段弧
        if (filled > SEGS) filled = SEGS;
        const float a0 = -GC_PI * 0.5f;
        r2d_set_color(r2d, 40, 40, 45, 200);        // 深墨
        for (int i = 0; i < filled; i++) {
                float a1 = a0 + (float)i * (2.0f * GC_PI / SEGS);
                float a2 = a0 + (float)(i + 1) * (2.0f * GC_PI / SEGS);
                for (int k = 0; k < (int)thick; k++) {
                        float rr = R + (float)k;
                        shape_draw_line(r2d, cx + cosf(a1) * rr, cy + sinf(a1) * rr,
                                              cx + cosf(a2) * rr, cy + sinf(a2) * rr);
                }
        }
}

// 圆按钮右下角叠加键盘键角标（方案 A，对齐 3D）：右下 1/4 处深墨小字母。
void GameplayControlsLayer::drawKeyBadge(font_p font, const char* key,
                float bx, float by)
{
        if (!ctx_) return;
        jr2d_p r2d = ctx_->r2d;
        if (!r2d || !font || !key || !key[0]) return;
        float size = 14.0f;                          // 角标像素高度（与 font 实际字号一致）
        float tw = size * 0.5f;                      // 单字符宽≈fontSize/2
        jcolor_t c = { 60, 60, 70, 255 };
        float px = floorf(bx - tw * 0.5f + 0.5f);    // 对齐到最近整数像素
        float py = floorf(by - size * 0.5f + 0.5f);
        jtext_printx(r2d, font, key, (int)strlen(key), c, px, py);
}

// 单个圆形按钮绘制（去掉 node 依赖后由 Render 直接调用）
void GameplayControlsLayer::drawButton(const gc_button_meta* m)
{
        if (!ctx_ || !m || !m->btn) return;
        jr2d_p r2d = ctx_->r2d;
        float cx = m->x + m->w * 0.5f;
        float cy = m->y + m->h * 0.5f;
        float R  = (m->w < m->h ? m->w : m->h) * 0.5f;

        // 最外描边半径（冷却环画在其外侧，两种底都通用）
        float out = R + 2.0f;

        // 纹理底（Style B 浅色圆钮 / 方向键）：按下用 *_highlight 图。
        // 方案1：保留墨色文字、键盘角标、冷却环，仅把程序白圆替换为素材纹理，
        // 贴合游戏浅色水墨 UI；纹理缺失时回退到原程序白底圆，避免回归。
        const char* basePath = "joy_topdown_template/sprites/ui/button_circle.png";
        const char* hlPath   = "joy_topdown_template/sprites/ui/button_circle_highlight.png";
        bool circleBase = true;   // 圆钮底；方向键为盾形底（墨色描边只画外圈）
        if (m->label) {
                if      (strcmp(m->label, "上") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_north.png";  hlPath = "joy_topdown_template/sprites/ui/dpad_element_north_highlight.png"; circleBase = false; }
                else if (strcmp(m->label, "下") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_south.png";  hlPath = "joy_topdown_template/sprites/ui/dpad_element_south_highlight.png"; circleBase = false; }
                else if (strcmp(m->label, "左") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_west.png";   hlPath = "joy_topdown_template/sprites/ui/dpad_element_west_highlight.png"; circleBase = false; }
                else if (strcmp(m->label, "右") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_east.png";   hlPath = "joy_topdown_template/sprites/ui/dpad_element_east_highlight.png"; circleBase = false; }
        }
        bool pressed = (m->btn == pressed_btn);
        auto* am = AssetManager::GetInstance();
        image_p tex = (am && ctx_) ? am->GetImage(r2d, pressed ? hlPath : basePath) : NULL;
        if (tex) {
                jrect_t src = { 0.0f, 0.0f, 64.0f, 64.0f };
                jrect_t dst = { m->x, m->y, m->w, m->h };
                image_draw1(tex, &src, &dst);
                // 墨色描边（水墨风识别度）：纹理填充偏浅，浅色地板上需墨线勾边。
                // 圆钮画内外双圈（恢复原白底墨边观感）；盾形方向键只画外圈避免割裂形状。
                r2d_set_color(r2d, 0, 0, 0, 255);
                shape_draw_circle(r2d, "line", { cx, cy }, out, 40);
                if (circleBase)
                        shape_draw_circle(r2d, "line", { cx, cy }, R, 40);
        } else {  // 纹理缺失：回退原程序白底圆 + 墨色描边
                r2d_set_color(r2d, 255, 255, 255, 255);
                shape_draw_circle(r2d, "fill", { cx, cy }, R, 40);
                r2d_set_color(r2d, 0, 0, 0, 255);
                shape_draw_circle(r2d, "line", { cx, cy }, R, 40);
                shape_draw_circle(r2d, "line", { cx, cy }, out, 40);
        }

        // 圆心内容：画文字
        if (m->label && m->label[0]) {
                font_p f = AssetManager::GetInstance()->GetSimheiFont24();
                jcolor_t tc = { 30, 30, 35, 255 };
                jtext_printx(r2d, f, m->label, (int)strlen(m->label), tc,
                        cx - 12.0f, cy - 12.0f);
        }

        // 冷却环：比最外描边略大一圈（深墨）
        if (m->coolType != 0) {
                float ratio = localCoolRatio(m->coolType);
                if (ratio > 0.0f)
                        drawCoolRing(cx, cy, out + 1.0f, 4.0f, 1.0f - ratio);
        }

        // 键盘角标：右下 1/4 处
        if (m->keyLabel && m->keyLabel[0]) {
                float off = R * 0.68f;
                float bx = cx + off * 0.7071f;
                float by = cy + off * 0.7071f;
                font_p f = AssetManager::GetInstance()->GetSimheiFont12();
                drawKeyBadge(f, m->keyLabel, bx, by);
        }
}

void GameplayControlsLayer::sendGameplayControlEvent(int event_code)
{
        push_user_event(GAMEPLAY_CONTROL_EVENT, event_code);
}

GameplayControlsLayer::GameplayControlsLayer(Context* ctx) : UILayer(ctx)
{
        load_dpad_panel();
}

GameplayControlsLayer::~GameplayControlsLayer() = default;

void GameplayControlsLayer::add_button(int idx, button_p* slot,
                float cx, float cy, float wf, float hf,
                const char* label, const char* keyLabel, int coolType)
{
        button_p b = button_create(ctx_->r2d, cx, cy, wf, hf);
        UIStyle::applyInkButton(b);
        *slot = b;
        int lw = 0, lh = 0;
        r2d_get_logical_presentation(ctx_->r2d, &lw, &lh, NULL);
        float w = wf * (float)lw, h = hf * (float)lh;
        float x = (cx - wf * 0.5f) * (float)lw, y = (cy - hf * 0.5f) * (float)lh;
        metas[idx] = { b, ctx_, w, h, x, y, label, keyLabel, coolType };
}

void GameplayControlsLayer::load_dpad_panel()
{
        // 俯视十字方向键（左/右/上/下），其余为技能键
        add_button(0, &btn_left,   0.09000f, 0.82000f, 0.09000f, 0.14000f, "左", "A", 0);
        add_button(1, &btn_right,0.27000f, 0.82000f, 0.09000f, 0.14000f, "右", "D", 0);
        add_button(2, &btn_up,   0.18000f, 0.71000f, 0.09000f, 0.14000f, "上", "W", 0);
        add_button(3, &btn_down, 0.18000f, 0.90000f, 0.09000f, 0.14000f, "下", "S", 0);
        add_button(4, &btn_attack,0.92500f, 0.89375f, 0.13333f, 0.20000f, "攻", "J", 1);

}

void GameplayControlsLayer::HandleEvent(const void* ev)
{
        jevent_callbacks_t cbs = {};
        cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
                (void)btn;
                auto* self = (GameplayControlsLayer*)arg;
                auto hit = [&](button_p b) { return button_is_pressed(b, x, y); };
                if (self->pressed_btn) return;   // 已按住一个钮，忽略后续按下
                if (self->btn_left && hit(self->btn_left)) {
                        self->pressed_btn = self->btn_left; self->pressed_code = GAMEPLAY_CONTROL_RELEASE_LEFT;
                        GameplayControlsLayer::sendGameplayControlEvent(GAMEPLAY_CONTROL_LEFT);
                } else if (self->btn_right && hit(self->btn_right)) {
                        self->pressed_btn = self->btn_right; self->pressed_code = GAMEPLAY_CONTROL_RELEASE_RIGHT;
                        GameplayControlsLayer::sendGameplayControlEvent(GAMEPLAY_CONTROL_RIGHT);
                } else if (self->btn_up && hit(self->btn_up)) {
                        self->pressed_btn = self->btn_up; self->pressed_code = GAMEPLAY_CONTROL_RELEASE_UP;
                        GameplayControlsLayer::sendGameplayControlEvent(GAMEPLAY_CONTROL_UP);
                } else if (self->btn_down && hit(self->btn_down)) {
                        self->pressed_btn = self->btn_down; self->pressed_code = GAMEPLAY_CONTROL_RELEASE_DOWN;
                        GameplayControlsLayer::sendGameplayControlEvent(GAMEPLAY_CONTROL_DOWN);
                } else if (self->btn_attack && hit(self->btn_attack) && self->localCoolRatio(1) <= 0.0f) {
                        self->pressed_btn = self->btn_attack; self->pressed_code = GAMEPLAY_CONTROL_ATTACK;
                }
        };
        cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
                (void)btn;
                auto* self = (GameplayControlsLayer*)arg;
                if (!self->pressed_btn) return;
                button_p b = self->pressed_btn;
                int code = self->pressed_code;
                self->pressed_btn = NULL;
                self->pressed_code = 0;
                button_set_pressed(b, false);
                if (b == self->btn_attack) {
                        // 技能：完整点击（松开仍在按钮内）才触发一次
                        if (!button_is_hovered(b, x, y)) return;
                        int coolType = 1;
                        if (self->localCoolRatio(coolType) > 0.0f) return;
                        GameplayControlsLayer::sendGameplayControlEvent(code);
                } else {
                        // 方向键：松开即清除对应位移位
                        GameplayControlsLayer::sendGameplayControlEvent(code);
                }
        };
        cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
                (void)dx; (void)dy;
                auto* self = (GameplayControlsLayer*)arg;
                // 事件给的是窗口像素；换算到逻辑坐标(480x320)供渲染层做鼠标瞄准（letterbox 自动处理）
                self->ctx_->mouse_logical = r2d_coordinates_from_window(self->ctx_->r2d, x, y);
                self->ctx_->mouse_valid   = true;
        };
        dispatch_event(cbs, ev, this);
}

void GameplayControlsLayer::Destroy()
{
        for (int i = 0; i < 5; i++) {
                if (metas[i].btn) { button_destroy(metas[i].btn); metas[i].btn = NULL; }
        }
        btn_left = btn_right = btn_up = btn_down = NULL;
        btn_attack = NULL;
        pressed_btn = NULL;
        pressed_code = 0;
}

void GameplayControlsLayer::Render()
{
        if (!ctx_) return;
        for (int i = 0; i < 5; i++) drawButton(&metas[i]);
}
