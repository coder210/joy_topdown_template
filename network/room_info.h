#ifndef ROOM_INFO_H
#define ROOM_INFO_H

#include <string>
#include <vector>
#include <cstdint>

// ---------------- 局域网房间发现常量 ----------------
// 广播端口与游戏端口(默认 30000)分开，避免和 WebSocket 服务抢端口。
static constexpr int kRoomBeaconPort       = 30001;
static constexpr int kRoomBeaconMagic      = 1;      // 协议版本
static constexpr int kRoomBeaconIntervalMs = 1000;   // 房主广播周期
static constexpr int kRoomExpireMs         = 3500;   // 收不到心跳多久后从列表移除
static constexpr int kRoomMaxPlayers       = 8;

// 一个可加入的房间（一次发现到的联机实例）。
struct RoomInfo {
    std::string ip;                          // 房主 IP（接收端取广播源地址）
    int         port = 0;                    // 房主游戏端口（WebSocket）
    int         players = 0;                 // 当前人数
    int         max_players = kRoomMaxPlayers;
    int         map_id = 0;                  // 地图/关卡
    std::string name;                        // 房间名
    int64_t     last_seen_ms = 0;            // 最近一次收到心跳的时间
};

// 可插拔的“房间来源”接口：
//   LAN 广播 / Steam Lobby / 公网主服务器 都实现它，LobbyScene 只依赖此接口，
//   换后端（例如以后接 Steam）时不需要改 UI 与流程。
class IRoomSource {
public:
    virtual ~IRoomSource() {}
    virtual void start() = 0;                              // 开始发现
    virtual void stop() = 0;                               // 停止发现
    virtual void update() = 0;                             // 每帧推进（收包 / 心跳 / 过期）
    virtual const std::vector<RoomInfo>& rooms() const = 0; // 当前发现的房间
};

#endif // ROOM_INFO_H
