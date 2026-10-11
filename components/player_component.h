#ifndef PLAYER_COMPONENT_H
#define PLAYER_COMPONENT_H

#include "../systems/character_catalog.h"
#include "../systems/weapon_catalog.h"

// 玩家武器库存：定长数组（不使用 std::vector），平凡可拷贝，
// ECS 快照 memcpy / 帧同步下保持一致。最多 8 把，空槽记 -1。
// weapon_index 指向当前选中下标；weapon_id 是 w_ids[weapon_index] 的镜像缓存
// （draw_held_weapon / 战斗系统仍读 weapon_id，无需改动）。
struct PlayerComponent
{
        int conv;
        // 角色 id：对应 character_catalog.h 运行时 catalog 的整数 id
        // （由 characters.json 中出现顺序分配，0 = 首个角色）
        int char_id = 0;

        int w_ids[8] = { -1 };   // 持有的武器 catalog id（与 weapons.json 出现顺序无关，仅存 id）
        int weapon_count = 0;    // 实际持有数量（≤ 8）
        int weapon_index = 0;    // 当前选中下标（0..weapon_count-1）

        // 当前武器 id：对应 weapon_catalog.h 运行时 catalog 的整数 id。-1 表示空手。
        int weapon_id = -1;
};

// 把角色授权的武器（key，如 "dagger"）写入库存；key 未登记进武器表则忽略。
inline void pc_grant_weapon(PlayerComponent& pc, const char* key)
{
        const WeaponDef* wd = g_weaponCatalog.findByName(key);
        if (!wd) return;
        if (pc.weapon_count < 8) pc.w_ids[pc.weapon_count++] = wd->id;
}

// 切到下一把持有的武器（固定顺序轮转）；库存 ≤1 把（含空手）时不移动。
inline void pc_select_next_weapon(PlayerComponent& pc)
{
        if (pc.weapon_count <= 1) return;
        pc.weapon_index = (pc.weapon_index + 1) % pc.weapon_count;
        pc.weapon_id = pc.w_ids[pc.weapon_index];
}

#endif