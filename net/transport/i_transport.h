#ifndef NET_TRANSPORT_I_TRANSPORT_H
#define NET_TRANSPORT_I_TRANSPORT_H

// ============================================================
// 传输层抽象：把"网络怎么收发"与"帧同步怎么组织"解耦。
//
// 上层（帧同步 / NetworkManager）只依赖本接口，不出现 joy 的
// netclient_* / netserver_* / net_message_t 等具体类型；
// 具体实现见 joy_transport.h（包装 joy 现有 WebSocket 传输）。
// 未来换 TCP/KCP/WebRTC 只需新增一个实现，不改上层。
// ============================================================

// 统一事件。type 取值与 joy 的 net_event_type_k 对齐（1/2/3）。
struct NetTransportEvent {
    enum Type {
        Connected    = 1,
        Disconnected = 2,
        Message      = 3,
    };

    Type        type = Connected;
    int         conv = 0;
    const char* data = nullptr;   // 仅 Message 有效；在 Release() 之前有效
    int         len  = 0;
};

class ITransport {
public:
    virtual ~ITransport() = default;

    // ---- 生命周期 ----
    virtual bool StartServer(const char* ip, int port) = 0;
    virtual bool StartClient(const char* host, int port) = 0;
    virtual bool IsServer() const = 0;
    virtual bool IsClient() const = 0;

    // ---- 事件泵 ----
    virtual void Update() = 0;                          // 驱动底层网络泵
    virtual bool Poll(NetTransportEvent& ev) = 0;       // 取一条事件；false = 暂无
    virtual void Release(NetTransportEvent& ev) = 0;    // 释放消息体（Message 事件必须调用）

    // ---- 发送 ----
    virtual void SendTo(int conv, const char* data, int len) = 0;   // 服务端单发
    virtual void Broadcast(const char* data, int len) = 0;          // 服务端广播
    virtual void Send(const char* data, int len) = 0;               // 客户端发送

    // ---- 状态 ----
    virtual int  ConnectionCount() const = 0;            // 服务端已连接数
};

#endif
