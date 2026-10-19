#ifndef UI_SFX_H
#define UI_SFX_H

#include <joy/joy.h>
#include "asset_manager.h"

// UI / 按钮音效（Kenney UI Pack，CC0）。
// 全局单例，**自带一个音频设备**：与游戏场景的 footstep / bgm 设备相互独立——
// 游戏场景 Destroy 时会关掉自己的设备，若 UI 共用该设备，退出游戏后菜单点击就没声了。
// 懒加载：首次播放时才开设备 + 载入三条 shot；资源缺失或开设备失败则静音（不反复重试）。
//
// 三条用途：
//   kClick  确认/导航类按钮（菜单、返回、选关、选角、结算）
//   kSwitch 开关类控件（设置页复选框）
//   kTap    游戏内操作（攻击键等轻反馈，比 click 更短促）
class UiSfx {
public:
        enum Kind { kClick = 0, kSwitch, kTap, kCount };

        static UiSfx& Get() { static UiSfx s; return s; }

        // 音量：一般取 Settings::sfx_volume，与游戏内音效同一档
        void setGain(float g) {
                gain_ = g;
                for (int i = 0; i < kCount; i++)
                        if (shots_[i]) audio_shot_set_gain(shots_[i], gain_);
        }

        void play(Kind k) {
                if (k < 0 || k >= kCount) return;
                if (!ensure()) return;
                if (shots_[k]) audio_shot_play(shots_[k]);
        }

        // 预热：应用启动时调用一次。提前打开音频设备 + 解码三条音效，
        // 避免“首次点击才懒开设备/解码”导致第一声延迟数秒（Android AAudio/HAL 冷启动）。
        void prewarm() { ensure(); }

        // 释放三条 shot 与设备（AppQuit 时调用；析构也兜底调一次）
        void shutdown() {
                for (int i = 0; i < kCount; i++) {
                        if (shots_[i]) { audio_shot_destroy(shots_[i]); shots_[i] = NULL; }
                }
                if (dev_) { audio_close_device(dev_); dev_ = 0; }
                inited_ = false;
        }

private:
        UiSfx() = default;
        ~UiSfx() { shutdown(); }
        UiSfx(const UiSfx&) = delete;
        UiSfx& operator=(const UiSfx&) = delete;

        static const char* pathFor(Kind k) {
                auto* am = AssetManager::GetInstance();
                const char* id = (k == kClick) ? "click" : (k == kSwitch) ? "switch" : "tap";
                const char* fallback = (k == kClick) ? "joy_topdown_template/audio/click-a.ogg"
                                     : (k == kSwitch) ? "joy_topdown_template/audio/switch-a.ogg"
                                                      : "joy_topdown_template/audio/tap-a.ogg";
                if (am) {
                        const char* p = am->GetResourcePath(AssetManager::kCatAudio, id);
                        if (p) return p;
                }
                return fallback;   // 索引未收录时用内置默认路径
        }

        bool ensure() {
                if (inited_) return dev_ != 0;
                inited_ = true;                       // 只尝试一次，失败即全程静音
                dev_ = audio_open_device();
                if (!dev_) { log_info("[ui_sfx] audio device unavailable, muted"); return false; }
                for (int i = 0; i < kCount; i++) {
                        const char* p = pathFor((Kind)i);
                        shots_[i] = audio_shot_create_ogg(p, dev_);
                        if (shots_[i]) audio_shot_set_gain(shots_[i], gain_);
                        else log_info("[ui_sfx] missing sound: %s", p);
                }
                return true;
        }

        uint32_t     dev_    = 0;
        float        gain_   = 1.0f;
        bool         inited_ = false;
        audio_shot_p shots_[kCount] = {};
};

// 按钮命中并播放音效：替代裸 button_is_pressed 调用，避免各场景漏加音效。
// 语义与 button_is_pressed 完全一致（命中返回 true），仅附带播一声。
static inline bool ui_button_hit(button_p b, float x, float y, UiSfx::Kind kind = UiSfx::kClick)
{
        if (!b || !button_is_pressed(b, x, y)) return false;
        UiSfx::Get().play(kind);
        return true;
}

#endif // UI_SFX_H
