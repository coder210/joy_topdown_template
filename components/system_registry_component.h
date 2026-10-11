#ifndef SYSTEM_REGISTRY_COMPONENT_H
#define SYSTEM_REGISTRY_COMPONENT_H
#include <vector>

// ECS system 调度注册表。拆自原 Context::systems。
// add_system/run_phase 仅持有 Context*、拿不到 game_scene，故存为 registry.ctx() 上下文组件
// （同 GlobalState 模式），经 system_registry(ctx) 读取。game 场景每次进入在 on_load 重新 add_system；
// 每次进入都 new 全新 registry，reset_context_for_mode 无需再手动清空。
struct SystemEntry;   // 完整定义见 context.h（含 Phase / std::function 签名）
struct SystemRegistry {
    std::vector<SystemEntry> systems;
};
#endif