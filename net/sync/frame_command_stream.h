#ifndef NET_SYNC_FRAME_COMMAND_STREAM_H
#define NET_SYNC_FRAME_COMMAND_STREAM_H

#include <map>
#include <string>
#include <utility>

// ============================================================
// 帧同步"帧流缓冲"：把「帧号 → 已打包字节块」的存取、追帧策略、
// 容量裁剪集中到一处。
//
// 关键设计：本类完全不依赖具体协议/玩法——命令帧与世界快照都只是
// std::string 字节块。因此它可以直接复用到其它帧同步项目，
// 上层只需自己负责"字节块怎么打包/解包"。
// ============================================================
class FrameCommandStream {
public:
    // 只保留最近 300 帧（≈15s @20Hz）：防止无玩家时内存线性增长。
    // 补帧最多追 9 帧、新玩家对齐用 lower_bound 命中最近帧，300 帧完全够用。
    static const int kMaxBufferedFrames = 300;

    // ---- 命令帧（每 tick 一帧，广播 / 补发用）----
    void StoreCommand(int frame_id, std::string bytes)
    {
        commands_[frame_id] = std::move(bytes);
        Trim(commands_);
    }

    // 未命中返回 nullptr。
    const std::string* FindCommand(int frame_id) const
    {
        auto it = commands_.find(frame_id);
        return it == commands_.end() ? nullptr : &it->second;
    }

    // ---- 世界快照（中途加入对齐用）----
    void StoreSnapshot(int frame_id, std::string bytes)
    {
        snapshots_[frame_id] = std::move(bytes);
        Trim(snapshots_);
    }

    // 取 <= frame_id 的最近快照；out_frame 回传该快照所属帧号。
    // 返回 nullptr 表示没有任何可用快照（调用方应回退到空 loading）。
    const std::string* FindSnapshotAtOrBefore(int frame_id, int* out_frame) const
    {
        if (snapshots_.empty()) return nullptr;

        auto it = snapshots_.lower_bound(frame_id);
        if (it == snapshots_.end() || it->first > frame_id) {
            if (it == snapshots_.begin()) return nullptr;   // 所有快照都晚于 frame_id
            --it;
        }
        if (out_frame) *out_frame = it->first;
        return &it->second;
    }

    // ---- 追帧策略 ----
    // 落后越多追得越快（2 / 4 / 9 帧每次），返回本 tick 应发到的目标帧。
    // 注意：这是网络策略，不属于玩法逻辑。
    static int TargetFrameId(int current_frame, int server_frame)
    {
        int diff = server_frame - current_frame;
        if (diff >= 50) return current_frame + 9;
        if (diff >= 30) return current_frame;      // 落后过多时先不追，避免带宽雪崩
        if (diff >= 10) return current_frame + 4;
        if (diff >= 2)  return current_frame + 2;
        return current_frame;
    }

    void Clear() { commands_.clear(); snapshots_.clear(); }
    size_t CommandCount() const { return commands_.size(); }
    size_t SnapshotCount() const { return snapshots_.size(); }

private:
    static void Trim(std::map<int, std::string>& m)
    {
        while ((int)m.size() > kMaxBufferedFrames)
            m.erase(m.begin());   // map 按帧号有序，begin() 即最旧帧
    }

    std::map<int, std::string> commands_;
    std::map<int, std::string> snapshots_;
};

#endif
