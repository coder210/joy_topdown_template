#include <cstring>
#include <joy/joy.h>
#include "game_scene_common.h"
#include "game_scene.h"
#include "../context.h"
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
    else if (strcmp(key, "e") == 0 || strcmp(key, "E") == 0) { game_runtime(self->ctx()).bullet_triggered = true; return; }
    else if (strcmp(key, "l") == 0 || strcmp(key, "L") == 0) { game_runtime(self->ctx()).ray_triggered = true; return; }
    else if (strcmp(key, "q") == 0 || strcmp(key, "Q") == 0) { game_runtime(self->ctx()).weapon_switch_triggered = true; return; }
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
    else if (strcmp(key, "e") == 0 || strcmp(key, "E") == 0) { game_runtime(self->ctx()).bullet_triggered = true; return; }
    else if (strcmp(key, "l") == 0 || strcmp(key, "L") == 0) { game_runtime(self->ctx()).ray_triggered = true; return; }
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
        case GAMEPLAY_CONTROL_RANGED:
            game_runtime(ctx).bullet_triggered = true;
            break;
        case GAMEPLAY_CONTROL_RAY:
            game_runtime(ctx).ray_triggered = true;
            break;
        case GAMEPLAY_CONTROL_SWITCH:
            game_runtime(ctx).weapon_switch_triggered = true;
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
    gs.random_seed = (int)(get_ticks() ^ ((uintptr_t)&gs & 0xFFFF));
    gs.random = lcg_random_create((uint64_t)(uint32_t)gs.random_seed);
}

// ============================================================
// 本机操作层 + 音频（单机 / 客户端 / Host）
// ============================================================
void GameSceneCommon::initControlsAndAudio(GameScene* self)
{
    Context* ctx = self->ctx();
    self->gcl = new GameplayControlsLayer(ctx);   // 纯 button 操作层，直接挂在 gs_ 上由 GameScene 桥接

    ctx->audio_dev = audio_open_device();
    ctx->step_sound = audio_shot_create("joy_topdown_template/sounds/grass-step.wav", ctx->audio_dev);
    audio_shot_set_gain(ctx->step_sound, ctx->settings.sfx_volume);
    ctx->bgm = audio_bgm_create("joy_topdown_template/sounds/bgm.wav", ctx->audio_dev);
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
        snprintf(b, sizeof(b), "%d:%d:%d:%d:%d:",
                inputs[k].conv, inputs[k].dir,
                (int)inputs[k].attack, (int)inputs[k].ranged,
                (int)inputs[k].ray);
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
