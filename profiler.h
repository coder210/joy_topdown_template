#ifndef PROFILER_H
#define PROFILER_H

#include <cstdio>
#include <chrono>
#include <cstdint>

// 逐系统毫秒剖析（仅统计 OnUpdate 系统，秒级打印到 stderr）。
// 纯观测用途：只负责计时/累计/定时清零，不参与调度，不影响帧同步逻辑。
// 调度循环仍由 run_phase（context.h）负责，二者职责分离。
//
// 纯 C++ 单例：构造私有、唯一实例经 SystemProfiler::instance() 获取；
// 各 run_phase 调用间共享累计状态，由成员函数封装全部操作。
class SystemProfiler {
public:
    // 每轮 OnUpdate 开始：索引归零，按系统调用顺序逐条 record
    void begin() { m_index = 0; }

    void record(double ms)
    {
        if (m_index < kMaxSystems) {
            m_ms[m_index] += ms;
            m_n[m_index]++;
            m_index++;
        }
    }

    // 距上次清零已满 1 秒则返回 true（调用方打印后应 reset）
    bool due(uint32_t now)
    {
        if (m_t0 == 0) m_t0 = now;
        return (now - m_t0) >= 1000;
    }

    void reset(uint32_t now)
    {
        for (int k = 0; k < kMaxSystems; k++) { m_ms[k] = 0.0; m_n[k] = 0; }
        m_t0 = now;
        m_index = 0;
    }

    // 供调用方读取累计结果
    int  count() const { return m_index; }
    long calls(int i) const { return m_n[i]; }
    double total_ms(int i) const { return m_ms[i]; }

    // 唯一实例（静态局部单例，线程初始化安全）
    static SystemProfiler& instance()
    {
        static SystemProfiler prof;
        return prof;
    }

private:
    SystemProfiler() {}
    SystemProfiler(const SystemProfiler&) = delete;
    SystemProfiler& operator=(const SystemProfiler&) = delete;

    static const int kMaxSystems = 128;   // 最多统计的系统数（远大于实际 OnUpdate 系统数）

    double   m_ms[SystemProfiler::kMaxSystems] = {0.0};
    long     m_n[SystemProfiler::kMaxSystems]  = {0};
    uint32_t m_t0 = 0;
    int      m_index = 0;
};

// 兼容快捷访问（等价于 SystemProfiler::instance()）
inline SystemProfiler& system_profiler() { return SystemProfiler::instance(); }

#endif