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
#include "../passes/player_status_hud_pass.h"
#include "../passes/frame_display_pass.h"
#include "../systems/input_consume_leave_system.h"
#include "../systems/input_consume_input_system.h"
#include "../systems/movement_system.h"
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

// 运行模式由 cfg_（kModeConfigs 中按 ctx->mode 取的能力配置）驱动，见 game_scene_common.h

void GameScene::Load()
{
    Context* ctx = this->ctx();
    log_info("[game scene] start... mode=%d", (int)ctx->mode);

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
    add_system(ctx, OnUpdate, "collision", collision_system);
    add_system(ctx, OnUpdate, "bullet", bullet_system);
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

    // 固定步长逻辑 Tick（20Hz）
    this->accumulator += dt;
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

    // 视觉相关 pass 用实时 dt 每帧更新
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
                if (!ev2.get<HealthComponent>(e).dead) { cleared = false; break; }
            }
        }
        if (cleared) {
            gsv.level_cleared = true;
            this->victory_delay += dt;
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

    // 玩家碰撞盒（淡墨调试可视化）
    //drawing_collision_debug_pass(ctx);

    // 模式特有渲染（碰撞调试、HUD 等）
    render();

    // 玩家状态 HUD（本地有玩家的模式显示）：血量文字 + 绿色胶囊血条
    if (ctx->mode != MODE_SERVER) {
        player_status_hud_pass(ctx->ecs, net_session(ctx).local_conv, ctx->r2d,
            AssetManager::GetInstance()->GetSimheiFont28());
    }

    // 当前逻辑帧号 + 实测逻辑帧率 + 敌人数量 + 全局实体 id
    int frame_id = (ctx->mode == MODE_SERVER || ctx->mode == MODE_HOST)
        ? ctx->ecs.ctx().get<GlobalState>().next_frame_id - 1
        : ctx->ecs.ctx().get<NetComponent>().frame_id;
    int logic_fps = ctx->ecs.ctx().get<GlobalState>().logic_fps;
    int enemy_count = (int)ctx->ecs.view<EnemyComponent>().size();
    int entity_id = ctx->ecs.ctx().get<GlobalState>().next_entity_id;
    frame_display_pass(ctx->r2d, AssetManager::GetInstance()->GetSimheiFont28(),
        frame_id, logic_fps, 8.0f, 60.0f, enemy_count, entity_id);

    // 水墨风小地图（右上角宣纸底）
    //drawing_minimap_pass(ctx);

    // 渲染模式子类挂载的 layer（控制层/调试层），纯 button 直接绘制
    if (this->debug_layer) this->debug_layer->Render();
    if (this->gcl) this->gcl->Render();
}

void GameScene::Destroy()
{
    Context* ctx = this->ctx();

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
    if (this->debug_layer) { delete this->debug_layer; this->debug_layer = NULL; }
    if (this->gcl) { delete this->gcl; this->gcl = NULL; }
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

    if (cfg_.localPlayer) {
        // 只有权威端才自己注入本地玩家；客户端的玩家由服务端下发的 join 命令产生
        if (cfg_.authoritative) GameSceneCommon::injectCommonPlayer(this);
        GameSceneCommon::initControlsAndAudio(this);
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
    // 权威碰撞网格调试：Host / Server 开启
    if (cfg_.drawCollisionDebug) drawing_collision_debug_pass(this->ctx());
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



