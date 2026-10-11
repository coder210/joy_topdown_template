#ifndef ENEMY_COMPONENT_H
#define ENEMY_COMPONENT_H

struct EnemyComponent {};
// 记录该敌人在 ENEMY_SPAWNS 中的索引，打包快照时写入 ai_type 字段
struct SpawnIdxComponent { int idx; };

#endif
