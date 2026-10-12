#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H
#include <string>
#include <vector>
#include <map>
#include <joy/joy.h>
#include "../context.h"
#include "../protocol.h"
// 注：下方组件/系统头仅为维持与原 network_utils.h 一致的 include 顺序，避免某些
// 组件头中定义的 `Entity` 别名与 entt::basic_registry 的默认模板参数冲突
// （snapshot_system.h 在 game_mode_common.cpp 中实例化时会触发该冲突）。
#include "components/global_state_component.h"
#include "components/action_state_component.h"
#include "components/ai_component.h"
#include "components/connection_component.h"
#include "components/enemy_component.h"
#include "components/health_component.h"
#include "components/id_component.h"
#include "components/logic_position_component.h"
#include "components/logic_rect_component.h"
#include "components/logic_velocity_component.h"
#include "components/player_component.h"
#include "components/spawn_point_component.h"
#include "components/transform_component.h"
#include "systems/spawn_def.h"
#include "systems/state_checksum.h"

// 网络收发与帧同步辅助：把原 network_utils.h 的 static 自由函数封装为有状态类，
// ctx 作为构造传入的成员（替代原先每个函数都带的 Context* ctx 隐含 self 参数）。
class NetworkManager {
public:
    explicit NetworkManager(Context* ctx) : ctx_(ctx) {}

    // 客户端（MODE_CLIENT）：连接 / 收 loading + 命令帧、发送本机输入
    void ClientReceive();
    void SendInput();

    // 服务端 / Host（MODE_SERVER / MODE_HOST）：收玩家命令、推进并广播命令帧
    void ServerReceive();
    void ServerTick();          // netserver_update + ServerReceive（原 server_network_tick）
    void CollectInput(int frame_id);
    void InputForwarding();

    // 单机 / Host 本地玩家辅助（MODE_SINGLE / MODE_HOST）
    void InjectLocalJoin();
    void CollectLocalInput();

private:
    Context* ctx_;

    void HandleLoading(int conv, int frame_id, int eid, const S2CEntity* entities, int ec);
    void HandleCmdLoading(int conv);
    void HandleCmdPlayerJoin(int conv, int64_t x, int64_t y, int char_id);
    void HandleCmdPlayerLeave(int conv);
    void HandleCmdPlayerInput(int conv, uint32_t seq, int dir, int attack);
    void HandleCmdHeartbeat(int conv);
    static int GetTargetFrameId(int curr_frameid, int context_frameid);
};

#endif
