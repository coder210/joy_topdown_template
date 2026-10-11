#pragma once
#include <joy/joy.h>
#include "../context.h"

// UI 层基类（阶段 3 之后）：纯 C++ 类，由宿主 Scene 直接持有并桥接。
// 与 Scene 基类对称：Render 用 ctx_ 的 renderer，HandleEvent 接收 SDL 事件指针，
// Destroy 做资源清理（按钮等），析构时自动调用。宿主在切换时 delete 即可。
class UILayer {
public:
    explicit UILayer(Context* ctx) : ctx_(ctx) {}
    virtual ~UILayer() { Destroy(); }

    virtual void Render() {}
    virtual void HandleEvent(const void* ev) {}
    virtual void Destroy() {}

protected:
    Context* ctx_ = nullptr;
};
