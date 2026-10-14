#include <cstring>
#include <joy/joy.h>
#include "game_scene_common.h"
#include "game_scene.h"
#include "../context.h"
#include "../asset_manager.h"
#include "../protocol.h"
#include "../layers/gameplay_controls_layer.h"
#include "../network/network_manager.h"
#include "../systems/spawn_enemy_system.h"
#include "../systems/snapshot_system.h"

// ============================================================
// 本机玩家键盘回调（单机/客户端/Host 共用）
// ============================================================
void GameSceneCommon::playKeyPressed(const char* key, int scancode, bool repeat,
                      bool ctrl, bool shift, bool alt, void* arg)
{
    GameScene* self = (GameScene*)arg;
    (void)scancode; (void)repeat; (void)ctrl; (void)shift; (void)alt;
    int key_mask = 0;
    if (strcmp(key, "a") == 0) key_mask = INPUT_LEFT;
    else if (strcmp(key, "A") == 0) key_mask = INPUT_LEFT;
    else if (strcmp(key, "d") == 0) key_mask = INPUT_RIGHT;
    else if (strcmp(key, "D") == 0) key_mask = INPUT_RIGHT;
    else if (strcmp(key, "w") == 0) key_mask = INPUT_UP;
    else if (strcmp(key, "W") == 0) key_mask = INPUT_UP;
    else if (strcmp(key, "s") == 0) key_mask = INPUT_DOWN;
    else if (strcmp(key, "S") == 0) key_mask = INPUT_DOWN;
    else if (strcmp(key, "up") == 0) key_mask = INPUT_UP;
    else if (strcmp(key, "down") == 0) key_mask = INPUT_DOWN;
    else if (strcmp(key, "space") == 0) key_mask = INPUT_ATTACK;


    if (key_mask) {
        game_runtime(self->ctx()).current_input_mask |= key_mask;
        if (key_mask & INPUT_DIR_MASK)
            game_runtime(self->ctx()).local_dir = input_to_dir(key_mask);
    }
}

void GameSceneCommon::playKeyReleased(const char* key, int scancode, void* arg)
{
    GameScene* self = (GameScene*)arg;
    (void)scancode;
    int key_mask = 0;
    if (strcmp(key, "a") == 0) key_mask = INPUT_LEFT;
    else if (strcmp(key, "A") == 0) key_mask = INPUT_LEFT;
    else if (strcmp(key, "d") == 0) key_mask = INPUT_RIGHT;
    else if (strcmp(key, "D") == 0) key_mask = INPUT_RIGHT;
    else if (strcmp(key, "w") == 0) key_mask = INPUT_UP;
    else if (strcmp(key, "W") == 0) key_mask = INPUT_UP;
    else if (strcmp(key, "s") == 0) key_mask = INPUT_DOWN;
    else if (strcmp(key, "S") == 0) key_mask = INPUT_DOWN;
    else if (strcmp(key, "up") == 0) key_mask = INPUT_UP;
    else if (strcmp(key, "down") == 0) key_mask = INPUT_DOWN;
    else if (strcmp(key, "space") == 0) key_mask = INPUT_ATTACK;


    if (key_mask) game_runtime(self->ctx()).current_input_mask &= ~key_mask;
}

// ============================================================
// 操作层事件 + 键盘事件分发（单机/客户端/Host 共用）
// ============================================================
void GameSceneCommon::playableHandleEvent(GameScene* self, const void* ev)
{
    Context* ctx = self->ctx();
    if (is_user_event(ev, GAMEPLAY_CONTROL_EVENT)) {
        int new_mask = game_runtime(ctx).current_input_mask;
        switch (get_event_code(ev)) {
        case GAMEPLAY_CONTROL_LEFT:  new_mask |= INPUT_LEFT; break;
        case GAMEPLAY_CONTROL_RIGHT: new_mask |= INPUT_RIGHT; break;
        case GAMEPLAY_CONTROL_UP:    new_mask |= INPUT_UP; break;
        case GAMEPLAY_CONTROL_DOWN:  new_mask |= INPUT_DOWN; break;
        case GAMEPLAY_CONTROL_RELEASE_LEFT:  new_mask &= ~INPUT_LEFT; break;
        case GAMEPLAY_CONTROL_RELEASE_RIGHT: new_mask &= ~INPUT_RIGHT; break;
        case GAMEPLAY_CONTROL_RELEASE_UP:    new_mask &= ~INPUT_UP; break;
        case GAMEPLAY_CONTROL_RELEASE_DOWN:  new_mask &= ~INPUT_DOWN; break;
        case GAMEPLAY_CONTROL_ATTACK:
            game_runtime(ctx).attack_triggered = true;
            break;
        default:
            new_mask &= ~(INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT);
            break;
        }
        if (new_mask != game_runtime(ctx).current_input_mask) {
            game_runtime(ctx).current_input_mask = new_mask;
        }
        return;
    }
    jevent_callbacks_t cbs = {};
    cbs.on_keypressed = GameSceneCommon::playKeyPressed;
    cbs.on_keyreleased = GameSceneCommon::playKeyReleased;
    dispatch_event(cbs, ev, self);
}

// ============================================================
// 权威端初始化（单机 / Host / 服务端）
// ============================================================
void GameSceneCommon::initAuthoritative(Context* ctx)
{
    add_system(ctx, OnStart, "spawn_enemy", spawn_enemy_system);
    add_system(ctx, PostUpdate, "snapshot", snapshot_system);
    auto& gs = ctx->ecs.ctx().get<GlobalState>();
    // 确定性种子：仅由关卡派生（不再依赖 get_ticks()/本地地址），
    // 保证单机、同进程多端、以及未走 loading 握手时，两端 RNG 序列一致。
    // 联网时服务端仍会经 loading 协议下发其 seed+完整 LCG 状态覆盖此值（见 network_manager.cpp）。
    uint32_t lvl = (uint32_t)gs.level;
    gs.random_seed = (int)(0x9E3779B1u + lvl * 2654435761u);
    gs.random = lcg_random_create((uint64_t)(uint32_t)gs.random_seed);
}

// ============================================================
// 音频辅助：可选资源 + 按扩展名选择解码器
// ============================================================
// 大小写不敏感比较（自行实现，避免依赖 SDL_strcasecmp / POSIX strcasecmp）
static bool audio_ext_eq(const char* ext, const char* want)
{
    if (!ext || !want) return false;
    for (; *ext && *want; ++ext, ++want) {
        char a = *ext, b = *want;
        if (a >= 'A' && a <= 'Z') a = (char)(a + 32);
        if (b >= 'A' && b <= 'Z') b = (char)(b + 32);
        if (a != b) return false;
    }
    return *ext == '\0' && *want == '\0';
}

static const char* audio_file_ext(const char* path)
{
    if (!path) return "";
    const char* dot = std::strrchr(path, '.');
    return dot ? dot : "";
}

// audio_shot_create / audio_bgm_create 内部走 SDL_LoadWAV，仅支持 wav；
// 这里按扩展名分派到 ogg/mp3/flac 变体（与 joy 的 luaclib/laudio.c 的 by_ext 同思路），
// 使 Kenney 等以 ogg 为主的音效包直接放进 assets 即可用。
static audio_shot_p shot_create_by_ext(const char* path, uint32_t dev)
{
    if (!path || !path[0]) return NULL;
    const char* ext = audio_file_ext(path);
    if (audio_ext_eq(ext, ".ogg"))  return audio_shot_create_ogg(path, dev);
    if (audio_ext_eq(ext, ".mp3"))  return audio_shot_create_mp3(path, dev);
    if (audio_ext_eq(ext, ".flac")) return audio_shot_create_flac(path, dev);
    return audio_shot_create(path, dev);
}

static audio_bgm_p bgm_create_by_ext(const char* path, uint32_t dev)
{
    if (!path || !path[0]) return NULL;
    const char* ext = audio_file_ext(path);
    if (audio_ext_eq(ext, ".ogg"))  return audio_bgm_create_ogg(path, dev);
    if (audio_ext_eq(ext, ".mp3"))  return audio_bgm_create_mp3(path, dev);
    if (audio_ext_eq(ext, ".flac")) return audio_bgm_create_flac(path, dev);
    return audio_bgm_create(path, dev);
}

// ============================================================
// 本机操作层 + 音频（单机 / 客户端 / Host）
// ============================================================
void GameSceneCommon::initControlsAndAudio(GameScene* self)
{
    Context* ctx = self->ctx();
    self->gcl = new GameplayControlsLayer(ctx);   // 纯 button 操作层，直接挂在 gs_ 上由 GameScene 桥接

    ctx->audio_dev = audio_open_device();
    // 音频为可选资源：路径走 AssetManager 的 audio 段（resources.json 为真源，缺失则回退内置默认）。
    // 文件不存在或解码失败时 create 返回 NULL，而 joy 的 set_gain / play 对 NULL 安全 → 静音继续运行。
    const char* footstep = AssetManager::GetInstance()
        ? AssetManager::GetInstance()->GetResourcePath(AssetManager::kCatAudio, "footstep") : nullptr;
    const char* bgm_path = AssetManager::GetInstance()
        ? AssetManager::GetInstance()->GetResourcePath(AssetManager::kCatAudio, "bgm") : nullptr;

    ctx->step_sound = shot_create_by_ext(footstep, ctx->audio_dev);
    if (!ctx->step_sound)
        log_info("[audio] footstep unavailable (path=%s), muted", footstep ? footstep : "(none)");
    audio_shot_set_gain(ctx->step_sound, ctx->settings.sfx_volume);

    ctx->bgm = bgm_create_by_ext(bgm_path, ctx->audio_dev);
    if (!ctx->bgm)
        log_info("[audio] bgm unavailable (path=%s), muted", bgm_path ? bgm_path : "(none)");
    audio_bgm_set_gain(ctx->bgm, ctx->settings.bgm_volume);
}

// ============================================================
// 注入本地玩家（单机 / Host）
// ============================================================
void GameSceneCommon::injectCommonPlayer(GameScene* self)
{
    self->net_.InjectLocalJoin();
}

// ============================================================
// 音频清理（单机 / 客户端 / Host）
// ============================================================
void GameSceneCommon::destroyPlayerAudio(Context* ctx)
{
    if (ctx->step_sound) { audio_shot_destroy(ctx->step_sound); ctx->step_sound = NULL; }
    if (ctx->bgm) { audio_bgm_destroy(ctx->bgm); ctx->bgm = NULL; }
    if (ctx->audio_dev) { audio_close_device(ctx->audio_dev); ctx->audio_dev = 0; }
}

// ============================================================
// 帧同步校验实现（客户端 / 单机共用）：由各模式 verifyFrameSync override 调用
// ============================================================
void GameSceneCommon::verifyFrameSyncImpl(GameScene* self, int frame_id,
                            S2CPlayerInput* inputs, int in, const char* server_checksum)
{
    Context* ctx = self->ctx();
    std::vector<ChecksumEntry> entries;
    auto pv = ctx->ecs.view<IdComponent, LogicPositionComponent>();
    for (auto e : pv) {
        auto& idc = pv.get<IdComponent>(e);
        auto& lpc = pv.get<LogicPositionComponent>(e);
        entries.push_back({ idc.id, lpc.x, lpc.y });
    }
    char local_checksum[64] = { 0 };
    compute_full_md5(entries, local_checksum, sizeof(local_checksum));

    std::string input_finger;
    for (int k = 0; k < in; k++) {
        char b[64];
        snprintf(b, sizeof(b), "%d:%d:%d:",
                inputs[k].conv, inputs[k].dir,
                (int)inputs[k].attack);
        input_finger += b;
    }

    int mismatched = (strcmp(local_checksum, server_checksum) != 0) ? 1 : 0;
    net_session(ctx).checksum_records.push_back(
            { frame_id, "", input_finger, checksum_entries_string(entries) });
    auto& rec = net_session(ctx).checksum_records.back();
    memcpy(rec.checksum, local_checksum, sizeof(rec.checksum));
    if (mismatched) {
        printf("[CHECKSUM] frame=%d MISMATCH server=%s local=%s inputs=%s entities=%s\n",
                frame_id, server_checksum, local_checksum,
                input_finger.c_str(), checksum_entries_string(entries).c_str());
        fflush(stdout);
    }
}
