#ifndef LOCAL_ONLY_COMPONENT_H
#define LOCAL_ONLY_COMPONENT_H

// 本地专用实体标记：不进入快照、不跨网络同步。
// 用于地图碰撞体等两端各自本地生成、无需同步的实体。
struct LocalOnlyComponent {};

#endif
