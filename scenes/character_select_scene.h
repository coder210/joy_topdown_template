#ifndef CHARACTER_SELECT_SCENE_H
#define CHARACTER_SELECT_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"
#include "../systems/character_catalog.h"

// 角色选择场景：继承 Scene 基类，事件由 SceneManager 直接分发到 HandleUiEvent
//（内部用 joy 的 dispatch_event 解析），不再依赖 joy 的 scene_node 桥接。
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

    // ---- 角色卡横向滚动（复用引擎 jui 的 scrollview 控件）----
    // scrollview 负责：拖拽/夹紧/裁剪/滚动条/点击与滑动区分；
    // 本场景只需按它的偏移搬动卡片按钮（保留模式控件存绝对位置，绘制与命中都基于它）。
    scrollview_p card_view = NULL;
    float card_base_x[MAX_CHARACTERS] = { 0 };  // 各卡在 scroll=0 时的顶边 x
    float card_base_y = 68.0f;                  // 卡片顶边 y
    void apply_card_scroll();                   // 按 scrollview 偏移刷新卡片按钮位置

    // 场景切换辅助（延迟登记过渡目标，经 ctx->scene_mgr->requestTransition 派发）
    void draw_card(int i);
    void go_to_level();
    void go_to_loading();
    void go_back();
};

#endif
