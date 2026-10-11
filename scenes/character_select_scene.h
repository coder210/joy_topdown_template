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
    // 场景切换辅助（延迟登记过渡目标，经 ctx->scene_mgr->requestTransition 派发）
    void draw_card(int i);
    void go_to_level();
    void go_to_loading();
    void go_back();
};

#endif
