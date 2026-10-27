#ifndef ONLINE_SCENE_H
#define ONLINE_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

#define ONLINE_EVENT (0x5)

enum online_event_code {
        ONLINE_EVENT_HOST = 1,     // 创建房间（→ MODE_HOST）
        ONLINE_EVENT_CLIENT = 2,   // 加入选中房间 / 直连（→ MODE_CLIENT）
        ONLINE_EVENT_SERVER = 3,   // 专用服务器（→ MODE_SERVER，保留，UI 不再暴露）
        ONLINE_EVENT_BACK = 4,
};

// 联机大厅（局域网房间浏览器），布局：顶部标题 + 全宽房间列表 + 底部操作条。
// 该结构单列、控件大，桌面与手机横屏都适用（触屏友好）。
//
// - 自动发现同网段房主（LanDiscovery，可插拔 IRoomSource，未来可换 Steam/主服务器）
// - 创建房间 / 加入选中 / 刷新 / 返回 + 直连 IP
// 选好房间后只写 settings.server_ip/port 并发 ONLINE_EVENT_CLIENT，
// 真正连接仍由 GameScene 按 MODE_CLIENT 建立（复用现有流程）。
class OnlineScene : public Scene {
public:
    explicit OnlineScene(Context* ctx);
    void Load() override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;

private:
    button_p btn_refresh = NULL, btn_create = NULL, btn_join = NULL,
             btn_back = NULL, btn_direct = NULL;
    inputbox_p ip_input = NULL;
    inputbox_p port_input = NULL;
    int selected_ = -1;

    // 房间列表面板几何（逻辑坐标），按当前逻辑分辨率每帧重算
    float list_x = 0, list_y = 0, list_w = 0, list_h = 0;
    float header_h = 0, row_h = 0;

    void layout();
    void join_selected();
    void direct_connect();
};

#endif
