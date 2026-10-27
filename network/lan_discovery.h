#ifndef LAN_DISCOVERY_H
#define LAN_DISCOVERY_H

#include <cstdio>
#include <cstring>
#include <vector>
#include <joy/joy.h>
#include "room_info.h"

// 局域网房间发现（UDP 广播），IRoomSource 的 LAN 实现。
//
// 协议：房主每 kRoomBeaconIntervalMs 向 255.255.255.255:kRoomBeaconPort 广播一行文本：
//   "JOYROOM|版本|游戏端口|人数|上限|地图|房间名"
// 客户端绑定 kRoomBeaconPort 收包，用 sys_recvfrom 返回的源 IP 作为房间 IP，
// 以 ip:port 去重，kRoomExpireMs 内无心跳则移除。
//
// 全部走 joy 的 sys_* socket（Windows / Linux / Android 可用；macOS 引擎本身不支持 socket）。
// 单例：同一进程内房主广播与客户端监听可同时存在（不同 socket）。
class LanDiscovery : public IRoomSource {
public:
    static LanDiscovery& Get() {
        static LanDiscovery inst;
        return inst;
    }

    // ---------------- 房主侧 ----------------
    bool start_beacon(const char* name, int game_port, int map_id, int max_players) {
        stop_beacon();
        beacon_fd_ = sys_udp();
        if (beacon_fd_ < 0) return false;
        // 非阻塞：广播 sendto 在部分系统/防火墙/无广播路由时会阻塞若干秒，
        // 若放在游戏线程里会周期性地卡顿（每秒一次）。这里务必设为非阻塞。
        sys_set_sock_accpettimeo(beacon_fd_, 1);
        sys_set_sock_broadcast(beacon_fd_);
        beacon_room_ = RoomInfo();
        beacon_room_.port = game_port;
        beacon_room_.players = 1;
        beacon_room_.max_players = max_players;
        beacon_room_.map_id = map_id;
        beacon_room_.name = name ? name : "";
        last_beacon_ms_ = 0;   // 下一帧立即发一次
        return true;
    }
    void set_beacon_players(int n) { beacon_room_.players = n; }
    void stop_beacon() {
        if (beacon_fd_ >= 0) { sys_closesocket(beacon_fd_); beacon_fd_ = -1; }
    }
    bool beacon_active() const { return beacon_fd_ >= 0; }

    // ---------------- 客户端侧 ----------------
    bool start_listen() {
        if (listen_fd_ >= 0) return true;
        listen_fd_ = sys_udp();
        if (listen_fd_ < 0) return false;
        sys_set_sock_reuseaddr(listen_fd_);
        if (!sys_bind(listen_fd_, "0.0.0.0", kRoomBeaconPort)) {
            sys_closesocket(listen_fd_);
            listen_fd_ = -1;
            return false;
        }
        // 双保险：非阻塞 + 1ms 收超时。某些平台 ioctlsocket(FIONBIO) 失败时
        // 仍会阻塞，导致大厅/主循环卡死；rcvtimeo 保证 recvfrom 一定会很快返回。
        sys_set_sock_accpettimeo(listen_fd_, 1);
        sys_set_sock_rcvtimeo(listen_fd_, 1);
        return true;
    }
    void stop_listen() {
        if (listen_fd_ >= 0) { sys_closesocket(listen_fd_); listen_fd_ = -1; }
    }
    bool listening() const { return listen_fd_ >= 0; }

    // ---------------- IRoomSource ----------------
    void start() override { start_listen(); }
    void stop() override { stop_listen(); }
    void update() override { tick(); }
    const std::vector<RoomInfo>& rooms() const override { return rooms_; }

private:
    LanDiscovery() {}

    void tick() {
        // 用 SDL_GetTicks（单调、便宜）：sys_current_time() 在 Windows 走
        // GetLocalTime + mktime，既非单调又有额外开销，曾在每帧调用时拖慢主循环。
        const int64_t now = (int64_t)get_ticks();

        // 房主：周期广播
        if (beacon_fd_ >= 0) {
            if (last_beacon_ms_ == 0 || now - last_beacon_ms_ >= kRoomBeaconIntervalMs) {
                char msg[256];
                int n = snprintf(msg, sizeof(msg), "JOYROOM|%d|%d|%d|%d|%d|%s",
                                 kRoomBeaconMagic, beacon_room_.port,
                                 beacon_room_.players, beacon_room_.max_players,
                                 beacon_room_.map_id, beacon_room_.name.c_str());
                if (n > 0) sys_sendto(beacon_fd_, msg, n, "255.255.255.255", kRoomBeaconPort);
                last_beacon_ms_ = now;
            }
        }

        // 客户端：非阻塞收包 + 过期清理
        if (listen_fd_ >= 0) {
            // 每帧最多处理 32 个包，避免一次涌入大量广播时长时间占用主线程
            for (int guard = 0; guard < 32; ++guard) {
                char buf[JOY_MAX_BUFFER + 1];
                char ip[JOY_MAX_IP];
                int port = 0;
                int n = sys_recvfrom(listen_fd_, buf, ip, &port);
                if (n <= 0) break;
                if (n > JOY_MAX_BUFFER) n = JOY_MAX_BUFFER;
                buf[n] = 0;
                merge(buf, ip, now);
            }
            for (size_t i = 0; i < rooms_.size(); ) {
                if (now - rooms_[i].last_seen_ms > kRoomExpireMs)
                    rooms_.erase(rooms_.begin() + i);
                else
                    ++i;
            }
        }
    }

    void merge(const char* buf, const char* ip, int64_t now) {
        int ver = 0, gport = 0, players = 0, mx = 0, map_id = 0;
        char name[128] = { 0 };
        int got = sscanf(buf, "JOYROOM|%d|%d|%d|%d|%d|%127[^\n]",
                         &ver, &gport, &players, &mx, &map_id, name);
        if (got < 5 || ver != kRoomBeaconMagic || gport <= 0) return;

        for (size_t i = 0; i < rooms_.size(); ++i) {
            if (rooms_[i].ip == ip && rooms_[i].port == gport) {
                rooms_[i].players = players;
                rooms_[i].max_players = mx;
                rooms_[i].map_id = map_id;
                rooms_[i].name = name;
                rooms_[i].last_seen_ms = now;
                return;
            }
        }
        RoomInfo r;
        r.ip = ip;
        r.port = gport;
        r.players = players;
        r.max_players = mx;
        r.map_id = map_id;
        r.name = name;
        r.last_seen_ms = now;
        rooms_.push_back(r);
    }

    int64_t  beacon_fd_ = -1;
    int64_t  listen_fd_ = -1;
    int64_t  last_beacon_ms_ = 0;
    RoomInfo beacon_room_;
    std::vector<RoomInfo> rooms_;
};

#endif // LAN_DISCOVERY_H
