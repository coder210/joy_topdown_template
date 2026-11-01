#ifndef SPLASH_SCENE_H
#define SPLASH_SCENE_H
#include <joy/joy.h>
#include "../context.h"
#include "scene.h"

// 开场场景：继承 Scene 基类。
class SplashScene : public Scene {
public:
    explicit SplashScene(Context* ctx);
    void Load() override;
    void Update(float dt) override;
    void Render() override;
    void Destroy() override;
private:
    image_p logo = NULL;
    float   elapsed = 0;
    float   alpha = 0;          // 透明度：0→1（淡入）→1→0（淡出）
    bool    faded_in = false;
    bool    fading_out = false;
};

#endif
