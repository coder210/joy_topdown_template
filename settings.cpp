#include <cstdio>
#include <cstdlib>
#include <cstring>
#include "settings.h"

// 保存设置到 settings.cfg（key=value 文本格式）
void settings_save(const Settings* s)
{
        FILE* fp = fopen("settings.cfg", "w");
        if (!fp) return;
        fprintf(fp, "server_ip=%s\n", s->server_ip);
        fprintf(fp, "server_port=%d\n", s->server_port);
        fprintf(fp, "bgm_volume=%f\n", s->bgm_volume);
        fprintf(fp, "sfx_volume=%f\n", s->sfx_volume);
        fprintf(fp, "fullscreen=%d\n", s->fullscreen ? 1 : 0);
        fprintf(fp, "show_collision=%d\n", s->show_collision ? 1 : 0);
        fclose(fp);
}

// 从 settings.cfg 加载设置（文件不存在时保留默认值）
void settings_load(Settings* s)
{
        FILE* fp = fopen("settings.cfg", "r");
        if (!fp) return;
        char key[64], value[128];
        // 格式串前导空格跳过行首换行/空白，否则第二行起 key 会带上 '\n' 导致匹配失败
        while (fscanf(fp, " %63[^=]=%127s", key, value) == 2) {
                if (strcmp(key, "server_ip") == 0) {
                        snprintf(s->server_ip, sizeof(s->server_ip), "%s", value);
                } else if (strcmp(key, "server_port") == 0) {
                        s->server_port = atoi(value);
                } else if (strcmp(key, "bgm_volume") == 0) {
                        s->bgm_volume = (float)atof(value);
                } else if (strcmp(key, "sfx_volume") == 0) {
                        s->sfx_volume = (float)atof(value);
                } else if (strcmp(key, "fullscreen") == 0) {
                        s->fullscreen = (atoi(value) != 0);
                } else if (strcmp(key, "show_collision") == 0) {
                        s->show_collision = (atoi(value) != 0);
                }
        }
        fclose(fp);
}
