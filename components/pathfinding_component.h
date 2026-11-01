#ifndef PATHFINDING_COMPONENT_H
#define PATHFINDING_COMPONENT_H

#include <joy/jmath.h>

/* 敌人寻路状态（纯数据）
 *  - ai_system 在追击时写入 target_gx/gy 并置 dirty
 *  - pathfinding_system 按节拍重算 A*，把结果写入 path[]，并沿路径移动
 *  - 只存在于带 EnemyComponent 的实体上（由 ai_system 懒创建）
 */
struct PathComponent {
    int  target_gx = 0;        // 目标格子坐标（世界位置取整）
    int  target_gy = 0;
    bool dirty = false;        // 需要重算路径
    int  path[1024];           // 路径格子索引序列 (y*MAP_W + x)
    int  path_len = 0;
    int  next_idx = 0;         // 下一个要走的路径点下标
    int  recompute_cooldown = 0; // 重算节拍（帧数）
};

#endif // PATHFINDING_COMPONENT_H
