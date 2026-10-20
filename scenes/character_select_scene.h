#ifndef CHARACTER_SELECT_SCENE_H
#define CHARACTER_SELECT_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"
#include "../systems/character_catalog.h"

// 角色选择场景：左侧角色列表 + 右侧大立绘/属性（与选关页同一套 Master-Detail 布局）
class CharacterSelectScene : public Scene {
public:
    explicit CharacterSelectScene(Context* ctx, GameModeKind mode);
    void Load() override;
    void Render() override;
    void Destroy() override;
    void HandleUiEvent(const void* e) override;
private:
    GameModeKind target_mode = MODE_SINGLE;
    button_p btn_cards[MAX_CHARACTERS];   // 实际数量由 g_characterCatalog.size() 决定
    button_p btn_confirm = NULL, btn_back = NULL;
    inputbox_p nickname_input = NULL;
    int selected = 0;
    int hovered_ = -1;                    // 鼠标悬停的角色行
    // 区分“点击行”与“滑动列表”：按下只记录，松手且仍在同一行才算点击
    int   press_row_ = -1;
    float press_x_ = 0.0f, press_y_ = 0.0f;

    // 左侧角色列表的纵向滚动
    scrollview_p card_view = NULL;
    void apply_card_scroll();             // 按 scrollview 偏移刷新行按钮位置
    void ensure_selected_visible();       // 键盘/初始选中时把行滚入视口

    void draw_card(int i);                            // 左侧列表行
    void draw_preview_panel(Context* ctx, int index); // 右侧大立绘 + 属性
    void go_to_level();
    void go_to_loading();
    void go_back();
};

#endif
