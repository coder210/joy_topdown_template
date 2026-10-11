#ifndef STATE_CHECKSUM_H
#define STATE_CHECKSUM_H
#include <joy/jmath.h>
#include <joy/joy.h>
#include <vector>
#include <algorithm>
#include <string>
#include <cstdio>
#include <cstring>

// 帧同步状态指纹：两端共用同一公式，保证 MD5 完全一致。
// 指纹来源：所有玩家 (id, LogicPosition.x, LogicPosition.y)。
struct ChecksumEntry
{
        int id;
        fp_t x, y;
};

// 计算玩家状态 MD5（hex 32 字符 + '\0' 写入 out，out 需 >= 33 字节）。
// 公式（两端必须逐字节一致）：
//   1. 按 id 升序排序
//   2. 拼接 "%d:%lld:%lld,"（坐标按 long long 取整）
//   3. utils_md5x
// 注意：此函数在"模拟本帧之前"调用，与服务端 collect_input 的时机一致。
static inline void compute_players_md5(std::vector<ChecksumEntry>& entries, char* out, int out_cap)
{
        std::sort(entries.begin(), entries.end(),
                [](const ChecksumEntry& a, const ChecksumEntry& b) {
                        return a.id < b.id;
                });

        std::string s;
        char buf[64];
        for (auto& e : entries) {
                int n = snprintf(buf, sizeof(buf), "%d:%lld:%lld,",
                        e.id, (long long)e.x, (long long)e.y);
                s.append(buf, n);
        }

        if (out_cap <= 0) return;
        memset(out, 0, out_cap);
        utils_md5x(s.c_str(), s.size(), out);
}

// 严格版状态指纹：把"所有带 IdComponent + LogicPositionComponent 的实体"（玩家 + 敌人）
// 的 (id, x, y) 都作为原料，检测范围比 compute_players_md5 更广——敌人位置/数量不一致
// 也能立刻暴露（例如中途加入者漏建/多建敌人、敌人 AI 推进错位等）。
// 公式与 compute_players_md5 完全一致（按 id 升序拼接），仅输入集合更大。
static inline void compute_full_md5(std::vector<ChecksumEntry>& entries, char* out, int out_cap)
{
        // 与玩家版共用同一排序/拼接/哈希公式，保证只在"集合"维度变严格。
        compute_players_md5(entries, out, out_cap);
}

// 把实体列表编成可读字符串 "id,x,y;id,x,y;..."（按 id 升序），用于校验记录落盘，
// 便于退出后 diff 出"多了/少了哪个玩家或敌人、位置差多少"。
static inline std::string checksum_entries_string(std::vector<ChecksumEntry>& entries)
{
        std::sort(entries.begin(), entries.end(),
                [](const ChecksumEntry& a, const ChecksumEntry& b) {
                        return a.id < b.id;
                });
        std::string s;
        char buf[64];
        for (auto& e : entries) {
                int n = snprintf(buf, sizeof(buf), "%d,%lld,%lld;",
                        e.id, (long long)e.x, (long long)e.y);
                s.append(buf, n);
        }
        return s;
}

// 帧同步校验记录（缓存用，退出时落盘）。两端共用同一结构，字段对齐便于 diff：
//   frame_id  逻辑帧号
//   checksum  本帧校验值（客户端填本地重算值；服务端填自己算的权威值）
//   inputs    本帧输入指纹 "conv:dir:atk:rng;..."（server 用本帧实际 inputs；client 用收到的 inputs）
//   entities  本帧所有玩家+敌人的 "id,x,y;..." 快照，用于定位集合/位置差异
// 注：不一致由客户端运行时用 printf 打到控制台，不进本记录；故只存一列 checksum。
struct ChecksumRecord {
        int      frame_id;
        char     checksum[64];
        std::string inputs;
        std::string entities;
};

#endif
