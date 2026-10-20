#include "pause_menu_layer.h"
#include <cstring>
#include "../ui_style.h"
#include "../ui_sfx.h"
#include "../asset_manager.h"
#include "../protocol.h"          // INPUT_*
#include "../settings.h"          // settings_save
#include "../scenes/main_scene.h"
#include "../scenes/scene_manager.h"

// 设计稿基准高度（与其它 UI 一致：480x320，按高度缩放）
static const float PAUSE_DESIGN_H = 320.0f;

static inline float pause_scale(int lh)
{
        return (lh > 0) ? ((float)lh / PAUSE_DESIGN_H) : 1.0f;
}

static jrect_t menu_panel_rect(int lw, int lh)
{
        const float s = pause_scale(lh);
        const float w = 230.0f * s, h = 268.0f * s;
        return { ((float)lw - w) * 0.5f, ((float)lh - h) * 0.5f, w, h };
}

static jrect_t confirm_panel_rect(int lw, int lh)
{
        const float s = pause_scale(lh);
        const float w = 250.0f * s, h = 140.0f * s;
        return { ((float)lw - w) * 0.5f, ((float)lh - h) * 0.5f, w, h };
}

static jrect_t settings_panel_rect(int lw, int lh)
{
        const float s = pause_scale(lh);
        const float w = 300.0f * s, h = 230.0f * s;   // 加高一点，容纳“调试信息”开关
        return { ((float)lw - w) * 0.5f, ((float)lh - h) * 0.5f, w, h };
}

// 宣纸面板：深墨描边 + 纸白填充
static void draw_panel(jr2d_p r2d, jrect_t r)
{
        r2d_set_color(r2d, 28, 28, 32, 255);
        jrect_t border = { r.x - 2.0f, r.y - 2.0f, r.w + 4.0f, r.h + 4.0f };
        shape_draw_rectangle(r2d, "fill", border);
        r2d_set_color(r2d, 248, 246, 240, 255);
        shape_draw_rectangle(r2d, "fill", r);
}

static void draw_text_centered(jr2d_p r2d, font_p f, const char* s,
                               jcolor_t c, float cx, float y)
{
        if (!f || !s) return;
        int w = 0, h = 0;
        if (!jtext_measurex(r2d, f, s, (int)strlen(s), c, &w, &h)) return;
        jtext_printx(r2d, f, s, (int)strlen(s), c, cx - (float)w * 0.5f, y);
}

PauseMenuLayer::PauseMenuLayer(Context* ctx) : UILayer(ctx)
{
        single_player_ = (ctx && ctx->mode == MODE_SINGLE);
        setupButtons();
}

PauseMenuLayer::~PauseMenuLayer()
{
        Destroy();
}

void PauseMenuLayer::setupButtons()
{
        if (!ctx_ || !ctx_->r2d) return;

        font_p f = AssetManager::GetInstance() ? AssetManager::GetInstance()->GetSimheiFont24() : NULL;
        jcolor_t black = { 0, 0, 0, 255 };
        jcolor_t danger = { 170, 40, 40, 255 };

        auto mk = [&](button_p* slot, const char* text, jcolor_t tc) {
                *slot = button_create(ctx_->r2d, 0.5f, 0.5f, 0.4f, 0.1f);
                if (!*slot) return;
                UIStyle::applyPaperButton(*slot);
                button_set_text_offset(*slot, 0, 2.0f);
                if (text && f) button_set_textx(*slot, f, text, (int)strlen(text), tc);
        };

        mk(&btn_pause_,       "||",         black);
        mk(&btn_resume_,      "继续",       black);
        mk(&btn_settings_,    "设置",       black);
        mk(&btn_mainmenu_,    "返回主菜单", black);
        mk(&btn_quit_,        "退出游戏",   danger);
        mk(&btn_cancel_,      "取消",       black);
        mk(&btn_ok_,          "确定",       black);
        mk(&btn_settings_back_,"返回",      black);
        mk(&btn_debug_, (ctx_ && ctx_->show_debug_hud) ? "调试信息：开" : "调试信息：关", black);

        bgm_slider_ = slider_create(ctx_->r2d, 0.3f, 0.4f, 0.4f, 0.05f,
                                    0.0f, 1.0f, ctx_->settings.bgm_volume);
        sfx_slider_ = slider_create(ctx_->r2d, 0.3f, 0.5f, 0.4f, 0.05f,
                                    0.0f, 1.0f, ctx_->settings.sfx_volume);
        if (bgm_slider_)
                slider_set_colors(bgm_slider_, { 200,200,200,255 }, { 50,50,60,255 },
                                  { 80,80,80,255 }, { 100,100,100,255 });
        if (sfx_slider_)
                slider_set_colors(sfx_slider_, { 200,200,200,255 }, { 50,50,60,255 },
                                  { 80,80,80,255 }, { 100,100,100,255 });

        layout_lw_ = 0;
        layout_lh_ = 0;
        syncLayout();
}

// 让“调试信息：开/关”与 ctx_->show_debug_hud 保持一致（F1 或本按钮都可能改动它）
void PauseMenuLayer::updateDebugButtonText()
{
        if (!btn_debug_ || !ctx_) return;
        font_p f = AssetManager::GetInstance() ? AssetManager::GetInstance()->GetSimheiFont24() : NULL;
        const char* txt = ctx_->show_debug_hud ? "调试信息：开" : "调试信息：关";
        button_set_textx(btn_debug_, f, txt, (int)strlen(txt), { 0, 0, 0, 255 });
}

void PauseMenuLayer::Destroy()
{
        auto d = [](button_p& b) { if (b) { button_destroy(b); b = NULL; } };
        d(btn_pause_);
        d(btn_resume_);
        d(btn_settings_);
        d(btn_mainmenu_);
        d(btn_quit_);
        d(btn_cancel_);
        d(btn_ok_);
        d(btn_settings_back_);
        d(btn_debug_);
        if (bgm_slider_) { slider_destroy(bgm_slider_); bgm_slider_ = NULL; }
        if (sfx_slider_) { slider_destroy(sfx_slider_); sfx_slider_ = NULL; }
        page_ = PAGE_NONE;
}

void PauseMenuLayer::syncLayout()
{
        if (!ctx_ || !ctx_->r2d) return;

        int lw = 0, lh = 0;
        r2d_get_logical_presentation(ctx_->r2d, &lw, &lh, NULL);
        if (lw <= 0 || lh <= 0) return;
        if (lw == layout_lw_ && lh == layout_lh_) return;
        layout_lw_ = lw;
        layout_lh_ = lh;

        const float s = pause_scale(lh);

        // 右上角暂停按钮
        if (btn_pause_) {
                const float sz = 36.0f * s;
                const float m  = 8.0f * s;
                button_set_position(btn_pause_, (float)lw - sz - m, m);
                button_set_size(btn_pause_, sz, sz);
        }

        // 主菜单面板
        {
                jrect_t p = menu_panel_rect(lw, lh);
                const float bw = 190.0f * s, bh = 40.0f * s, gap = 12.0f * s;
                const float bx = (float)lw * 0.5f - bw * 0.5f;
                float by = p.y + 66.0f * s;

                auto place = [&](button_p b) {
                        if (!b) return;
                        button_set_position(b, bx, by);
                        button_set_size(b, bw, bh);
                        by += bh + gap;
                };
                place(btn_resume_);
                place(btn_settings_);
                place(btn_mainmenu_);
                place(btn_quit_);
        }

        // 确认框按钮（左右并排）
        {
                jrect_t c = confirm_panel_rect(lw, lh);
                const float bw = 96.0f * s, bh = 36.0f * s;
                const float by = c.y + c.h - bh - 20.0f * s;
                if (btn_cancel_) {
                        button_set_position(btn_cancel_, c.x + 24.0f * s, by);
                        button_set_size(btn_cancel_, bw, bh);
                }
                if (btn_ok_) {
                        button_set_position(btn_ok_, c.x + c.w - bw - 24.0f * s, by);
                        button_set_size(btn_ok_, bw, bh);
                }
        }

        // 设置页「调试信息」开关 + 「返回」
        {
                jrect_t st = settings_panel_rect(lw, lh);
                const float bw = 100.0f * s, bh = 34.0f * s;
                if (btn_debug_) {
                        button_set_position(btn_debug_, st.x + 24.0f * s, st.y + 146.0f * s);
                        button_set_size(btn_debug_, st.w - 48.0f * s, 28.0f * s);
                }
                if (btn_settings_back_) {
                        button_set_position(btn_settings_back_,
                                st.x + (st.w - bw) * 0.5f, st.y + st.h - bh - 16.0f * s);
                        button_set_size(btn_settings_back_, bw, bh);
                }
        }

        layoutSliders();
}

void PauseMenuLayer::layoutSliders()
{
        if (layout_lw_ <= 0 || layout_lh_ <= 0) return;
        const float s = pause_scale(layout_lh_);
        jrect_t st = settings_panel_rect(layout_lw_, layout_lh_);
        const float x = st.x + 70.0f * s;
        const float w = st.w - 100.0f * s;
        const float h = 18.0f * s;
        if (bgm_slider_) slider_set_rect(bgm_slider_, x, st.y + 70.0f * s, w, h);
        if (sfx_slider_) slider_set_rect(sfx_slider_, x, st.y + 118.0f * s, w, h);
}

void PauseMenuLayer::clearGameInput()
{
        if (!ctx_) return;
        game_runtime(ctx_).current_input_mask &=
                ~(INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT | INPUT_ATTACK);
}

void PauseMenuLayer::openMenu()
{
        clearGameInput();
        page_ = PAGE_MENU;
}

void PauseMenuLayer::closeMenu()
{
        if (settings_dirty_ && ctx_) {
                settings_save(&ctx_->settings);
                settings_dirty_ = false;
        }
        page_ = PAGE_NONE;
}

void PauseMenuLayer::openSettings()
{
        page_ = PAGE_SETTINGS;
        if (ctx_) {
                if (bgm_slider_) slider_set_value(bgm_slider_, ctx_->settings.bgm_volume);
                if (sfx_slider_) slider_set_value(sfx_slider_, ctx_->settings.sfx_volume);
        }
        updateDebugButtonText();   // F1 可能已经改过状态，进入设置页时同步一下
        layoutSliders();
}

void PauseMenuLayer::showConfirm(ConfirmAction a)
{
        clearGameInput();
        confirm_action_ = a;
        page_ = PAGE_CONFIRM;
}

void PauseMenuLayer::applySettings()
{
        if (!ctx_) return;

        if (bgm_slider_) {
                float v = slider_get_value(bgm_slider_);
                if (v != ctx_->settings.bgm_volume) {
                        ctx_->settings.bgm_volume = v;
                        if (ctx_->bgm) audio_bgm_set_gain(ctx_->bgm, v);
                        settings_dirty_ = true;
                }
        }
        if (sfx_slider_) {
                float v = slider_get_value(sfx_slider_);
                if (v != ctx_->settings.sfx_volume) {
                        ctx_->settings.sfx_volume = v;
                        if (ctx_->step_sound) audio_shot_set_gain(ctx_->step_sound, v);
                        if (ctx_->bow_sound)  audio_shot_set_gain(ctx_->bow_sound, v);
                        UiSfx::Get().setGain(v);
                        settings_dirty_ = true;
                }
        }
}

void PauseMenuLayer::confirmOk()
{
        UiSfx::Get().play(UiSfx::kClick);

        if (confirm_action_ == CONFIRM_QUIT) {
                push_quit_event();
                return;
        }

        // 返回主菜单：延迟切换，避免在事件分发中析构当前场景
        if (settings_dirty_ && ctx_) {
                settings_save(&ctx_->settings);
                settings_dirty_ = false;
        }
        if (ctx_ && ctx_->scene_mgr) {
                ctx_->scene_mgr->requestTransition(new MainScene(ctx_));
        }
}

void PauseMenuLayer::updateHover(float x, float y)
{
        if (page_ == PAGE_MENU) {
                if (btn_resume_)   button_is_hovered(btn_resume_, x, y);
                if (btn_settings_) button_is_hovered(btn_settings_, x, y);
                if (btn_mainmenu_) button_is_hovered(btn_mainmenu_, x, y);
                if (btn_quit_)     button_is_hovered(btn_quit_, x, y);
        } else if (page_ == PAGE_CONFIRM) {
                if (btn_cancel_) button_is_hovered(btn_cancel_, x, y);
                if (btn_ok_)     button_is_hovered(btn_ok_, x, y);
        } else if (page_ == PAGE_SETTINGS) {
                if (btn_settings_back_) button_is_hovered(btn_settings_back_, x, y);
                if (btn_debug_)         button_is_hovered(btn_debug_, x, y);
                if (bgm_slider_) slider_is_hovered(bgm_slider_, x, y);
                if (sfx_slider_) slider_is_hovered(sfx_slider_, x, y);
        }
}

void PauseMenuLayer::releasePressed()
{
        auto r = [](button_p b) { if (b) button_set_pressed(b, false); };
        r(btn_pause_);
        r(btn_resume_);
        r(btn_settings_);
        r(btn_mainmenu_);
        r(btn_quit_);
        r(btn_cancel_);
        r(btn_ok_);
        r(btn_settings_back_);
        r(btn_debug_);
}

bool PauseMenuLayer::handle_event(const void* ev)
{
        if (!ev) return false;
        const SDL_Event* e = (const SDL_Event*)ev;

        // ESC / 安卓返回键：统一作为“返回”语义
        if (e->type == SDL_EVENT_KEY_DOWN &&
            (e->key.scancode == SDL_SCANCODE_ESCAPE ||
             e->key.scancode == SDL_SCANCODE_AC_BACK)) {
                if (page_ == PAGE_NONE) {
                        openMenu();
                        return true;
                }
                if (page_ == PAGE_SETTINGS || page_ == PAGE_CONFIRM) {
                        page_ = PAGE_MENU;
                        return true;
                }
                closeMenu();
                return true;
        }

        // 菜单关闭：只关心右上角暂停按钮，其余事件交给游戏
        if (page_ == PAGE_NONE) {
                if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == 1 &&
                    btn_pause_ && button_is_pressed(btn_pause_, e->button.x, e->button.y)) {
                        UiSfx::Get().play(UiSfx::kClick);
                        openMenu();
                        return true;
                }
                return false;
        }

        // ---- 以下为模态状态：所有事件都不再传给游戏 ----
        if (e->type == SDL_EVENT_MOUSE_MOTION) {
                updateHover(e->motion.x, e->motion.y);
                if (page_ == PAGE_SETTINGS) {
                        if (bgm_slider_) slider_process_event(bgm_slider_, ev);
                        if (sfx_slider_) slider_process_event(sfx_slider_, ev);
                        applySettings();
                }
                return true;
        }

        if (e->type == SDL_EVENT_MOUSE_BUTTON_UP) {
                releasePressed();
                if (page_ == PAGE_SETTINGS) {
                        if (bgm_slider_) slider_process_event(bgm_slider_, ev);
                        if (sfx_slider_) slider_process_event(sfx_slider_, ev);
                        applySettings();
                }
                return true;
        }

        if (e->type == SDL_EVENT_MOUSE_BUTTON_DOWN && e->button.button == 1) {
                const float x = e->button.x, y = e->button.y;

                if (page_ == PAGE_MENU) {
                        if (btn_resume_ && button_is_pressed(btn_resume_, x, y)) {
                                UiSfx::Get().play(UiSfx::kClick);
                                closeMenu();
                        } else if (btn_settings_ && button_is_pressed(btn_settings_, x, y)) {
                                UiSfx::Get().play(UiSfx::kClick);
                                openSettings();
                        } else if (btn_mainmenu_ && button_is_pressed(btn_mainmenu_, x, y)) {
                                UiSfx::Get().play(UiSfx::kClick);
                                showConfirm(CONFIRM_MAINMENU);
                        } else if (btn_quit_ && button_is_pressed(btn_quit_, x, y)) {
                                UiSfx::Get().play(UiSfx::kClick);
                                showConfirm(CONFIRM_QUIT);
                        }
                } else if (page_ == PAGE_CONFIRM) {
                        if (btn_cancel_ && button_is_pressed(btn_cancel_, x, y)) {
                                UiSfx::Get().play(UiSfx::kClick);
                                page_ = PAGE_MENU;
                        } else if (btn_ok_ && button_is_pressed(btn_ok_, x, y)) {
                                confirmOk();
                        }
                } else if (page_ == PAGE_SETTINGS) {
                        if (btn_settings_back_ && button_is_pressed(btn_settings_back_, x, y)) {
                                UiSfx::Get().play(UiSfx::kClick);
                                if (settings_dirty_) {
                                        settings_save(&ctx_->settings);
                                        settings_dirty_ = false;
                                }
                                page_ = PAGE_MENU;
                        } else if (btn_debug_ && button_is_pressed(btn_debug_, x, y)) {
                                // 手机端没有 F1：在这里切换调试 HUD
                                UiSfx::Get().play(UiSfx::kClick);
                                if (ctx_) ctx_->show_debug_hud = !ctx_->show_debug_hud;
                                updateDebugButtonText();
                        } else {
                                if (bgm_slider_) slider_process_event(bgm_slider_, ev);
                                if (sfx_slider_) slider_process_event(sfx_slider_, ev);
                                applySettings();
                        }
                }
                return true;
        }

        return true;   // 模态：吞掉其余事件
}

// UILayer 虚接口（GameScene 走 handle_event 的返回值版本；这里保持基类契约）
void PauseMenuLayer::HandleEvent(const void* ev)
{
        handle_event(ev);
}

void PauseMenuLayer::Render()
{
        if (!ctx_ || !ctx_->r2d) return;
        syncLayout();

        jr2d_p r2d = ctx_->r2d;
        const int lw = layout_lw_, lh = layout_lh_;
        if (lw <= 0 || lh <= 0) return;

        if (page_ == PAGE_NONE) {
                if (btn_pause_) button_draw(btn_pause_);
                return;
        }

        const float s = pause_scale(lh);
        font_p f = AssetManager::GetInstance() ? AssetManager::GetInstance()->GetSimheiFont24() : NULL;
        jcolor_t ink = { 28, 28, 32, 255 };
        jcolor_t sub = { 90, 90, 98, 255 };

        // 遮罩透明度：0 = 完全透明（可直接看到游戏场景内容）；想压暗背景就调大（如 60）。
        const int overlay_alpha = 0;
        if (overlay_alpha > 0) {
                r2d_set_color(r2d, 0, 0, 0, (uint8_t)overlay_alpha);
                jrect_t full = { 0.0f, 0.0f, (float)lw, (float)lh };
                shape_draw_rectangle(r2d, "fill", full);
        }

        if (page_ == PAGE_MENU) {
                jrect_t p = menu_panel_rect(lw, lh);
                draw_panel(r2d, p);
                draw_text_centered(r2d, f, "暂 停", ink, p.x + p.w * 0.5f, p.y + 18.0f * s);

                if (btn_resume_)   button_draw(btn_resume_);
                if (btn_settings_) button_draw(btn_settings_);
                if (btn_mainmenu_) button_draw(btn_mainmenu_);
                if (btn_quit_)     button_draw(btn_quit_);

                if (!single_player_) {
                        // 联机模式不暂停：提示对局仍在继续
                        draw_text_centered(r2d, f, "联机中：对局仍在继续", sub,
                                p.x + p.w * 0.5f, p.y + p.h - 26.0f * s);
                }
        } else if (page_ == PAGE_CONFIRM) {
                jrect_t c = confirm_panel_rect(lw, lh);
                draw_panel(r2d, c);

                const char* title = (confirm_action_ == CONFIRM_QUIT)
                        ? "确定退出游戏？" : "确定返回主菜单？";
                draw_text_centered(r2d, f, title, ink, c.x + c.w * 0.5f, c.y + 26.0f * s);
                draw_text_centered(r2d, f,
                        (confirm_action_ == CONFIRM_QUIT) ? "对局进度不会保存" : "当前对局将结束",
                        sub, c.x + c.w * 0.5f, c.y + 56.0f * s);

                if (btn_cancel_) button_draw(btn_cancel_);
                if (btn_ok_)     button_draw(btn_ok_);
        } else if (page_ == PAGE_SETTINGS) {
                jrect_t st = settings_panel_rect(lw, lh);
                draw_panel(r2d, st);
                draw_text_centered(r2d, f, "设 置", ink, st.x + st.w * 0.5f, st.y + 16.0f * s);

                // 标签（与滑块左对齐）
                jtext_printx(r2d, f, "音乐", (int)strlen("音乐"), ink,
                        st.x + 22.0f * s, st.y + 64.0f * s);
                jtext_printx(r2d, f, "音效", (int)strlen("音效"), ink,
                        st.x + 22.0f * s, st.y + 112.0f * s);

                if (bgm_slider_) slider_draw(bgm_slider_);
                if (sfx_slider_) slider_draw(sfx_slider_);
                if (btn_debug_) button_draw(btn_debug_);
                if (btn_settings_back_) button_draw(btn_settings_back_);
        }
}
