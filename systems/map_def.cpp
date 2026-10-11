// map_def.h 的可变共享全局定义。
// C++11 无命名空间 inline 变量，故头文件用 extern 声明、此处唯一定义一次，
// 所有 include map_def.h 的翻译单元都引用同一实例（帧同步数据须全局唯一）。
#include "map_def.h"

int MAP_W = 64;
int MAP_H = 64;

// 行主序 y*MAP_W+x；由 map_load_level 填充
char g_map_grid[MAP_MAX_CELLS];

MapLevelInfo g_map_info;