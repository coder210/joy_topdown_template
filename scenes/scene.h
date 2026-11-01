#ifndef SCENE_H
#define SCENE_H

#include <joy/joy.h>
#include "../context.h"
#include "scene_manager.h"   // 供场景子类访问 SceneManager 类型（经 ctx->scene_mgr 调用 requestTransition/setPending）

// 场景基类（阶段 3）：不再持有 joy 的 scene_p / scene_node，完全由 C++ SceneManager
// 管理生命周期与事件分发。SceneManager 对每条事件双回调：先 HandleEvent（场景级），
// 再 HandleUiEvent（UI 级，内部用 joy 的 dispatch_event 解析鼠标/键盘并产生控件回调）。
//
// 生命周期：对象由 SceneManager 以 std::unique_ptr<Scene> 持有（current_ / pending_）。
// SceneManager 在切换时先调旧场景 Destroy() 做资源清理（按钮/网络等），再删除对象本体；
// 因此子类 Destroy() 只做资源清理，不要再 delete this，避免双重释放。
//
// UI 事件：所有 UI 控件层（menu_layer / gameplay_controls_layer / debug_layer）均为
// 纯 button / 直接绘制，彻底脱离 joy scene_node 系统；按钮命中由 button_is_pressed 独立完成。
// 宿主场景在 HandleUiEvent 里把事件转交对应 layer 的 handle_event / render 函数。
class Scene {
public:
    Scene(Context* ctx, const char* name);
    virtual ~Scene() = default;

    const char* name() const { return name_; }
    Context* ctx() const { return ctx_; }

    // 子类 override 这些（由 SceneManager 直接调用）
    virtual void Load() {}
    virtual void HandleEvent(const void* ev) {}   // 场景级事件（用户事件 / 键盘等），对应原 joy cb_event
    virtual void HandleUiEvent(const void* ev) {}  // UI 级事件（鼠标 / 控件输入），内部用 dispatch_event 解析
    virtual void Update(float dt) {}
    virtual void Render() {}
    virtual void Destroy() {}

protected:
    Context* ctx_;
    const char* name_;
};

#endif
