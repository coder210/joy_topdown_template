#include "joy_transport.h"
#include <cstring>

JoyTransport::~JoyTransport()
{
    // 若上一条消息未释放，先释放，避免泄漏
    ReleasePending();
    if (client_) { joy_net_netclient_destroy(client_); client_ = nullptr; }
    if (server_) { joy_net_netserver_destroy(server_); server_ = nullptr; }
}

bool JoyTransport::StartServer(const char* ip, int port)
{
    server_ = joy_net_netserver_create(NET_SERVER_WEBSOCKET, ip, port);
    return server_ != nullptr;
}

bool JoyTransport::StartClient(const char* host, int port)
{
    client_ = joy_net_netclient_create(NET_CLIENT_WEBSOCKET, host, port);
    return client_ != nullptr;
}

void JoyTransport::Update()
{
    if (server_)      joy_net_netserver_update(server_);
    else if (client_) joy_net_netclient_update(client_);
}

void JoyTransport::ReleasePending()
{
    if (msg_pending_) {
        joy_net_netmessage_free_data(&msg_);
        msg_pending_ = false;
    }
}

bool JoyTransport::Poll(NetTransportEvent& ev)
{
    // 上一条若未显式 Release，这里兜底释放（正常调用方每条都会 Release）
    ReleasePending();

    memset(&msg_, 0, sizeof(msg_));

    bool ok = false;
    if (server_)      ok = joy_net_netserver_poll_message(server_, &msg_);
    else if (client_) ok = joy_net_netclient_poll_message(client_, &msg_);
    if (!ok) return false;

    ev.type = (NetTransportEvent::Type)msg_.type;
    ev.conv = msg_.conv;
    ev.data = msg_.data;
    ev.len  = msg_.len;

    msg_pending_ = (msg_.type == NET_TYPE_MESSAGE && msg_.data != nullptr);
    return true;
}

void JoyTransport::Release(NetTransportEvent& ev)
{
    ReleasePending();
    ev.data = nullptr;
    ev.len  = 0;
}

void JoyTransport::SendTo(int conv, const char* data, int len)
{
    if (server_) joy_net_netserver_send(server_, conv, data, len);
}

void JoyTransport::Broadcast(const char* data, int len)
{
    if (server_) joy_net_netserver_broadcast(server_, data, len);
}

void JoyTransport::Send(const char* data, int len)
{
    if (client_) joy_net_netclient_send(client_, data, len);
}

int JoyTransport::ConnectionCount() const
{
    return server_ ? joy_net_netserver_connection_count(server_) : 0;
}
