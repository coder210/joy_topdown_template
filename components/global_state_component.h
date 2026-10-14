#ifndef GLOBAL_STATE_COMPONENT_H
#define GLOBAL_STATE_COMPONENT_H
#include <joy/jmath.h>
#include <joy/joy.h>

// 全局单例状态（通过 registry.ctx().get<GlobalState>() 访问）
struct GlobalState {
    // 帧同步
    int next_entity_id = 1;
    int next_frame_id  = 1;       // 逻辑帧序号：每个固定步长 tick +1（两端确定性一致）

    // 实际逻辑帧率（每秒统计一次，仅用于调试显示）
    int logic_fps = 0;

    // 摄像机
    vec2_t camera_position = { 0.0f, 0.0f };
    int camera_follow_conv = -1;  // 要跟随的玩家 conv，-1 表示不跟随

    // 横版清关：把世界 Y 锁到屏幕垂直中心（<0 = 不锁，随玩家上下跟随，保持原俯视行为）
    float camera_lock_y = -1.0f;
    // 横版清关：水平前视偏置（世界单位，正值 = 多看前方、角色偏屏幕左侧）
    float camera_bias_x = 0.0f;
    // 横版清关：side_view 开启后相机 Y 自动锁到走廊中线（首次跟随时取玩家 Y），不再上下滚
    bool side_view = false;

    // 确定性随机数（帧同步用）：seed 由服务端生成并通过 loading 协议下发，
    // 两端以同一 seed 创建同一序列，保证 AI/巡逻等行为完全一致。
    // 用单状态 LCG（lcg_random_*）：状态仅 1 个 uint64，loading 同步量最小。
    int random_seed = 0;
    lcg_num_p random = NULL;      // lcg_random_create(seed) 创建，帧同步期间共享推进

    // 选关地图与通关判定（与 3D 的 game_scene 业务逻辑对齐）
    int level = 1;                // 当前选关索引（来自 ctx->selected_level）
    bool level_cleared = false;   // 本关敌人是否已全部清空（给定性推导，双端一致）

    // 敌人补充用的出生点轮转游标（仅权威端推进；客户端不运行 enemy_maintain_system，
    // 由快照接收新敌人，故该值无需跨端同步）。初始 0。
    int next_enemy_spawn_idx = 0;

    // A* 寻路工作区（存放 pathfind_astar 搜索过程的临时缓冲，含地图可通行性缓存）。
    // 仅作临时计算缓冲，不是帧同步状态、不参与 checksum，不随网络序列化。
    // 放 GlobalState 全局单例（随 ECS ctx 一起 emplace/get），避免函数内 static 数组的
    // 跨编译单元/可重入隐患。MAX_CELLS = 30x20 地图格数。
    struct AstarWork {
        static const int MAX_CELLS = 4096;  // MAP_W * MAP_H (64x64)

        int   gscore[MAX_CELLS];
        int   fscore[MAX_CELLS];
        int   came[MAX_CELLS];
        bool  closed[MAX_CELLS];
        bool  in_open[MAX_CELLS];
        int   heap[MAX_CELLS];              // 二叉堆：按 fscore 最小堆
        bool  walkable[MAX_CELLS];          // 地图可通行性缓存（避免每格重复查表）
        bool  walkable_init = false;
    } astar_work;
};

#endif
