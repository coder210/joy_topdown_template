#include "scene_manager.h"
#include "context.h"
#include "scenes/scene.h"
#include "events.h"

// 接管 next 为当前场景：先清理旧场景资源，再加载新场景，最后删除旧对象
static void adopt(std::unique_ptr<Scene>& slot, Scene* next) {
    if (!next) return;
    if (slot) slot->Destroy();
    next->Load();
    slot.reset(next);
}

void SceneManager::push(Scene* next) {
    adopt(current_, next);
}

void SceneManager::change(Scene* next) {
    adopt(current_, next);
}

void SceneManager::setPending(Scene* next) {
    pending_.reset(next);
}

void SceneManager::requestTransition(Scene* next, int code) {
    if (!next) return;
    pending_.reset(next);
    push_user_event(TRANSITION_EVENT, code);
}

void SceneManager::processPending() {
    if (!pending_) return;
    // 先清理旧场景资源，再加载新场景，最后接管（删除旧对象）
    if (current_) current_->Destroy();
    pending_->Load();
    current_ = std::move(pending_);
}

void SceneManager::HandleEvent(const SDL_Event* e) {
    if (!current_ || !e) return;
    // 模拟原 joy 的双回调：先场景级事件，再 UI 级事件（HandleUiEvent 内部用 dispatch_event 解析）
    current_->HandleEvent((const void*)e);
    current_->HandleUiEvent((const void*)e);
}

void SceneManager::Update(float dt) {
    if (current_) current_->Update(dt);
}

void SceneManager::Render(jr2d_p r2d) {
    if (current_) current_->Render();
}

void SceneManager::reset() {
    // 优化：先 Destroy 释放场景资源（纹理/字体/音频等），再释放 unique_ptr，
    // 避免 R 键重建场景时旧场景资源悬空指针/泄漏（对齐 joy_side_template）。
    if (current_) {
        current_->Destroy();
        current_.reset();
    }
    pending_.reset();
}

const char* SceneManager::currentName() const {
    return current_ ? current_->name() : "";
}
