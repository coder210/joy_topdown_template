#ifndef NET_TRANSPORT_JOY_TRANSPORT_H
#define NET_TRANSPORT_JOY_TRANSPORT_H

#include <joy/joy.h>
#include "i_transport.h"

// joy 引擎传输实现：包装 netclient_* / netserver_*（WebSocket）。
// 拥有底层句柄：析构时自动 destroy。
class JoyTransport : public ITransport {
public:
    JoyTransport() = default;
    ~JoyTransport() override;

    bool StartServer(const char* ip, int port) override;
    bool StartClient(const char* host, int port) override;
    bool IsServer() const override { return server_ != nullptr; }
    bool IsClient() const override { return client_ != nullptr; }

    void Update() override;
    bool Poll(NetTransportEvent& ev) override;
    void Release(NetTransportEvent& ev) override;

    void SendTo(int conv, const char* data, int len) override;
    void Broadcast(const char* data, int len) override;
    void Send(const char* data, int len) override;

    int  ConnectionCount() const override;

private:
    void ReleasePending();

    netclient_p   client_ = nullptr;
    netserver_p   server_ = nullptr;
    net_message_t msg_{};          // 最近一次 Poll 取到的消息（Release 时释放）
    bool          msg_pending_ = false;
};

#endif
