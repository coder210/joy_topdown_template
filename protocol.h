#pragma once
/* 冒险游戏网络协议（基于 jmsgpack）
 * Server ↔ Client 帧同步协议定义
 * 枚举、结构体、打包/解包工具函数 */
#include <joy/joy.h>
#include <cstring>
#include <cstdlib>
#include <type_traits>

/* ================================ 常量 ================================ */
/* PIXELS_PER_METER 现由 joy/joy.h 统一提供，不再在此重复定义。 */

/* 输入位掩码 */
enum {
    INPUT_UP     = 1 << 0,
    INPUT_DOWN   = 1 << 1,
    INPUT_LEFT   = 1 << 2,
    INPUT_RIGHT  = 1 << 3,
    INPUT_ATTACK = 1 << 4,
    INPUT_RANGED = 1 << 5,
    INPUT_RAY    = 1 << 6,   // 射线大招（hitscan，玩家专属）
    INPUT_DIR_MASK = INPUT_UP | INPUT_DOWN | INPUT_LEFT | INPUT_RIGHT,
};

/* C2S 命令 */
enum C2SCmd {
    CMD_UNKNOWN       = 0,
    CMD_PLAYER_JOIN   = 1,
    CMD_PLAYER_LEAVE  = 2,
    CMD_PLAYER_INPUT  = 3,
    CMD_PLAYER_HEART  = 4,
    CMD_LOADING       = 5,
};

/* S2C 命令 */
enum S2CCmd {
    S2C_CMD_NONE    = 0,
    S2C_CMD_LOADING = 1,
    S2C_CMD_COMMAND = 2,
};

/* S2C 实体类型 */
enum S2CType {
    S2C_TYPE_NORMAL = 0,
    S2C_TYPE_PLAYER = 1,
    S2C_TYPE_ENEMY  = 2,
};

/* ============================ 协议结构体 ============================ */
struct S2CPlayerJoin  { int32_t conv; int64_t position_x, position_y; int32_t char_id; };
struct S2CPlayerLeave { int32_t conv; };
struct S2CPlayerInput { uint32_t client_seq; int32_t conv, dir;  int8_t attack; int8_t ranged; int8_t ray; int8_t weapon_switch; };
struct S2CEntity {
    int32_t id, type;
    int64_t position_x, position_y, width, height, hp;
    int32_t player_conv;
    int32_t player_char_id;
    int64_t ai_type;       // AI 类型（SpawnDef 索引）
    // AI 运行时状态（仅敌人有意义，客户端接入时恢复，避免持续累积的 AI 状态起点不同步）
    int32_t ai_timer;          // 巡逻唤醒倒计时
    int32_t ai_state;          // 0=巡逻, 1=追击, 2=攻击
    int64_t ai_target_x;       // 当前移动目标
    int64_t ai_target_y;
    int32_t ai_shoot_cooldown; // 远程射击冷却
};

/* ======================== 辅助函数 ======================== */

/* 从 keycode 提取方向索引 (0=下, 1=左, 2=右, 3=上) */
static inline int input_to_dir(int keycode) {
    if (keycode & INPUT_DOWN)  return 0;
    if (keycode & INPUT_LEFT)  return 1;
    if (keycode & INPUT_RIGHT) return 2;
    if (keycode & INPUT_UP)    return 3;
    return 0;
}
static inline bool has_dir(int keycode) { return (keycode & INPUT_DIR_MASK) != 0; }

/* 从 jmsgpack 对象中提取有符号整数 */
static inline int64_t jmp_int(const jmsgpack_object_t* o) {
    if (o->type == JMSGPACK_OBJECT_POSITIVE_INTEGER) return (int64_t)o->via.u64;
    if (o->type == JMSGPACK_OBJECT_NEGATIVE_INTEGER) return o->via.i64;
    return 0;
}

/* ======================== msgpack 打包 ======================== */

/* 回写回调：写入 std::string */
static int mp_write_cb(void* data, const char* buf, size_t len) {
    ((std::string*)data)->append(buf, len);
    return 0;
}

/* --- 子对象打包 --- */
static inline void pack_entity(jmsgpack_packer_t* pk, const S2CEntity& e) {
    jmsgpack_pack_array(pk, 15);
    jmsgpack_pack_int(pk, e.id);    jmsgpack_pack_int(pk, e.type);
    jmsgpack_pack_int64(pk, e.position_x); jmsgpack_pack_int64(pk, e.position_y);
    jmsgpack_pack_int64(pk, e.width);      jmsgpack_pack_int64(pk, e.height);
    jmsgpack_pack_int64(pk, e.hp);  jmsgpack_pack_int(pk, e.player_conv);
    jmsgpack_pack_int(pk, e.player_char_id);
    jmsgpack_pack_int64(pk, e.ai_type);
    jmsgpack_pack_int(pk, e.ai_timer);  jmsgpack_pack_int(pk, e.ai_state);
    jmsgpack_pack_int64(pk, e.ai_target_x); jmsgpack_pack_int64(pk, e.ai_target_y);
    jmsgpack_pack_int(pk, e.ai_shoot_cooldown);
}
static inline void pack_join(jmsgpack_packer_t* pk, const S2CPlayerJoin& j) {
    jmsgpack_pack_array(pk, 4);
    jmsgpack_pack_int(pk, j.conv);
    jmsgpack_pack_int64(pk, j.position_x);
    jmsgpack_pack_int64(pk, j.position_y);
    jmsgpack_pack_int(pk, j.char_id);
}
static inline void pack_leave(jmsgpack_packer_t* pk, const S2CPlayerLeave& l) {
    jmsgpack_pack_array(pk, 1);
    jmsgpack_pack_int(pk, l.conv);
}
static inline void pack_input(jmsgpack_packer_t* pk, const S2CPlayerInput& i) {
    jmsgpack_pack_array(pk, 6);
    jmsgpack_pack_int(pk, i.conv);
    jmsgpack_pack_int(pk, i.dir);
    jmsgpack_pack_int(pk, i.attack);
    jmsgpack_pack_int(pk, i.ranged);
    jmsgpack_pack_int(pk, i.ray);
    jmsgpack_pack_int(pk, i.weapon_switch);
}

/* C2S 打包: [cmd, payload] */
static inline std::string pack_c2s(int cmd, int64_t join_x, int64_t join_y, int input_dir, int input_attack, int input_ranged = 0, int input_ray = 0, int input_weapon_switch = 0, int char_id = 0) {
    std::string out;
    jmsgpack_packer_t pk;
    jmsgpack_packer_init(&pk, &out, mp_write_cb);
    jmsgpack_pack_array(&pk, 2);
    jmsgpack_pack_int(&pk, cmd);
    if (cmd == CMD_PLAYER_JOIN) {
        jmsgpack_pack_array(&pk, 3);
        jmsgpack_pack_int64(&pk, join_x);
        jmsgpack_pack_int64(&pk, join_y);
        jmsgpack_pack_int(&pk, char_id);
    } else if (cmd == CMD_PLAYER_INPUT) {
        jmsgpack_pack_array(&pk, 5);
        jmsgpack_pack_int(&pk, input_dir);
        jmsgpack_pack_int(&pk, input_attack);
        jmsgpack_pack_int(&pk, input_ranged);
        jmsgpack_pack_int(&pk, input_ray);
        jmsgpack_pack_int(&pk, input_weapon_switch);
    } else {
        jmsgpack_pack_nil(&pk);
    }
    return out;
}

/* S2C 世界快照打包: [world_eid, [entity...]] */
static inline std::string pack_world(int entity_id, const S2CEntity* entities, int count) {
    std::string out;
    jmsgpack_packer_t pk;
    jmsgpack_packer_init(&pk, &out, mp_write_cb);
    jmsgpack_pack_array(&pk, 2);
    jmsgpack_pack_int(&pk, entity_id);
    jmsgpack_pack_array(&pk, count);
    for (int i = 0; i < count; i++)
        pack_entity(&pk, entities[i]);
    return out;
}

/* S2C loading 打包: [S2C_CMD_LOADING, [conv, frame_id, [world_eid, [entity...]], seed, random_state]]
 * seed: 确定性随机种子；random_state: 服务端当前 LCG 单状态（64 位）。
 * 仅同步 seed 会让客户端 random 回到初始状态，而服务端已推进，导致 AI 随机序列分叉，
 * 故须同步当前状态（LCG 仅 1 个 uint64，同步量最小）。 */
static inline std::string pack_s2c_loading(int32_t conv, int32_t frame_id, int32_t world_eid,
    const S2CEntity* entities, int ec, int32_t seed,
    int64_t random_state)
{
    std::string out;
    jmsgpack_packer_t pk;
    jmsgpack_packer_init(&pk, &out, mp_write_cb);
    jmsgpack_pack_array(&pk, 2);
    jmsgpack_pack_int(&pk, S2C_CMD_LOADING);
    jmsgpack_pack_array(&pk, 5);
    jmsgpack_pack_int(&pk, conv);
    jmsgpack_pack_int(&pk, frame_id);
    jmsgpack_pack_array(&pk, 2);
    jmsgpack_pack_int(&pk, world_eid);
    jmsgpack_pack_array(&pk, ec);
    for (int i = 0; i < ec; i++)
        pack_entity(&pk, entities[i]);
    jmsgpack_pack_int(&pk, seed);
    jmsgpack_pack_int64(&pk, random_state);
    return out;
}

/* S2C command 打包（服务端用）: [S2C_CMD_COMMAND, [frame_id, joins, leaves, inputs, checksum]] */
static inline std::string pack_s2c(int16_t cmd, int32_t frame_id,
    const S2CPlayerJoin* joins, int jn,
    const S2CPlayerLeave* leaves, int ln,
    const S2CPlayerInput* inputs, int in,
    const char* checksum)
{
    std::string out;
    jmsgpack_packer_t pk;
    jmsgpack_packer_init(&pk, &out, mp_write_cb);
    jmsgpack_pack_array(&pk, 2);
    jmsgpack_pack_int(&pk, cmd);
    if (cmd == S2C_CMD_COMMAND) {
        jmsgpack_pack_array(&pk, 5);
        jmsgpack_pack_int(&pk, frame_id);
        jmsgpack_pack_array(&pk, jn);
        for (int i = 0; i < jn; i++) pack_join(&pk, joins[i]);
        jmsgpack_pack_array(&pk, ln);
        for (int i = 0; i < ln; i++) pack_leave(&pk, leaves[i]);
        jmsgpack_pack_array(&pk, in);
        for (int i = 0; i < in; i++) pack_input(&pk, inputs[i]);
        jmsgpack_pack_str(&pk, checksum ? strlen(checksum) : 0);
        if (checksum) jmsgpack_pack_str_body(&pk, checksum, strlen(checksum));
    } else {
        jmsgpack_pack_nil(&pk);
    }
    return out;
}

/* ======================== msgpack 解包 ======================== */

/* 解包单个实体 */
static inline void unpack_entity(const jmsgpack_object_t* o, S2CEntity* e) {
    if (!o || o->type != JMSGPACK_OBJECT_ARRAY || o->via.array.size < 10) return;
    e->id         = (int)jmp_int(&o->via.array.ptr[0]);
    e->type       = (int)jmp_int(&o->via.array.ptr[1]);
    e->position_x = jmp_int(&o->via.array.ptr[2]);
    e->position_y = jmp_int(&o->via.array.ptr[3]);
    e->width      = jmp_int(&o->via.array.ptr[4]);
    e->height     = jmp_int(&o->via.array.ptr[5]);
    e->hp         = jmp_int(&o->via.array.ptr[6]);
    e->player_conv = (int)jmp_int(&o->via.array.ptr[7]);
    e->player_char_id = (int)jmp_int(&o->via.array.ptr[8]);
    e->ai_type    = jmp_int(&o->via.array.ptr[9]);
    e->ai_timer   = (int)jmp_int(&o->via.array.ptr[10]);
    e->ai_state   = (int)jmp_int(&o->via.array.ptr[11]);
    e->ai_target_x = jmp_int(&o->via.array.ptr[12]);
    e->ai_target_y = jmp_int(&o->via.array.ptr[13]);
    e->ai_shoot_cooldown = (int)jmp_int(&o->via.array.ptr[14]);
}

/* 解包 S2C 世界快照: [eid, [entity...]]，返回实体数 */
static inline int unpack_world(const char* data, int len, int* eid, S2CEntity* entities, int max_ec) {
    jmsgpack_unpacked_t r;
    jmsgpack_unpacked_init(&r);
    size_t off = 0;
    if (jmsgpack_unpack_next(&r, data, len, &off) < 0) { jmsgpack_unpacked_destroy(&r); return 0; }
    if (r.data.type != JMSGPACK_OBJECT_ARRAY || r.data.via.array.size < 2) { jmsgpack_unpacked_destroy(&r); return 0; }
    *eid = (int)jmp_int(&r.data.via.array.ptr[0]);
    auto* arr = &r.data.via.array.ptr[1];
    int n = (int)arr->via.array.size;
    if (n > max_ec) n = max_ec;
    for (int i = 0; i < n; i++)
        unpack_entity(&arr->via.array.ptr[i], &entities[i]);
    jmsgpack_unpacked_destroy(&r);
    return n;
}

/* 解包 S2C loading 消息: 返回实体数，通过出参返回 conv/frame_id/eid/seed，
 * 以及 random 当前状态（single int64 random_state，LCG 单状态） */
static inline int unpack_loading(const char* data, int len,
    int* conv, int* frame_id, int* eid, int* seed,
    int64_t* random_state,
    S2CEntity* entities, int max_ec)
{
    jmsgpack_unpacked_t r;
    jmsgpack_unpacked_init(&r);
    size_t off = 0;
    if (jmsgpack_unpack_next(&r, data, len, &off) < 0) { jmsgpack_unpacked_destroy(&r); return -1; }
    auto* pl = &r.data.via.array.ptr[1];
    if (pl->type != JMSGPACK_OBJECT_ARRAY || pl->via.array.size < 4) { jmsgpack_unpacked_destroy(&r); return -1; }
    if (conv)     *conv     = (int)jmp_int(&pl->via.array.ptr[0]);
    if (frame_id) *frame_id = (int)jmp_int(&pl->via.array.ptr[1]);
    auto* w = &pl->via.array.ptr[2];
    if (w->type != JMSGPACK_OBJECT_ARRAY || w->via.array.size < 2) { jmsgpack_unpacked_destroy(&r); return -1; }
    if (eid) *eid = (int)jmp_int(&w->via.array.ptr[0]);
    auto* arr = &w->via.array.ptr[1];
    int n = (int)arr->via.array.size;
    if (n > max_ec) n = max_ec;
    for (int i = 0; i < n; i++)
        unpack_entity(&arr->via.array.ptr[i], &entities[i]);
    if (seed) *seed = (int)jmp_int(&pl->via.array.ptr[3]);
    // 第 5 个元素：random_state（LCG 单状态 int64）
    if (random_state && pl->via.array.size >= 5)
        *random_state = jmp_int(&pl->via.array.ptr[4]);
    jmsgpack_unpacked_destroy(&r);
    return n;
}

/* 读取一条 command payload 数组元素（C++11 重载分派，替代 C++14 泛型 lambda + C++17 if constexpr）
 * loader_type 仅用于模板分派；实际按 T 调用的重载读取字段。 */
template <typename T>
static inline void unpack_command_read_one(T& d, const jmsgpack_object_t* o);

static inline void unpack_command_read_one(S2CPlayerJoin& d, const jmsgpack_object_t* o) {
    if (o->type == JMSGPACK_OBJECT_ARRAY && o->via.array.size >= 4) {
        d.conv = (int)jmp_int(&o->via.array.ptr[0]);
        d.position_x = jmp_int(&o->via.array.ptr[1]);
        d.position_y = jmp_int(&o->via.array.ptr[2]);
        d.char_id = (int)jmp_int(&o->via.array.ptr[3]);
    }
}
static inline void unpack_command_read_one(S2CPlayerLeave& d, const jmsgpack_object_t* o) {
    if (o->type == JMSGPACK_OBJECT_ARRAY && o->via.array.size >= 1) {
        d.conv = (int)jmp_int(&o->via.array.ptr[0]);
    }
}
static inline void unpack_command_read_one(S2CPlayerInput& d, const jmsgpack_object_t* o) {
    if (o->type == JMSGPACK_OBJECT_ARRAY && o->via.array.size >= 6) {
        d.conv = (int)jmp_int(&o->via.array.ptr[0]);
        d.dir = (int)jmp_int(&o->via.array.ptr[1]);
        d.attack = (int8_t)jmp_int(&o->via.array.ptr[2]);
        d.ranged = (int8_t)jmp_int(&o->via.array.ptr[3]);
        d.ray = (int8_t)jmp_int(&o->via.array.ptr[4]);
        d.weapon_switch = (int8_t)jmp_int(&o->via.array.ptr[5]);
    }
}

/* 从 command payload 读取一个子数组（idx: joins=1 / leaves=2 / inputs=3） */
template <typename T>
static inline void read_command_payload(const jmsgpack_object_t* pl, int idx, T* out,
        int max, int& cnt)
{
    if (idx >= (int)pl->via.array.size) return;
    const jmsgpack_object_t* a = &pl->via.array.ptr[idx];
    if (a->type != JMSGPACK_OBJECT_ARRAY) return;
    int n = (int)a->via.array.size;
    if (n > max) n = max;
    cnt = n;
    for (int i = 0; i < n; i++) {
        const jmsgpack_object_t* o = &a->via.array.ptr[i];
        if (o->type != JMSGPACK_OBJECT_ARRAY) continue;
        unpack_command_read_one(out[i], o);
    }
}

/* 解包 S2C command 消息（通用版，服务端/客户端均可用）
 * 返回 1 成功，0 失败。
 * 通过出参返回 frame_id / joins / leaves / inputs / checksum */
static inline int unpack_command(const char* data, int len,
    int* frame_id,
    S2CPlayerJoin* joins, int max_jn, int* jn,
    S2CPlayerLeave* leaves, int max_ln, int* ln,
    S2CPlayerInput* inputs, int max_in, int* in,
    char* checksum = nullptr, int checksum_cap = 0)
{
    if (jn)  memset(joins,  0, max_jn * sizeof(S2CPlayerJoin));
    if (ln)  memset(leaves, 0, max_ln * sizeof(S2CPlayerLeave));
    if (in)  memset(inputs, 0, max_in * sizeof(S2CPlayerInput));
    if (jn)  *jn = 0;
    if (ln)  *ln = 0;
    if (in)  *in = 0;

    jmsgpack_unpacked_t r;
    jmsgpack_unpacked_init(&r);
    size_t off = 0;
    if (jmsgpack_unpack_next(&r, data, len, &off) < 0) { jmsgpack_unpacked_destroy(&r); return 0; }
    if (r.data.type != JMSGPACK_OBJECT_ARRAY || r.data.via.array.size < 2) { jmsgpack_unpacked_destroy(&r); return 0; }
    auto* pl = &r.data.via.array.ptr[1];
    if (pl->type != JMSGPACK_OBJECT_ARRAY || pl->via.array.size < 5) { jmsgpack_unpacked_destroy(&r); return 0; }

    if (frame_id) *frame_id = (int)jmp_int(&pl->via.array.ptr[0]);

    read_command_payload(pl, 1, joins,  max_jn, *jn);
    read_command_payload(pl, 2, leaves, max_ln, *ln);
    read_command_payload(pl, 3, inputs, max_in, *in);

    // 第 5 个元素：状态指纹 checksum（服务端打包，客户端校验）
    if (checksum && checksum_cap > 0) {
        checksum[0] = '\0';
        if (pl->via.array.size >= 5) {
            auto* c = &pl->via.array.ptr[4];
            if (c->type == JMSGPACK_OBJECT_STR && c->via.str.ptr) {
                int n = (int)c->via.str.size;
                if (n > checksum_cap - 1) n = checksum_cap - 1;
                memcpy(checksum, c->via.str.ptr, n);
                checksum[n] = '\0';
            }
        }
    }

    jmsgpack_unpacked_destroy(&r);
    return 1;
}

/* 解包 C2S 消息: [cmd, payload]
 * 返回 1 成功，0 失败。
 * 通过出参返回 cmd / join_x, join_y / input_dir, input_attack */
static inline int unpack_c2s(const char* data, int len,
    int* cmd, int64_t* join_x, int64_t* join_y, int* input_dir, int* input_attack,
    int* input_ranged = nullptr, int* input_ray = nullptr, int* char_id = nullptr, int* input_weapon_switch = nullptr)
{
    jmsgpack_unpacked_t r;
    jmsgpack_unpacked_init(&r);
    size_t off = 0;
    if (jmsgpack_unpack_next(&r, data, len, &off) < 0) { jmsgpack_unpacked_destroy(&r); return 0; }
    if (r.data.type != JMSGPACK_OBJECT_ARRAY || r.data.via.array.size < 2) { jmsgpack_unpacked_destroy(&r); return 0; }
    *cmd = (int)jmp_int(&r.data.via.array.ptr[0]);
    auto* payload = &r.data.via.array.ptr[1];
    if (payload->type == JMSGPACK_OBJECT_ARRAY) {
        switch (*cmd) {
        case CMD_PLAYER_JOIN:
            if (payload->via.array.size >= 2) {
                *join_x = jmp_int(&payload->via.array.ptr[0]);
                *join_y = jmp_int(&payload->via.array.ptr[1]);
            }
            if (payload->via.array.size >= 3 && char_id)
                *char_id = (int)jmp_int(&payload->via.array.ptr[2]);
            break;
        case CMD_PLAYER_INPUT:
            if (payload->via.array.size >= 2) {
                *input_dir = (int)jmp_int(&payload->via.array.ptr[0]);
                *input_attack = (int)jmp_int(&payload->via.array.ptr[1]);
            }
            if (payload->via.array.size >= 3 && input_ranged)
                *input_ranged = (int)jmp_int(&payload->via.array.ptr[2]);
            if (payload->via.array.size >= 4 && input_ray)
                *input_ray = (int)jmp_int(&payload->via.array.ptr[3]);
            if (payload->via.array.size >= 5 && input_weapon_switch)
                *input_weapon_switch = (int)jmp_int(&payload->via.array.ptr[4]);
            break;
        }
    }
    jmsgpack_unpacked_destroy(&r);
    return 1;
}


