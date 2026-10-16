#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "gameplay_controls_layer.h"
#include "../ui_style.h"
#include "../asset_manager.h"
#include "../components/player_component.h"
#include "../components/action_state_component.h"
#include "../protocol.h"   // aim_yaw_quantize（摇杆瞄准角量化，与鼠标同约定）

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

// 单个按钮绘制（去掉 node 依赖后由 Render 直接调用）
// 纯纹理呈现：底图自带造型（方向键自带箭头 / 圆钮为黑色描边、中心镂空），
// 按下切 Highlights A 金色辉光图；不再叠加程序绘制的圆与键盘角标。
void GameplayControlsLayer::drawButton(const gc_button_meta* m)
{
        if (!ctx_ || !m || !m->btn) return;
        jr2d_p r2d = ctx_->r2d;
        float cx = m->x + m->w * 0.5f;
        float cy = m->y + m->h * 0.5f;
        float R  = (m->w < m->h ? m->w : m->h) * 0.5f;

        // 最外轮廓半径（冷却环画在其外侧）
        float out = R + 2.0f;

        // 纹理路径：label 决定用方向键素材还是通用圆钮素材
        const char* basePath = "joy_topdown_template/sprites/ui/button_circle.png";
        const char* hlPath   = "joy_topdown_template/sprites/ui/button_circle_highlight.png";
        bool isDpad = false;
        if (m->label) {
                if      (strcmp(m->label, "上") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_north.png"; hlPath = "joy_topdown_template/sprites/ui/dpad_element_north_highlight.png"; isDpad = true; }
                else if (strcmp(m->label, "下") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_south.png"; hlPath = "joy_topdown_template/sprites/ui/dpad_element_south_highlight.png"; isDpad = true; }
                else if (strcmp(m->label, "左") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_west.png";  hlPath = "joy_topdown_template/sprites/ui/dpad_element_west_highlight.png";  isDpad = true; }
                else if (strcmp(m->label, "右") == 0) { basePath = "joy_topdown_template/sprites/ui/dpad_element_east.png";  hlPath = "joy_topdown_template/sprites/ui/dpad_element_east_highlight.png";  isDpad = true; }
        }
        bool pressed = (m->btn == pressed_btn);
        auto* am = AssetManager::GetInstance();
        image_p tex = (am && ctx_) ? am->GetImage(r2d, pressed ? hlPath : basePath) : NULL;
        if (tex) {
                jrect_t src = { 0.0f, 0.0f, 64.0f, 64.0f };
                jrect_t dst = { m->x, m->y, m->w, m->h };
                image_draw1(tex, &src, &dst);
        }

        // 圆心汉字：仅通用圆钮（攻/射/线）保留，用墨黑色。
        // Style D 圆钮为黑色描边、中心镂空，黑字落在镂空处透出的浅色地板上，清晰可读；
        // 方向键由素材自带箭头表达，不再叠字。
        if (!isDpad && m->label && m->label[0]) {
                font_p f = AssetManager::GetInstance()->GetSimheiFont24();
                jcolor_t tc = { 20, 20, 25, 255 };
                jtext_printx(r2d, f, m->label, (int)strlen(m->label), tc,
                        cx - 12.0f, cy - 12.0f);
        }

        // 冷却环：比最外轮廓略大一圈（深墨）
        if (m->coolType != 0) {
                float ratio = localCoolRatio(m->coolType);
                if (ratio > 0.0f)
                        drawCoolRing(cx, cy, out + 1.0f, 4.0f, 1.0f - ratio);
        }
}

void GameplayControlsLayer::sendGameplayControlEvent(int event_code)
{
        push_user_event(GAMEPLAY_CONTROL_EVENT, event_code);
}

GameplayControlsLayer::GameplayControlsLayer(Context* ctx) : UILayer(ctx)
{
        load_controls();
}

GameplayControlsLayer::~GameplayControlsLayer() = default;

void GameplayControlsLayer::add_button(button_p* slot, gc_button_meta* meta,
                float cx, float cy, float wf, float hf,
                const char* label, int coolType)
{
        button_p b = button_create(ctx_->r2d, cx, cy, wf, hf);
        UIStyle::applyInkButton(b);
        *slot = b;
        int lw = 0, lh = 0;
        r2d_get_logical_presentation(ctx_->r2d, &lw, &lh, NULL);
        float w = wf * (float)lw, h = hf * (float)lh;
        float x = (cx - wf * 0.5f) * (float)lw, y = (cy - hf * 0.5f) * (float)lh;
        *meta = { b, ctx_, w, h, x, y, label, coolType };
}

void GameplayControlsLayer::load_controls()
{
        // 左下左摇杆：同时作为移动方向与瞄准方向（移动端无鼠标时的瞄准来源）。
        // 注意：joystick_create 的 cx/cy/radius 均为“逻辑分辨率的比例(0~1)”，内部会乘以宽高，
        //       不是像素坐标，传像素值会让中心跑到屏幕外导致摇杆不可见。
        // cy/radius 取值保证整个基底圆(及满推时的摇杆头)都落在 480x320 窗口内，不裁边。
        // 纹理由 joystick_set_textures 提供（无纹理则退回纯绘制）。
        move_stick = joystick_create(ctx_->r2d, 0.17f, 0.70f, 0.15f);
        joystick_set_textures(move_stick,
                "joy_topdown_template/sprites/ui/joystick_base.png",
                "joy_topdown_template/sprites/ui/joystick_nub.png");
        joystick_set_dir_mode(move_stick, JOYSTICK_DIR_4WAY);   // 限制为 上/下/左/右 四向

        // 右下攻击键：通用圆钮 + 圆心墨黑「攻」字 + 冷却环（保持原样）。
        add_button(&btn_attack, &attack_meta, 0.92500f, 0.89375f, 0.13333f, 0.20000f, "攻", 1);
}

void GameplayControlsLayer::HandleEvent(const void* ev)
{
        jevent_callbacks_t cbs = {};
        cbs.on_mousepressed = [](float x, float y, int btn, void* arg) {
                (void)btn;
                auto* self = (GameplayControlsLayer*)arg;
                // 攻击键优先（右下圆钮）
                if (self->btn_attack && button_is_pressed(self->btn_attack, x, y)) {
                        self->pressed_btn = self->btn_attack;
                        self->pressed_code = GAMEPLAY_CONTROL_ATTACK;
                        return;
                }
                // 否则交给左摇杆（仅在半径内激活）
                joystick_touch_down(self->move_stick, x, y);
                self->stick_active = joystick_is_dragging(self->move_stick) ? 1 : 0;
                if (self->stick_active) {
                        self->ctx_->mouse_valid = false;   // 拖动摇杆时让 aim 走摇杆而非鼠标
                        self->updateStick();
                }
        };
        cbs.on_mousereleased = [](float x, float y, int btn, void* arg) {
                (void)btn;
                auto* self = (GameplayControlsLayer*)arg;
                if (self->pressed_btn == self->btn_attack) {
                        button_p b = self->pressed_btn;
                        self->pressed_btn = NULL;
                        self->pressed_code = 0;
                        button_set_pressed(b, false);
                        if (!button_is_hovered(b, x, y)) return;
                        if (self->localCoolRatio(1) > 0.0f) return;
                        GameplayControlsLayer::sendGameplayControlEvent(GAMEPLAY_CONTROL_ATTACK);
                        return;
                }
                if (self->stick_active) {
                        joystick_touch_up(self->move_stick);
                        self->stick_active = 0;
                        // 清除移动方向位（保留瞄准角锁存，仍可朝该方向开火）
                        if (self->last_set_ev) {
                                GameplayControlsLayer::sendGameplayControlEvent(self->last_rel_ev);
                                self->last_set_ev = 0; self->last_rel_ev = 0;
                        }
                }
        };
        cbs.on_mousemoved = [](float x, float y, float dx, float dy, void* arg) {
                (void)dx; (void)dy;
                auto* self = (GameplayControlsLayer*)arg;
                if (self->stick_active) {
                        joystick_touch_move(self->move_stick, x, y);
                        self->updateStick();
                        self->ctx_->mouse_valid = false;   // 拖动摇杆时瞄准走摇杆而非鼠标
                        return;
                }
                // 非摇杆拖动：桌面端用鼠标瞄准；移动端(MOBILE_TOUCH)不污染瞄准
#ifndef MOBILE_TOUCH
                self->ctx_->mouse_logical = r2d_coordinates_from_window(self->ctx_->r2d, x, y);
                self->ctx_->mouse_valid   = true;
#endif
        };
        dispatch_event(cbs, ev, this);
}

void GameplayControlsLayer::Destroy()
{
        if (attack_meta.btn) { button_destroy(attack_meta.btn); attack_meta.btn = NULL; }
        btn_attack = NULL;
        if (move_stick) { joystick_destroy(move_stick); move_stick = NULL; }
        pressed_btn = NULL;
        pressed_code = 0;
        stick_active = 0; last_set_ev = 0; last_rel_ev = 0;
}

// 把左摇杆向量转成：
//   ① 移动方向位（4 向，经 GAMEPLAY_CONTROL 事件进入 current_input_mask）
//   ② 瞄准角锁存（量化 0..254，写入 ctx->aim_stick_yaw_q，供 collect_aim_yaw_q 在鼠标无效时回落）
// 松手回中（mag<=deadzone）时只清除移动位，瞄准角保留（仍可朝最后方向开火）。
void GameplayControlsLayer::updateStick()
{
        if (!move_stick || !ctx_) return;
        vec2_t dir = joystick_get_direction(move_stick);
        float mag = joystick_get_magnitude(move_stick);
        if (mag > 0.25f) {
                int set_ev = 0, rel_ev = 0;
                if (fabsf(dir.x) >= fabsf(dir.y)) {
                        if (dir.x > 0.0f) { set_ev = GAMEPLAY_CONTROL_RIGHT; rel_ev = GAMEPLAY_CONTROL_RELEASE_RIGHT; }
                        else               { set_ev = GAMEPLAY_CONTROL_LEFT;  rel_ev = GAMEPLAY_CONTROL_RELEASE_LEFT;  }
                } else {
                        if (dir.y > 0.0f) { set_ev = GAMEPLAY_CONTROL_DOWN;  rel_ev = GAMEPLAY_CONTROL_RELEASE_DOWN;  }
                        else               { set_ev = GAMEPLAY_CONTROL_UP;    rel_ev = GAMEPLAY_CONTROL_RELEASE_UP;    }
                }
                // 瞄准角：与鼠标同约定（屏幕 y 向下，atan2(dy,dx)）
                float deg = atan2f(dir.y, dir.x) * 180.0f / 3.1415926f;
                ctx_->aim_stick_yaw_q = aim_yaw_quantize(deg);
                if (set_ev != last_set_ev) {
                        if (last_set_ev) sendGameplayControlEvent(last_rel_ev);
                        sendGameplayControlEvent(set_ev);
                        last_set_ev = set_ev; last_rel_ev = rel_ev;
                }
        } else {
                if (last_set_ev) { sendGameplayControlEvent(last_rel_ev); last_set_ev = 0; last_rel_ev = 0; }
                // 保留 aim_stick_yaw_q 锁存
        }
}

void GameplayControlsLayer::Render()
{
        if (!ctx_) return;
        drawButton(&attack_meta);
        if (move_stick) joystick_draw(move_stick);
}
