#ifndef SPAWN_POINT_COMPONENT_H
#define SPAWN_POINT_COMPONENT_H
#include <joy/jmath.h>

// 默认出生点（与 client handle_loading 发送的 join 位置一致：地图中央开阔地）
static const joy_fp_t DEFAULT_SPAWN_X = fp_from_float(15.5f);
static const joy_fp_t DEFAULT_SPAWN_Y = fp_from_float(10.5f);

// 出生点组件：实体出生/重生锚点（玩家加入位置、敌人固定出生点、死亡重生传送目标）。
// 与 HealthComponent 解耦：血量归零是"状态"，而出生点是"地图数据"，生命周期互不相关。
struct SpawnPointComponent {
    joy_fp_t x;
    joy_fp_t y;

    SpawnPointComponent() = default;
    SpawnPointComponent(joy_fp_t sx, joy_fp_t sy) : x(sx), y(sy) {}
};

#endif
