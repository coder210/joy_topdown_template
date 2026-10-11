#ifndef SETTINGS_H
#define SETTINGS_H

// 用户设置（自动保存到 settings.cfg，切换模式/进出场景时保留）
struct Settings {
        char  server_ip[64] = "192.168.2.28";
        int   server_port = 30000;
        float bgm_volume = 0.5f;   // 背景音乐音量 0~1
        float sfx_volume = 1.0f;   // 音效音量 0~1
        bool  fullscreen = false;
        bool  show_collision = false;   // 是否显示测试碰撞框（渲染辅助，不进帧同步）
};

void settings_save(const Settings* s);
void settings_load(Settings* s);

#endif
