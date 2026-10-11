#ifndef CONNECTION_COMPONENT_H
#define CONNECTION_COMPONENT_H
#include <cstdint>

// 挂载在玩家实体上的网络连接数据（用于断线重连和心跳检测）
struct ConnectionComponent {
    int conv = 0;                  // 连接标识
    int health = 10;               // 连接健康度（心跳超时用）
    uint64_t last_heartbeat = 0;   // 上次心跳时间戳
    uint32_t last_processed_seq = 0; // 最后已处理的序列号
    bool is_loaded = false;       // 是否已完成初始加载
    int frameid = 0;              // 客户端最后已接收的帧号
};

#endif
