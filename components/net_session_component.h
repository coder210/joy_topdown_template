#ifndef NET_SESSION_COMPONENT_H
#define NET_SESSION_COMPONENT_H
#include <vector>
#include <map>
#include <string>
#include <cstdint>
#include <joy/joy.h>
#include "../protocol.h"
#include "../systems/state_checksum.h"

// 历史状态快照（用于预测回滚）。拆移自 context.h，供 NetSession 与 network_utils 使用。
struct state_snapshot {
    fp_t position_x, position_y;
};

// 网络会话态（game 场景运行时）。
// 拆自原 context.h 的 context_net_state。存为 registry.ctx() 上下文组件（同 GlobalState 模式），
// 经 net_session(ctx) 读取。只有 game 场景（Host/Server/连机客户端）使用，UI 场景不接触。
struct NetSession {
    uint32_t local_conv = 0;    // 本地玩家的 conv（连接标识）

    // 服务端 / Host 字段（命令收集与转发）
    std::vector<S2CPlayerJoin>  player_joins;
    std::vector<S2CPlayerLeave> player_leaves;
    std::vector<S2CPlayerInput> player_inputs;
    std::map<int, std::string> commands;        // 以帧号为键的命令帧缓冲（补发用）
    std::map<int, std::string> world_snapshots; // 以帧号为键的世界快照（中途加入用）

    // 预测回滚
    std::map<int, std::map<int, state_snapshot>> snapshots; // 按 frame_id 递增存储的历史快照
    std::vector<int> pending_inputs;            // 已发送但未确认的输入

    // 帧同步校验记录缓冲（性能优化：不实时打印，退出时落盘）
    std::vector<ChecksumRecord> checksum_records;

    // 世界快照暂存缓冲（拆自原 Context::entities，仅 snapshot_system 打包时使用）
    S2CEntity entities[512];
};
#endif