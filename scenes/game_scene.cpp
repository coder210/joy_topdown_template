#include <map>
#include <string>
#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <cstdio>
#include <memory>
#include <joy/joy.h>
#include "game_scene.h"
#include "game_scene_common.h"
#include "../protocol.h"
#include "../components/net_component.h"
#include "../passes/interpolate_pass.h"
#include "../passes/draw_player_pass.h"
#include "../passes/draw_enemy_pass.h"
#include "../passes/draw_bullet_pass.h"

#include "../systems/input_consume_join_system.h"
#include "../systems/knockback_system.h"
#include "../passes/player_status_hud_pass.h"
#include "../passes/enemy_counter_hud_pass.h"
#include "../passes/frame_display_pass.h"
#include "../systems/input_consume_leave_system.h"
#include "../systems/input_consume_input_system.h"
#include "../systems/movement_system.h"
#include "../systems/separation_system.h"   // 敌人间分离（防止叠在一起）
#include "../systems/cleanup_system.h"
#include "../systems/collision_system.h"
#include "../layers/debug_layer.h"
#include "../asset_manager.h"
#include "../network/network_manager.h"
#include "../passes/camera_follow_pass.h"
#include "../systems/ai_system.h"
#include "../systems/melee_attack_system.h"
#include "../systems/player_shoot_system.h"
#include "../systems/bullet_system.h"
#include "../network/lan_discovery.h"


#include "../systems/player_death_system.h"
#include "../systems/pathfinding_system.h"

#include "../passes/drawing_map_pass.h"
#include "../passes/drawing_collision_debug_pass.h"
#include "../systems/state_checksum.h"
#include "../systems/map_spawn_system.h"
#include "../passes/fog_pass_2d.h"
#include "../passes/drawing_minimap_pass.h"
#include "victory_scene.h"
#include "scene.h"
#include "../logical_presentation.h"

// 运行模式由 cfg_（kModeConfigs 中按 ctx->mode 取的能力配置）驱动，见 game_scene_common.h

void GameScene::Load()
{
    Context* ctx = this->ctx();
    log_info("[game scene] start... mode=%d", (int)ctx->mode);

    // 游戏场景用自适应逻辑宽度：铺满窗口、左右无黑边（宽屏横向多看地图）。
    // 其它场景在 GameScene::Destroy 里恢复设计分辨率（3:2），避免 UI 比例被拉伸。
    presentation_set_adaptive(ctx->r2d);

    this->debug_layer = new DebugLayer(ctx);

    // 注册全局单例
    ctx->ecs.ctx().emplace<NetComponent>();
    auto& gs = ctx->ecs.ctx().emplace<GlobalState>();
    gs.level = ctx->selected_level;   // 应用选关页选定的固定地图关卡
    map_load_level(gs.level);         // 装入当前关卡 64x64 ASCII 网格（墙+出生）
    // 俯视模式（资源原始设计）：相机 X/Y 同时跟随玩家，支持全向移动
    gs.side_view = false;
    gs.camera_lock_y = -1.0f;
    gs.camera_bias_x = 0.0f;

    // 游戏场景共享运行量（默认构造）：
    // GameRuntime / NetSession 原本内嵌于 Context(game/ net)，现按引擎的
    // GlobalState 模式存入 registry.ctx()，供仅持有 Context* 的 system/pass/network 读取。
    ctx->ecs.ctx().emplace<GameRuntime>();
    ctx->ecs.ctx().emplace<NetSession>();

    // System 调度注册表（registry.ctx() 组件）：随后 add_system 写入、run_phase 读取。
    ctx->ecs.ctx().emplace<SystemRegistry>();

    // 注册公共系统
    add_system(ctx, OnStart, "map_spawn", map_spawn_system);
    add_system(ctx, PreUpdate, "input_consume_join", input_consume_join_system);
    add_system(ctx, PreUpdate, "input_consume_leave", input_consume_leave_system);
    add_system(ctx, PreUpdate, "input_consume_input", input_consume_input_system);
    add_system(ctx, OnUpdate, "ai_system", ai_system);
    add_system(ctx, OnUpdate, "melee_attack", melee_attack_system);
    add_system(ctx, OnUpdate, "player_shoot", player_shoot_system);


    add_system(ctx, OnUpdate, "player_death", player_death_system);
    add_system(ctx, OnUpdate, "pathfinding", pathfinding_system);
    add_system(ctx, OnUpdate, "movement", movement_system);
    // 敌人间分离：必须在 pathfinding/movement 之后、collision（墙）之前，
    // 这样被推开后万一进入墙格，collision_system 会再把它推出墙体。
    add_system(ctx, OnUpdate, "separation", separation_system);
    add_system(ctx, OnUpdate, "collision", collision_system);
    add_system(ctx, OnUpdate, "bullet", bullet_system);
    add_system(ctx, OnUpdate, "knockback", knockback_system);
    add_system(ctx, OnUpdate, "cleanup", cleanup_system);

    // 模式特有初始化（多态分派到子类）：注册 spawn_enemy/snapshot，并建链、注入本地玩家
    init();

    // 执行 OnStart phase（生成地图 + 生成敌人）：必须在 init() 之后，
    // 因为 spawn_enemy_system 在 initAuthoritative 内才注册到 OnStart；
    // 若先跑 OnStart，则该系统尚未注册，敌人永远不会被生成。
    run_phase(ctx, OnStart, 0);
}

void GameScene::HandleEvent(const void* ev)
{
    // 暂停菜单优先：菜单打开时模态吞掉所有输入（ESC/返回键、按钮点击都在这里处理）；
    // 菜单关闭时只拦截右上角暂停按钮，其余事件继续传给游戏。
    if (this->pause_layer && this->pause_layer->handle_event(ev)) return;

    handleEvent(ev);
    // 转发事件给操作层（纯 button），模拟原 joy scene 的 node 事件分发
    if (this->gcl) this->gcl->HandleEvent(ev);
}

// 消费一条命令帧：从 command_queue 取一条，解包后写入 NetComponent（模拟前），
// 客户端/单机模式下做帧同步 checksum 校验。返回是否拿到可模拟的命令帧。
bool GameScene::consumeCommandFrame()
{
    Context* ctx = this->ctx();
    bool got_command = false;
    if (!game_runtime(ctx).command_queue.empty()) {
        std::string command_data = game_runtime(ctx).command_queue.front();
        game_runtime(ctx).command_queue.pop();

        int frame_id, jn = 0, ln = 0, in = 0;
        S2CPlayerJoin joins[32];
        S2CPlayerLeave leaves[32];
        S2CPlayerInput inputs[64];
        char server_checksum[64] = { 0 };
        bool ok = unpack_command(command_data.c_str(), (int)command_data.size(),
            &frame_id, joins, 32, &jn, leaves, 32, &ln, inputs, 64, &in,
            server_checksum, sizeof(server_checksum));
        if (ok) {
            // 客户端跳过 <= server_frameid 的帧（模式特定，经虚函数）
            if (shouldSkipFrame(frame_id)) {
                // 跳过，不模拟
            } else {
                got_command = true;
                auto& cmd = ctx->ecs.ctx().get<NetComponent>();
                cmd.frame_id = frame_id;
                cmd.num_joins = jn;  memcpy(cmd.joins, joins, jn * sizeof(S2CPlayerJoin));
                cmd.num_leaves = ln; memcpy(cmd.leaves, leaves, ln * sizeof(S2CPlayerLeave));
                cmd.num_inputs = in; memcpy(cmd.inputs, inputs, in * sizeof(S2CPlayerInput));

                // 帧同步校验（客户端 / 单机，经虚函数）
                verifyFrameSync(frame_id, inputs, in, server_checksum);
            }
        }
    }
    return got_command;
}

void GameScene::Update(float dt)
{
    static uint32_t s_diag_t0 = 0;
    static int     s_diag_frames = 0, s_diag_ticks = 0;
    static float   s_diag_dtsum = 0;
    Context* ctx = this->ctx();

    // 网络层更新（模式特有）
    networkUpdate();

    // 局域网房间广播：房主/专用服每 200ms 驱动一次（内部仍 1s 才真正发包），
    // 降低每帧对连接数查询/时钟读取的调用频率，避免给主循环添负担。
    if (LanDiscovery::Get().beacon_active()) {
        static uint64_t s_last_lan_ms = 0;
        uint64_t now_ms = get_ticks();
        if (now_ms - s_last_lan_ms >= 200) {
            s_last_lan_ms = now_ms;
            int n = ctx->net_server ? netserver_connection_count(ctx->net_server) : 0;
            if (cfg_.localPlayer) n += 1;
            LanDiscovery::Get().set_beacon_players(n);
            LanDiscovery::Get().update();
        }
    }

    // 单机暂停：跳过固定步模拟；联机不暂停（本地暂停会让双端帧同步发散）。
    // 暂停期间不累积时间，恢复后也不会补跑一串 tick。
    const bool sim_paused = (this->pause_layer && this->pause_layer->should_pause_sim());
    if (sim_paused) this->accumulator = 0.0f;
    else            this->accumulator += dt;
    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    // ---- 临时诊断：每秒打点 dt 累计 / 渲染帧数 / 实际逻辑 tick 数 ----
    if (get_ticks() - s_diag_t0 >= 1000) {
        log_info("[diag] framerate=%d dt_sum=%.4f ticks=%d logic=%d",
            s_diag_frames, s_diag_dtsum, s_diag_ticks, gs.logic_fps);
        s_diag_frames = 0; s_diag_dtsum = 0; s_diag_ticks = 0; s_diag_t0 = get_ticks();
    }
    s_diag_frames++; s_diag_dtsum += dt;
    while (this->accumulator >= game_runtime(ctx).FIXED_TIMESTEP) {
        this->accumulator -= game_runtime(ctx).FIXED_TIMESTEP;
        s_diag_ticks++;

        // 模式特有 tick：帧号分配、输入收集、命令打包/转发、输入发送
        tick();

        // 消费一条命令帧（模拟前先写 NetComponent）
        bool got_command = consumeCommandFrame();

        // 模拟推进（是否推进由模式决定，经虚函数）
        if (shouldSimulate(got_command)) {
            run_phase(ctx, PreUpdate, game_runtime(ctx).FIXED_TIMESTEP);
            run_phase(ctx, OnUpdate, game_runtime(ctx).FIXED_TIMESTEP);
            run_phase(ctx, PostUpdate, game_runtime(ctx).FIXED_TIMESTEP);
        }
    }

    // 把逻辑步长写入 GlobalState，供 interpolate_pass 当作渲染平滑时间常数
    //（指数平滑：既把 20Hz 逻辑步进补成 60Hz 平滑移动，又把敌人拐弯直角磨圆）。
    if (ctx->ecs.ctx().contains<GlobalState>()) {
        auto& gs = ctx->ecs.ctx().get<GlobalState>();
        gs.fixed_timestep = game_runtime(ctx).FIXED_TIMESTEP;
    }

    // 逻辑帧率测量
    // 客户端（MODE_CLIENT）本端 gs.next_frame_id 不递增（由服务器权威帧号 NetComponent.frame_id 驱动），
    // 直接用它度量会恒为 0；因此客户端改用 NetComponent.frame_id，其余模式仍用本端 next_frame_id。
    uint32_t now_ms = get_ticks();
    int cur_logic_frame = currentLogicFrame();
    if (this->fps_measure_time == 0) {
        this->fps_measure_time  = now_ms;
        this->fps_measure_frame = cur_logic_frame;
    } else if (now_ms - this->fps_measure_time >= 1000) {
        int elapsed_frames = cur_logic_frame - this->fps_measure_frame;
        if (elapsed_frames < 0) elapsed_frames = 0;
        gs.logic_fps = (int)(elapsed_frames * 1000 / (now_ms - this->fps_measure_time));
        this->fps_measure_time  = now_ms;
        this->fps_measure_frame = cur_logic_frame;
    }

    // 视觉相关 pass 用实时 dt 每帧更新（子弹已不参与插值，由 draw_bullet_pass 直接按逻辑位置绘制）
    interpolate_pass(ctx->ecs, ctx, dt);
    camera_follow_pass(ctx->ecs, ctx, dt);
    // 玩家动画推进已并入 Render 阶段的 player_render_pass（纯渲染、不参与帧同步），
    // 这里仅把本帧 dt 存下供渲染期推进动画使用。
    this->render_dt_ = dt;

    // 视觉时间推进
    game_runtime(ctx).visual_time += dt;

    // ---- 通关判定（与 3D game_scene 业务逻辑对齐）----
    // 关卡开始后，检测全部敌人被肃清则延时切入胜利场景（非服务端）。
    // 优先取权威字段 level_cleared；兜底用本地实体状态推导（帧同步实体同步，跨端确定性一致）。
    if (!this->game_started) {
        auto en0 = ctx->ecs.view<EnemyComponent>();
        if (en0.begin() != en0.end()) this->game_started = true;
    }
    if (!this->victory_started && shouldCheckVictory() &&
        ctx->ecs.ctx().contains<GlobalState>()) {
        auto& gsv = ctx->ecs.ctx().get<GlobalState>();
        bool cleared = gsv.level_cleared;
        if (!cleared && this->game_started) {
            cleared = true;
            auto ev2 = ctx->ecs.view<EnemyComponent, HealthComponent>();
            for (auto e : ev2) {
                // hp<=0 即视为已死（尸体可能下一 tick 才被 player_death 销毁，
                // 不能只依赖 dead 标记，否则最后一只击杀后会多等一 tick）。
                const auto& h = ev2.get<HealthComponent>(e);
                if (!h.dead && h.hp > 0) { cleared = false; break; }
            }
        }
        if (cleared) {
            gsv.level_cleared = true;
            if (!sim_paused) this->victory_delay += dt;
            if (this->victory_delay >= 1.2f) {
                this->victory_started = true;
                ctx->scene_mgr->setPending(new VictoryScene(ctx));
                push_user_event(VICTORY_EVENT, VICTORY_EVENT_TRIGGERED);
            }
        }
    }
    // 驱动模式子类挂载的 layer 更新（纯 button 层无需 scene 图更新）
}

void GameScene::Render()
{
    Context* ctx = this->ctx();

    // 几何网格背景 + 地形装饰
    //draw_grid_background(ctx);
    draw_map_pass(ctx);

    // 按对象分 pass 渲染（每个 pass 只负责一类实体的绘制，职责清晰）
    draw_enemy_pass(ctx);              // 敌人：墨色方块占位

    player_render_pass(ctx->ecs, ctx, this->render_dt_);   // 玩家：动画推进 + 绘制（characters.json 序列帧）

    draw_bullet_pass(ctx);              // 飞行中的箭/子弹（tilesheet.png col7，按速度旋转）

    // 2D 战争迷雾：覆盖地图与实体，仅暴露视野附近，墙后与远端被墨雾遮蔽
    //fog_pass_2d(ctx);

    // 碰撞盒调试可视化（玩家红框 / 墙绿框）
    // 开关由 pass 内部判定：settings.show_collision（设置页"显示碰撞框"勾选）。
    // 原来这行被注释掉，而 render() 里那处调用又受 cfg_.drawCollisionDebug 限制
    // —— kModeConfigs 里 4 个模式该值全是 false，等于永远不画，勾了也没反应。
    drawing_collision_debug_pass(ctx);

    // 模式特有渲染（碰撞调试、HUD 等）
    render();

    // 常驻 HUD（本地有玩家的模式）：右上角“剩余敌人 / 总数”，避开视野中心
    if (ctx->mode != MODE_SERVER) {
        enemy_counter_hud_pass(ctx->ecs, ctx->r2d,
            AssetManager::GetInstance()->GetSimheiFont12());
    }

    // 调试 HUD（FPS / Frame / POS / ST / EID）：默认关闭，F1 或暂停菜单里的
    // “调试信息”开关切换。不铺底色（透明），只用小号字 + 白色描边保证可读。
    if (ctx->show_debug_hud) {
        if (ctx->mode != MODE_SERVER) {
            player_status_hud_pass(ctx->ecs, net_session(ctx).local_conv, ctx->r2d,
                AssetManager::GetInstance()->GetSimheiFont12());
        }

        // 当前逻辑帧号 + 实测逻辑帧率 + 敌人数量 + 全局实体 id
        int frame_id = (ctx->mode == MODE_SERVER || ctx->mode == MODE_HOST)
            ? ctx->ecs.ctx().get<GlobalState>().next_frame_id - 1
            : ctx->ecs.ctx().get<NetComponent>().frame_id;
        int logic_fps = ctx->ecs.ctx().get<GlobalState>().logic_fps;
        int enemy_count = (int)ctx->ecs.view<EnemyComponent>().size();
        int entity_id = ctx->ecs.ctx().get<GlobalState>().next_entity_id;
        frame_display_pass(ctx->r2d, AssetManager::GetInstance()->GetSimheiFont12(),
            frame_id, logic_fps, 8.0f, 36.0f, enemy_count, entity_id);
    }

    // 水墨风小地图（右上角宣纸底）
    //drawing_minimap_pass(ctx);

    // 渲染模式子类挂载的 layer（控制层/调试层），纯 button 直接绘制
    if (this->debug_layer) this->debug_layer->Render();
    if (this->gcl) this->gcl->Render();
    if (this->pause_layer) this->pause_layer->Render();   // 暂停菜单在最上层
}

void GameScene::Destroy()
{
    Context* ctx = this->ctx();

    // 停止局域网广播（房主/专用服）
    LanDiscovery::Get().stop_beacon();

    // 网络清理
    if (ctx->netclient) {
        netclient_destroy(ctx->netclient);
        ctx->netclient = NULL;
    }
    if (ctx->net_server) {
        netserver_destroy(ctx->net_server);
        ctx->net_server = NULL;
    }
    AssetManager::GetInstance()->ClearTexCache();
    player_anim_cache_clear();   // 释放玩家动画定义缓存

    // 模式特有清理（音频等）
    destroy();

    // 退出时统一落盘帧同步校验记录
    if (!net_session(ctx).checksum_records.empty()) {
        const char* log_name = logTag();
        FILE* fp = fopen(log_name, "w");
        if (fp) {
            fprintf(fp, "frame\tchecksum\tinputs\tentities\n");
            for (auto& r : net_session(ctx).checksum_records) {
                fprintf(fp, "%d\t%s\t%s\t%s\n",
                    r.frame_id, r.checksum,
                    r.inputs.c_str(), r.entities.c_str());
            }
            fclose(fp);
        }
    }
    if (this->pause_layer) { delete this->pause_layer; this->pause_layer = NULL; }
    if (this->debug_layer) { delete this->debug_layer; this->debug_layer = NULL; }
    if (this->gcl) { delete this->gcl; this->gcl = NULL; }

    // 逻辑分辨率保持自适应宽度（全局统一），无需恢复 3:2。
}

GameScene::GameScene(Context* ctx)
    : Scene(ctx, "game"), net_(ctx), cfg_(mode_config(ctx->mode))
{
}

// ============================================================
// 模式步骤：全部由 cfg_ 的能力开关驱动（取代原 4 个子类的 override）
// ============================================================
void GameScene::init()
{
    Context* ctx = this->ctx();
    if (cfg_.authoritative)
        GameSceneCommon::initAuthoritative(ctx);

    // 建链：Host/Server 起服务端，Client 连服务端，单机无网络
    if (cfg_.transport == Transport::Server)
        ctx->net_server = netserver_create(NET_SERVER_WEBSOCKET,
                                           ctx->settings.server_ip, ctx->settings.server_port);
    else if (cfg_.transport == Transport::Client)
        ctx->netclient = netclient_create(NET_CLIENT_WEBSOCKET,
                                          ctx->settings.server_ip, ctx->settings.server_port);

    // 房主/专用服：开始局域网广播，让大厅能发现本房间
    if (cfg_.transport == Transport::Server) {
        auto& gsv = ctx->ecs.ctx().get<GlobalState>();
        char room_name[64];
        snprintf(room_name, sizeof(room_name), "%s的房间", ctx->selected_nickname);
        LanDiscovery::Get().start_beacon(room_name, ctx->settings.server_port,
                                         gsv.level, kRoomMaxPlayers);
    }

    if (cfg_.localPlayer) {
        // 只有权威端才自己注入本地玩家；客户端的玩家由服务端下发的 join 命令产生
        if (cfg_.authoritative) GameSceneCommon::injectCommonPlayer(this);
        GameSceneCommon::initControlsAndAudio(this);

        // 暂停菜单（右上角按钮 + 菜单/确认/设置），仅本地玩家场景需要
        this->pause_layer = new PauseMenuLayer(ctx);
    }
}

void GameScene::handleEvent(const void* ev)
{
    if (cfg_.localPlayer) {
        GameSceneCommon::playableHandleEvent(this, ev);
        return;
    }
    // 独立服务端：只响应退出键 q
    jevent_callbacks_t cbs = {};
    cbs.on_keypressed = GameScene::onServerKeyPressed;
    dispatch_event(cbs, ev, NULL);
}

void GameScene::networkUpdate()
{
    Context* ctx = this->ctx();
    if (cfg_.transport == Transport::Server) {
        this->net_.ServerTick();
    } else if (cfg_.transport == Transport::Client && ctx->netclient) {
        netclient_update(ctx->netclient);
        this->net_.ClientReceive();
    }
    // Transport::None（单机）：无网络
}

void GameScene::tick()
{
    if (!cfg_.authoritative) {          // 客户端：只上报自己的输入
        this->net_.SendInput();
        return;
    }
    // ↓ 以下全是权威端路径，顺序即流程
    auto& gs = this->ctx()->ecs.ctx().get<GlobalState>();
    int frame_id = gs.next_frame_id++;
    if (cfg_.localPlayer) this->net_.CollectLocalInput();
    this->net_.CollectInput(frame_id);
    if (cfg_.transport == Transport::Server) this->net_.InputForwarding();
}

void GameScene::render()
{
    // 碰撞调试绘制已移到主绘制序列（见上方 drawing_collision_debug_pass 调用），
    // 由设置页"显示碰撞框"开关控制。不再走 cfg_.drawCollisionDebug ——
    // kModeConfigs 里 4 个模式该值都是 false，按它判断则任何模式都画不出来。
}

void GameScene::destroy()
{
    if (cfg_.localPlayer) GameSceneCommon::destroyPlayerAudio(this->ctx());
}

// ============================================================
// 帧同步 / 模式特征
// ============================================================
bool GameScene::shouldSkipFrame(int frame_id) const
{
    // 只有非权威端（客户端）会收到落后于服务器的帧
    return !cfg_.authoritative && frame_id <= game_runtime(this->ctx()).server_frameid;
}

void GameScene::verifyFrameSync(int frame_id, S2CPlayerInput* inputs, int in,
                                const char* server_checksum)
{
    if (cfg_.verifyChecksum)
        GameSceneCommon::verifyFrameSyncImpl(this, frame_id, inputs, in, server_checksum);
}

bool GameScene::shouldSimulate(bool got_command) const
{
    return cfg_.authoritative || got_command;   // 权威端无条件推进；客户端等命令帧
}

int GameScene::currentLogicFrame() const
{
    auto& gs = this->ctx()->ecs.ctx().get<GlobalState>();
    // 客户端本端 next_frame_id 不递增（由服务器权威帧号驱动），改用 server_frameid
    return cfg_.authoritative ? gs.next_frame_id
                              : game_runtime(this->ctx()).server_frameid;
}

bool GameScene::shouldCheckVictory() const
{
    return cfg_.localPlayer;   // 无本地玩家的服务端不做通关判定
}

void GameScene::onServerKeyPressed(const char* key, int scancode, bool repeat,
                                   bool ctrl, bool shift, bool alt, void* arg)
{
    (void)scancode; (void)repeat; (void)ctrl; (void)shift; (void)alt; (void)arg;
    if (strcmp(key, "q") == 0 || strcmp(key, "Q") == 0) {
        push_quit_event();
    }
}



