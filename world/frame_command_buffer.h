#ifndef WORLD_FRAME_COMMAND_BUFFER_H
#define WORLD_FRAME_COMMAND_BUFFER_H
#include "../protocol.h"

// 当前逻辑帧待消费的命令缓冲（通过 registry.ctx().get<FrameCommandBuffer>() 访问）。
// 注意：这不是实体组件，而是"每 tick 覆盖一次"的世界级资源；
// 原名为 NetComponent，容易与实体组件混淆，故改名为语义明确的 FrameCommandBuffer。
struct FrameCommandBuffer {
    // 当前 tick 待处理的帧同步命令
    int frame_id = 0;
    int num_joins = 0;
    int num_leaves = 0;
    int num_inputs = 0;
    S2CPlayerJoin joins[32];
    S2CPlayerLeave leaves[32];
    S2CPlayerInput inputs[64];
};

#endif
