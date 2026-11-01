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

// 联机大厅（局域网房间浏览器）。
// 视觉与布局对齐「选择角色」场景：米色纸底 + 顶部墨色标题带 + 左列表 / 右详情 Master-Detail + 纸质按钮。
//   - 左侧：房间卡片列表（选中深框 + 红条）
//   - 右侧：选中房间详情
//   - 顶部右上角「直连」→ 居中弹窗输入 IP/端口
//   - 底部：创建房间 / 加入房间 / 刷新 / 返回
class OnlineScene : public Scene {
public:
    explicit OnlineScene(Context* ctx);
    void Load() override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;

private:
    button_p btn_create = NULL, btn_join = NULL, btn_refresh = NULL, btn_back = NULL;
    button_p btn_direct_open = NULL;                 // 标题带右上角「直连」
    button_p btn_connect = NULL, btn_cancel = NULL;  // 直连弹窗按钮
    inputbox_p ip_input = NULL;
    inputbox_p port_input = NULL;
    bool direct_open_ = false;                       // 直连弹窗是否打开
    int selected_ = -1;

    // 列表/面板几何（逻辑像素），按当前逻辑分辨率每帧重算
    float list_x = 0, list_y = 0, list_w = 0, list_h = 0;
    float row_x = 0, row_w = 0, row_top = 0, row_h = 0, row_pitch = 0;

    void layout();
    void join_selected();
    void direct_connect();
};

#endif
