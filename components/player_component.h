#ifndef PLAYER_COMPONENT_H
#define PLAYER_COMPONENT_H

#include "../systems/character_catalog.h"
#include "../systems/weapon_catalog.h"

// 玩家持有的武器：由角色类型（characters.json 的 weapon 字段）决定，单一固定，无切换。
// 武器切换功能已移除——不同角色类型持有其对应的武器。
// weapon_id 对应 weapon_catalog.h 运行时 catalog 的整数 id；-1 表示空手。
// 渲染（draw_held_weapon）与战斗系统按 weapon_id 查 kind（melee/ranged）判定行为。
struct PlayerComponent
{
        int conv;
        // 角色 id：对应 character_catalog.h 运行时 catalog 的整数 id
        // （由 characters.json 中出现顺序分配，0 = 首个角色）
        int char_id = 0;

        // 当前武器 id：对应 weapon_catalog.h 运行时 catalog 的整数 id。-1 表示空手。
        int weapon_id = -1;
};

// 按角色对应的武器 key（如 "sword"）设置武器；key 未登记进武器表则置空手（-1）。
inline void pc_set_weapon(PlayerComponent& pc, const char* key)
{
        if (!key || !key[0]) { pc.weapon_id = -1; return; }
        const WeaponDef* wd = g_weaponCatalog.findByName(key);
        pc.weapon_id = wd ? wd->id : -1;
}

#endif