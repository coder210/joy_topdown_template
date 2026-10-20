#ifndef LEVEL_SELECT_SCENE_H
#define LEVEL_SELECT_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

// 关卡选择场景：继承 Scene 基类，事件由 SceneManager 直接分发到 HandleUiEvent
//（内部用 joy 的 dispatch_event 解析），不再依赖 joy 的 scene_node 桥接。
class LevelSelectScene : public Scene {
public:
    explicit LevelSelectScene(Context* ctx, GameModeKind mode);
    void Load() override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;
private:
    // 关卡槽位上限：与 LEVEL_META 条数解耦，加关卡只改 LEVEL_META 即可（不必再扩数组）
    static const int LEVEL_SLOT_MAX = 12;
    GameModeKind target_mode = MODE_SINGLE;
    button_p btn_levels[LEVEL_SLOT_MAX];
    button_p btn_confirm = NULL, btn_back = NULL;
    int selected = 0;
    int hovered_ = -1;                 // 鼠标悬停的卡片（自行跟踪，避免依赖按钮内部态）
    // 各关地图是否已制作（Tiled 导出的 levelN.lua 是否存在）；缺图的关卡标"制作中"且不可进入
    bool level_ready[LEVEL_SLOT_MAX] = { false };
    // "此关正在制作中" 提示（模态：显示期间消费所有事件，点击任意处关闭）
    bool tip_active = false;
    // 关卡列表纵向滚动（复用引擎 jui 的 scrollview）：关卡增多超出视口时可上下拖动
    scrollview_p level_view = NULL;
    float level_base_y[LEVEL_SLOT_MAX] = { 0 };   // 各关在 scroll=0 时的顶边 y
    // 每关真实网格的缓存（Load 时读取），用于卡片左侧迷你地图（零美术资源）
    char  preview_grid_[LEVEL_SLOT_MAX][4096];
    int   preview_w_[LEVEL_SLOT_MAX] = {0};
    int   preview_h_[LEVEL_SLOT_MAX] = {0};
    int   preview_player_[LEVEL_SLOT_MAX] = {-1};
    int   preview_enemy_[LEVEL_SLOT_MAX] = {0};
    bool  preview_has_[LEVEL_SLOT_MAX] = {false};
    void apply_level_scroll();    // 按 scrollview 偏移刷新关卡行按钮位置
    void draw_level(int i);       // 画左侧列表中的一行
    void draw_preview_panel(Context* ctx, int index);   // 右侧大缩略图 + 关卡信息
    void draw_tip();              // 画"此关正在制作中"模态提示
    void ensure_selected_visible();  // 键盘换关时把选中行滚入视口
    // 迷你地图（用该关真实网格绘制）/ 难度标记
    void draw_mini_map(Context* ctx, int i, float mx, float my, float mw, float mh, float s, bool ready);
    void draw_difficulty(Context* ctx, int i, float start_x, float y, float s);
};

#endif
