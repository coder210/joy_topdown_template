#include "network_manager.h"
#include <cstring>
#include <math.h>
#include <joy/joy.h>
#include "protocol.h"
#include "world/global_state.h"
#include "world/entity_factory.h"
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

// ============================================================
// 客户端网络（MODE_CLIENT）：连接服务端/Host，收 loading + 命令帧
// ============================================================

// ========== 加载数据处理 ==========
void NetworkManager::HandleLoading(int conv, int frame_id, int eid, const S2CEntity* entities, int ec)
{
        log_info("handle_loading");
        game_runtime(ctx_).ready = true;
        net_session(ctx_).local_conv = conv;
        ctx_->ecs.ctx().get<GlobalState>().camera_follow_conv = conv;
        game_runtime(ctx_).server_frameid = frame_id;
        // eid 即服务端 spawn 完敌人后的 next_entity_id（已跳过敌人占用的 id 区间），
        // 作为客户端 get_entity_id 的起点，保证玩家/子弹 id 与服务端对齐、不重叠。
        ctx_->ecs.ctx().get<GlobalState>().next_entity_id = eid;

        for (int i = 0; i < ec; i++) {
                auto& entity = entities[i];
                // 地图(S2C_TYPE_NORMAL)由客户端本地 map_spawn_system 生成，不通过网络同步
                if (entity.type == S2C_TYPE_NORMAL) continue;

                // 实体装配统一走 world/entity_factory.h（唯一构造入口）：
                // 快照恢复与正常 join / 敌人出生共用同一套组件字段，不再各自 emplace。
                if (entity.type == S2C_TYPE_PLAYER) {
                        spawn_player_from_snapshot(ctx_->ecs, ctx_,
                            entity.id, entity.player_conv, entity.player_char_id,
                            entity.position_x, entity.position_y,
                            entity.width, entity.height, (int)entity.hp);
                }
                else if (entity.type == S2C_TYPE_ENEMY) {
                        // 使用服务端打包的 ai_type 作为 SpawnDef 索引（按当前关卡 'E' 格确定性派生）；
                        // 索引非法时 def=nullptr，工厂退化为最小装配（与原行为一致）。
                        int si = (int)entity.ai_type;
                        int e_cnt = 0;
                        const SpawnDef* e_defs = level_enemy_defs(
                            ctx_->ecs.ctx().get<GlobalState>().level, e_cnt);
                        const SpawnDef* def = (si >= 0 && si < e_cnt) ? &e_defs[si] : nullptr;

                        spawn_enemy_from_snapshot(ctx_->ecs, ctx_,
                            entity.id, def, si,
                            entity.position_x, entity.position_y,
                            entity.width, entity.height, (int)entity.hp,
                            entity.ai_timer, entity.ai_state,
                            (fp_t)entity.ai_target_x, (fp_t)entity.ai_target_y);
                }
        }

        // 保存当前快照
        std::map<int, state_snapshot> snapshots;
        auto pv = ctx_->ecs.view<PlayerComponent, IdComponent, LogicRectComponent, LogicPositionComponent>();
        for (auto e : pv) {
                auto& id = pv.get<IdComponent>(e);
                auto& pos = pv.get<LogicPositionComponent>(e);
                snapshots.insert({ id.id, { pos.x, pos.y } });
        }
        net_session(ctx_).snapshots.insert({ game_runtime(ctx_).server_frameid, snapshots });

        // 告知服务器本地玩家加入（出生在当前关卡 'P' 格）
        fp_t sx_j, sy_j;
        map_player_spawn(ctx_->ecs.ctx().get<GlobalState>().level, &sx_j, &sy_j);
        std::string data = pack_c2s(CMD_PLAYER_JOIN, sx_j, sy_j, 0, 0, ctx_->selected_char_id);
        if (ctx_->transport) ctx_->transport->Send(data.c_str(), (int)data.size());
}

// ===== 本地瞄准角（量化后随输入上传，供所有端同步"武器指向"的表现）=====
// 只在本机计算并量化成整数，两端按同一个整数还原角度，故帧同步一致（浮点不跨端）。
// 来源优先级：键盘方向 > 摇杆（移动端兜底）。不再使用鼠标位置瞄准（取消"鼠标旋转"）。
// 键盘方向现按 dx/dy 合成向量经 atan2 求角，支持八向（含斜角），再 aim_yaw_quantize 量化成 0..254；
// 该 255 档量化逻辑保持不变——以后改回模拟输入（如摇杆全方向）即可恢复 255 档细分。
// 返回 0..254（有效角度）或 kAimYawInvalid（无输入方向 / 本地玩家不存在）。
static int collect_aim_yaw_q(Context* ctx)
{
    if (!ctx) return kAimYawInvalid;

    auto& rt = game_runtime(ctx);

    // 1) 键盘方向（八向）：按 dx/dy 符号合成方向向量，atan2 求角；斜向(如左上)即指向斜角，
    //    武器随之 8 向旋转。松手后锁存上一次方向（仍可向该方向开火）。
    int kb_mask = rt.current_input_mask & INPUT_DIR_MASK;
    if (has_dir(kb_mask)) {
        float dx = 0.0f, dy = 0.0f;
        if (kb_mask & INPUT_RIGHT) dx += 1.0f;
        if (kb_mask & INPUT_LEFT)  dx -= 1.0f;
        if (kb_mask & INPUT_DOWN)  dy += 1.0f;
        if (kb_mask & INPUT_UP)    dy -= 1.0f;
        // 屏幕 y 向下，与鼠标/摇杆同约定；dx/dy ∈ {-1,0,1}，斜向成 8 个方向
        float deg = atan2f(dy, dx) * 180.0f / 3.1415926f;
        rt.aim_keyboard_yaw_q = aim_yaw_quantize(deg);
        return rt.aim_keyboard_yaw_q;
    }
    if (rt.aim_keyboard_yaw_q >= 0) return rt.aim_keyboard_yaw_q;   // 锁存：停步后仍朝最后方向

    // 2) 移动端（无键盘）：用摇杆锁存的瞄准角
    if (rt.aim_stick_yaw_q >= 0) return rt.aim_stick_yaw_q;

    return kAimYawInvalid;
}

// ========== 发送输入（客户端）==========
void NetworkManager::SendInput()
{
    if (!ctx_->transport) return;
    int dir = game_runtime(ctx_).current_input_mask & INPUT_DIR_MASK;
    int attack = (game_runtime(ctx_).current_input_mask & INPUT_ATTACK) ? 1 : 0;
    if (game_runtime(ctx_).attack_triggered) { attack = 1; game_runtime(ctx_).attack_triggered = false; }
    // 瞄准角：只在变化时补发一帧（静止不动也要能更新朝向给别人看）
    const int yaw = collect_aim_yaw_q(ctx_);
    if (dir || attack || yaw != game_runtime(ctx_).last_aim_yaw_sent) {
        auto data = pack_c2s(CMD_PLAYER_INPUT, 0, 0, dir, attack, 0, yaw);
        ctx_->transport->Send(data.c_str(), (int)data.size());
        game_runtime(ctx_).last_aim_yaw_sent = yaw;
    }
}

// ========== 网络消息接收（客户端）==========
void NetworkManager::ClientReceive()
{
        if (!ctx_->transport) return;
        NetTransportEvent ev;
        if (ctx_->transport->Poll(ev)) {
            if (ev.type == NetTransportEvent::Connected) {
                auto d = pack_c2s(CMD_LOADING, 0, 0, 0, 0);
                ctx_->transport->Send(d.c_str(), (int)d.size());
            }
            else if (ev.type == NetTransportEvent::Disconnected) {
                log_info("disconnected=%d", ev.conv);
            }
            else if (ev.type == NetTransportEvent::Message) {
                jmsgpack_unpacked_t r;
                jmsgpack_unpacked_init(&r);
                size_t off = 0;
                int is_loading = 0;
                if (jmsgpack_unpack_next(&r, ev.data, ev.len, &off) >= 0) {
                    const jmsgpack_object_t* root = jmsgpack_unpacked_data(&r);
                    if (root && jmsgpack_object_type(root) == JMSGPACK_OBJECT_ARRAY
                            && jmsgpack_object_array_size(root) >= 2) {
                        const jmsgpack_object_t* first = jmsgpack_object_array_at(root, 0);
                        if (first && jmsgpack_object_type(first) == JMSGPACK_OBJECT_POSITIVE_INTEGER)
                            is_loading = ((int)jmsgpack_object_u64(first) == S2C_CMD_LOADING);
                    }
                }
                jmsgpack_unpacked_destroy(&r);

                if (is_loading) {
                    int conv, frame_id, eid, seed;
                    int64_t random_state = 0;
                    S2CEntity entities[512];
                    int ec = unpack_loading(ev.data, ev.len, &conv, &frame_id, &eid, &seed,
                                            &random_state, entities, 512);
                    if (ec >= 0) {
                        // 用服务端下发的 seed 创建确定性随机序列，再用服务端当前单状态恢复，
                        // 保证与 server 的 random 推进位置完全一致（否则已推进的随机序列会分叉）。
                        auto& gs = ctx_->ecs.ctx().get<GlobalState>();
                        if (gs.random) lcg_random_destroy(gs.random);
                        gs.random_seed = seed;
                        gs.random = lcg_random_create((uint64_t)(uint32_t)seed);
                        lcg_random_set_state(gs.random, (uint64_t)random_state);
                        HandleLoading(conv, frame_id, eid, entities, ec);
                    }
                }
                else {
                    game_runtime(ctx_).command_queue.push(std::string(ev.data, ev.len));
                }
            }
            ctx_->transport->Release(ev);
        }
}

// ============================================================
// 服务端 / Host 网络（MODE_SERVER / MODE_HOST / MODE_SINGLE）：
// 收玩家命令 → 打包命令帧 → 广播/本地消费
// ============================================================

// ========== 网络命令处理（HandleCmd_*）==========
void NetworkManager::HandleCmdLoading(int conv)
{
        log_info("C2S_CMD_LOADING");
        auto view = ctx_->ecs.view<ConnectionComponent>();
        entt::entity conn = entt::null;
        for (auto e : view) {
                if (ctx_->ecs.get<ConnectionComponent>(e).conv == conv) {
                        conn = e;
                        break;
                }
        }
        if (conn == entt::null) {
                conn = ctx_->ecs.create();
                ctx_->ecs.emplace<ConnectionComponent>(conn, conv);
        }
        auto& comp = ctx_->ecs.get<ConnectionComponent>(conn);
        comp.is_loaded = false;
        comp.frameid = ctx_->ecs.ctx().get<GlobalState>().next_frame_id;
}

void NetworkManager::HandleCmdPlayerJoin(int conv, int64_t x, int64_t y, int char_id)
{
        S2CPlayerJoin j = { conv, x, y, char_id };
        net_session(ctx_).player_joins.push_back(j);
}

void NetworkManager::HandleCmdPlayerLeave(int conv)
{
        S2CPlayerLeave l = { conv };
        net_session(ctx_).player_leaves.push_back(l);
}

void NetworkManager::HandleCmdPlayerInput(int conv, uint32_t seq, int dir, int attack, int yaw)
{
        S2CPlayerInput i = { seq, conv, dir, (int8_t)attack, yaw };
        net_session(ctx_).player_inputs.push_back(i);
}

void NetworkManager::HandleCmdHeartbeat(int conv)
{
        log_info("%d: C2S_CMD_HEARTBEAT", conv);
        uint64_t ms_ticks;
        utils_get_current_time(&ms_ticks);
        auto view = ctx_->ecs.view<ConnectionComponent>();
        entt::entity conn = entt::null;
        for (auto e : view) {
                if (ctx_->ecs.get<ConnectionComponent>(e).conv == conv) {
                        conn = e;
                        break;
                }
        }
        if (conn == entt::null) {
                log_debug("Heartbeat from unknown conv: %d", conv);
                return;
        }
        auto& comp = ctx_->ecs.get<ConnectionComponent>(conn);
        comp.health = 10;
        comp.last_heartbeat = (uint32_t)ms_ticks;
}

// ========== 网络接收（服务端/Host）==========
void NetworkManager::ServerReceive()
{
        if (!ctx_->transport) return;
        NetTransportEvent ev;
        while (ctx_->transport->Poll(ev)) {
                if (ev.type == NetTransportEvent::Connected) {
                        log_info("connected=%d", ev.conv);
                }
                else if (ev.type == NetTransportEvent::Disconnected) {
                        log_info("disconnected=%d", ev.conv);
                        S2CPlayerLeave l = { ev.conv };
                        net_session(ctx_).player_leaves.push_back(l);
                }
                else if (ev.type == NetTransportEvent::Message) {
                        int cmd, input_dir = 0, input_attack = 0, char_id = 0, input_yaw = kAimYawInvalid; int64_t join_x = 0, join_y = 0;
                        if (unpack_c2s(ev.data, ev.len, &cmd, &join_x, &join_y, &input_dir, &input_attack, &char_id, &input_yaw)) {
                                switch (cmd) {
                                case CMD_LOADING:       HandleCmdLoading(ev.conv); break;
                                case CMD_PLAYER_JOIN:   HandleCmdPlayerJoin(ev.conv, join_x, join_y, char_id); break;
                                case CMD_PLAYER_LEAVE:  HandleCmdPlayerLeave(ev.conv); break;
                                case CMD_PLAYER_INPUT:  HandleCmdPlayerInput(ev.conv, 0, input_dir, input_attack, input_yaw); break;
                                case CMD_PLAYER_HEART:  HandleCmdHeartbeat(ev.conv); break;
                                }
                        }
                        else {
                                log_info("Failed to parse message from conv=%d", ev.conv);
                        }
                }
                ctx_->transport->Release(ev);
        }
}

void NetworkManager::ServerTick()
{
        if (ctx_->transport) {
                ctx_->transport->Update();
                ServerReceive();
        }
}

// ========== 收集输入 + 打包命令帧 ==========
// frame_id 由调用方在"决定推进本帧"时分配（见 game_scene.cpp on_update 的空 tick 跳过逻辑）。
void NetworkManager::CollectInput(int frame_id)
{
        S2CPlayerJoin joins[32]; int num_joins = 0;
        for (auto& j : net_session(ctx_).player_joins)
                if (num_joins < 32) joins[num_joins++] = j;
        S2CPlayerLeave leaves[32]; int num_leaves = 0;
        for (auto& l : net_session(ctx_).player_leaves)
                if (num_leaves < 32) leaves[num_leaves++] = l;
        S2CPlayerInput inputs[64]; int num_inputs = 0;
        for (auto& inp : net_session(ctx_).player_inputs)
                if (num_inputs < 64) inputs[num_inputs++] = inp;

        // 帧同步状态指纹（严格版）：与客户端共用同一公式（state_checksum.h）。
        // 纳入所有带 IdComponent + LogicPositionComponent 的实体（玩家 + 敌人 + 子弹）。
        std::vector<ChecksumEntry> entries;
        auto pos_view = ctx_->ecs.view<IdComponent, LogicPositionComponent>();
        for (auto e : pos_view) {
                auto& idc = pos_view.get<IdComponent>(e);
                auto& lpc = pos_view.get<LogicPositionComponent>(e);
                entries.push_back({ idc.id, lpc.x, lpc.y });
        }
        char checksum[64] = { 0 };
        compute_full_md5(entries, checksum, sizeof(checksum));

        auto command_data = pack_s2c(S2C_CMD_COMMAND, frame_id, joins, num_joins, leaves, num_leaves, inputs, num_inputs, checksum);
        net_session(ctx_).frames.StoreCommand(frame_id, command_data);
        game_runtime(ctx_).command_queue.push(command_data);

        // 缓存本帧校验记录（退出时落盘 checksum_log_server.txt，与客户端对称）：
        // checksum 填服务端权威值；inputs 用本帧实际 inputs；entities 用本帧实体快照。
        {
                std::string input_finger;
                for (int k = 0; k < num_inputs; k++) {
                        char b[64];
                        snprintf(b, sizeof(b), "%d:%d:%d;",
                                inputs[k].conv, inputs[k].dir,
                                (int)inputs[k].attack);
                        input_finger += b;
                }
                ChecksumRecord rec;
                rec.frame_id = frame_id;
                rec.inputs = input_finger;
                rec.entities = checksum_entries_string(entries);
                memcpy(rec.checksum, checksum, sizeof(rec.checksum));
                net_session(ctx_).checksum_records.push_back(rec);
        }

        net_session(ctx_).player_joins.clear();
        net_session(ctx_).player_leaves.clear();
        net_session(ctx_).player_inputs.clear();
        // 命令帧容量裁剪由 FrameCommandStream::StoreCommand 统一处理
}

// ========== 帧同步辅助 ==========
int NetworkManager::GetTargetFrameId(int curr_frameid, int context_frameid)
{
    // 追帧策略统一由帧流缓冲提供（网络策略，不属于玩法）
    return FrameCommandStream::TargetFrameId(curr_frameid, context_frameid);
}

// ========== 转发命令帧给客户端 ==========
void NetworkManager::InputForwarding()
{
        if (!ctx_->transport) return;
        auto view = ctx_->ecs.view<ConnectionComponent>();
        for (auto e : view) {
                auto& comp = view.get<ConnectionComponent>(e);
                if (comp.is_loaded) {
                        int taget_frameid = GetTargetFrameId(comp.frameid,
                            ctx_->ecs.ctx().get<GlobalState>().next_frame_id);
                        for (int i = comp.frameid; i < taget_frameid; i++) {
                            const std::string* cmd = net_session(ctx_).frames.FindCommand(i);
                            if (cmd) {
                                ctx_->transport->SendTo(comp.conv, cmd->c_str(), (int)cmd->size());
                            }
                            else {
                                log_error("Frame %d not found in commands, skip sending.", i);
                            }
                        }
                        comp.frameid = taget_frameid;
                }
                else {
                        int snap_frame = 0;
                        const std::string* snap = net_session(ctx_).frames.FindSnapshotAtOrBefore(comp.frameid, &snap_frame);
                        if (!snap) {
                                log_info("WARNING: no snapshot cache found for frame %d (client %d), sending empty loading", comp.frameid, comp.conv);
                                auto& gs = ctx_->ecs.ctx().get<GlobalState>();
                                uint64_t lcg_state = 0;
                                lcg_random_get_state(gs.random, &lcg_state);
                                auto data = pack_s2c_loading(comp.conv, comp.frameid,
                                        gs.next_entity_id, nullptr, 0,
                                        (int32_t)gs.random_seed, (int64_t)lcg_state);
                                ctx_->transport->SendTo(comp.conv, data.c_str(), data.size());
                                return;
                        }

                        S2CEntity entities[512];
                        int ec = 0, eid = 0;
                        ec = unpack_world(snap->c_str(), (int)snap->size(), &eid, entities, 512);
                        auto& gs = ctx_->ecs.ctx().get<GlobalState>();
                        uint64_t lcg_state = 0;
                        lcg_random_get_state(gs.random, &lcg_state);
                        // 关键：loading 快照是帧 snap_frame 的，上报的 frame_id 必须也是 snap_frame，
                        // 客户端才能用它精确初始化 AI 状态并从 snap_frame+1 开始演算。
                        // 之前误用 comp.frameid 导致"快照帧 ≠ 上报帧"，AI 状态初始化错位。
                        auto data = pack_s2c_loading(comp.conv, snap_frame, eid, entities, ec,
                        (int32_t)gs.random_seed, (int64_t)lcg_state);
                        ctx_->transport->SendTo(comp.conv, data.c_str(), data.size());
                        comp.frameid = snap_frame + 1;   // 客户端已初始化到 snap_frame，下一个要发的是 snap_frame+1
                        comp.is_loaded = true;
                }
        }
}

// ============================================================
// 单机 / Host 本地玩家辅助（MODE_SINGLE / MODE_HOST）
// ============================================================

// 注入本地玩家（单机/Host 与客户端 join 地址一致：当前关卡 'P' 格）
void NetworkManager::InjectLocalJoin()
{
        net_session(ctx_).local_conv = 1;
        fp_t sx_l, sy_l;
        map_player_spawn(ctx_->ecs.ctx().get<GlobalState>().level, &sx_l, &sy_l);
        S2CPlayerJoin j = { (int32_t)net_session(ctx_).local_conv, sx_l, sy_l, (int32_t)ctx_->selected_char_id };
        net_session(ctx_).player_joins.push_back(j);
}

// 把本地键盘/屏上输入转成 conv=1 的 S2CPlayerInput（单机/Host 每 tick 调用）
void NetworkManager::CollectLocalInput()
{
        int dir = game_runtime(ctx_).current_input_mask & INPUT_DIR_MASK;
        int attack = (game_runtime(ctx_).current_input_mask & INPUT_ATTACK) ? 1 : 0;
        if (game_runtime(ctx_).attack_triggered) { attack = 1; game_runtime(ctx_).attack_triggered = false; }
        // 瞄准角同 SendInput：有变化就补发一帧，静止时也能更新朝向
        const int yaw = collect_aim_yaw_q(ctx_);
        if (dir || attack || yaw != game_runtime(ctx_).last_aim_yaw_sent) {
                S2CPlayerInput i = { 0, (int32_t)net_session(ctx_).local_conv, dir, (int8_t)attack, yaw };
                net_session(ctx_).player_inputs.push_back(i);
                game_runtime(ctx_).last_aim_yaw_sent = yaw;
        }
}
