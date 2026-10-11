#ifndef NET_COMPONENT_H
#define NET_COMPONENT_H
#include "../protocol.h"

struct NetComponent {
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
