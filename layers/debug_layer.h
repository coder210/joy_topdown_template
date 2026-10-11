#ifndef DEBUG_LAYER_H
#define DEBUG_LAYER_H
#include "layer.h"

// 纯 C++ 类实现的调试 overlay（继承 UILayer，不再依赖 joy scene_node）
class DebugLayer : public UILayer {
public:
    explicit DebugLayer(Context* ctx);
    ~DebugLayer() override;

    void Render() override;
    void Destroy() override;

private:
    // 原 free 函数 draw_debug，改为成员以直接访问 ctx_
    void drawDebug();
};

#endif
