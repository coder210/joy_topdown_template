#ifndef LOADING_SCENE_H
#define LOADING_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

// 加载场景：继承 Scene 基类，load/update/render/destroy 为 scene 级回调对应虚函数。
class LoadingScene : public Scene {
public:
    explicit LoadingScene(Context* ctx, GameModeKind mode);
    void Load() override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;
private:
    GameModeKind target_mode = MODE_SINGLE;
    std::unique_ptr<Scene> target_scene;
    float       elapsed = 0;
    bool        loaded = false;
    int         dot_count = 0;
    float       dot_timer = 0;
};

#endif
