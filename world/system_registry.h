#ifndef WORLD_SYSTEM_REGISTRY_H
#define WORLD_SYSTEM_REGISTRY_H
#include <vector>

// 世界级 ECS system 调度注册表（通过 registry.ctx().get<SystemRegistry>() 访问）。
// 注意：这是"每个世界一份的资源"，不是实体组件。
// game 场景每次进入都会 new 全新 registry 并重新 add_system，故无需手动清空。
struct SystemEntry;   // 完整定义见 context.h（含 Phase / std::function 签名）
struct SystemRegistry {
    std::vector<SystemEntry> systems;
};
#endif
