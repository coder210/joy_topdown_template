#ifndef SPRITE_ANIM_H
#define SPRITE_ANIM_H
#include <joy/joy.h>
#include <string>
#include <vector>

/* ============================================================
 * 精灵序列帧状态机（纯渲染层，不进快照 / 不参与帧同步）
 *
 * 为什么在模板里自己实现：新版 joy 的动画 API 已从「栈上 anim_def_t + anim_t」
 * 改为不透明句柄 joy_scene_animator_p，且只提供「原坐标原尺寸直接画」
 * (joy_scene_animator_draw)，不再暴露当前 clip 的 src_rect / 图片路径。
 * 本模板的玩家贴图需要按 1 米缩放、原点居中、水平镜像，并且要与武器图层
 * 前后穿插，因此保留一份最小的本地状态机，行为与旧 janim 保持一致：
 *   - 状态 / clip（图路径 + 源矩形 + 时长）/ 默认态 / 过渡（按参数比较或 trigger）
 *   - 过渡可设 exit_time（非循环态播满即切）
 *   - 非循环态播到末帧停住（死亡态由外部强制切入）
 * ============================================================ */

static const int SPRITE_ANIM_MAX_CLIPS = 16;   // 单状态最大帧数（精灵表横向帧数上限）

enum SpriteAnimCond {
        SPRITE_COND_GREATER = 0,   // param > threshold
        SPRITE_COND_LESS,          // param < threshold
        SPRITE_COND_EQUALS,        // param == threshold（float 带 epsilon）
        SPRITE_COND_NOT_EQUALS,
        SPRITE_COND_IF_TRUE,       // bool 参数为真
        SPRITE_COND_IF_FALSE,
        SPRITE_COND_TRIGGER,       // trigger 被置位
};

struct SpriteClip {
        std::string path;   // 运行时相对路径（纹理由 AssetManager 缓存）
        joy_rect_t  src = { 0.0f, 0.0f, 0.0f, 0.0f };
        float       duration = 0.1f;   // 本帧时长（秒）
};

struct SpriteState {
        std::string            name;
        bool                   loop = false;
        std::vector<SpriteClip> clips;
};

struct SpriteTransition {
        int         from = -1;     // -1 = 任意态（Any State）
        int         to = -1;
        std::string param;
        int         cond = SPRITE_COND_TRIGGER;
        float       threshold = 0.0f;
        float       exit_time = -1.0f;   // >=0 时：当前态播满该时长即切（不看条件）
        bool        cond_ok = false;
};

struct SpriteAnimDef {
        std::vector<SpriteState>      states;
        std::vector<SpriteTransition> trans;
        int                           default_state = -1;

        // 追加状态，返回下标（= 状态 id）
        int add_state(const char* name, bool loop)
        {
                SpriteState s;
                s.name = name;
                s.loop = loop;
                states.push_back(s);
                return (int)states.size() - 1;
        }

        void add_clip(int state, const char* path, float duration, const joy_rect_t& src)
        {
                if (state < 0 || state >= (int)states.size()) return;
                SpriteClip c;
                c.path = path ? path : "";
                c.src = src;
                c.duration = duration > 0.0f ? duration : 0.1f;
                states[(size_t)state].clips.push_back(c);
        }

        void set_default_state(int state) { default_state = state; }

        // 追加过渡，返回下标；from = -1 表示任意态
        int add_transition(int from, int to)
        {
                SpriteTransition t;
                t.from = from;
                t.to = to;
                trans.push_back(t);
                return (int)trans.size() - 1;
        }

        void add_cond(int trans_idx, const char* param, int cond, float threshold)
        {
                if (trans_idx < 0 || trans_idx >= (int)trans.size()) return;
                SpriteTransition& t = trans[(size_t)trans_idx];
                t.param = param ? param : "";
                t.cond = cond;
                t.threshold = threshold;
        }

        void set_exit_time(int trans_idx, float seconds)
        {
                if (trans_idx < 0 || trans_idx >= (int)trans.size()) return;
                trans[(size_t)trans_idx].exit_time = seconds;
        }
};

// 过渡条件读取的参数集合（与旧 janim 的 params 一致）
struct SpriteAnimParams {
        float speed = 0.0f;
        int   dir = 0;
        bool  attack = false;
        bool  knocked = false;
};

// 每实体运行时状态（挂在 ECS 组件上）
struct SpriteAnim {
        int   state = -1;
        int   frame = 0;
        float frame_elapsed = 0.0f;
        float state_elapsed = 0.0f;
        bool  flip_h = false;
};

inline float sprite_param_value(const SpriteAnimParams& p, const std::string& name)
{
        if (name == "speed")   return p.speed;
        if (name == "dir")     return (float)p.dir;
        if (name == "attack")  return p.attack ? 1.0f : 0.0f;
        if (name == "knocked") return p.knocked ? 1.0f : 0.0f;
        return 0.0f;
}

inline bool sprite_cond_ok(const SpriteTransition& t, const SpriteAnimParams& p)
{
        const float v = sprite_param_value(p, t.param);
        const float eps = 1e-5f;
        switch (t.cond) {
        case SPRITE_COND_GREATER:    return v > t.threshold;
        case SPRITE_COND_LESS:       return v < t.threshold;
        case SPRITE_COND_EQUALS:     return (v > t.threshold ? v - t.threshold : t.threshold - v) <= eps;
        case SPRITE_COND_NOT_EQUALS: return (v > t.threshold ? v - t.threshold : t.threshold - v) > eps;
        case SPRITE_COND_IF_TRUE:    return v != 0.0f;
        case SPRITE_COND_IF_FALSE:   return v == 0.0f;
        case SPRITE_COND_TRIGGER:    return v != 0.0f;   // trigger 由调用方按帧边沿给出
        default:                     return false;
        }
}

inline void sprite_anim_enter(SpriteAnim& a, const SpriteAnimDef& def, int state)
{
        if (state < 0 || state >= (int)def.states.size()) return;
        if (a.state == state) return;
        a.state = state;
        a.frame = 0;
        a.frame_elapsed = 0.0f;
        a.state_elapsed = 0.0f;
}

inline void sprite_anim_update(SpriteAnim& a, const SpriteAnimDef& def,
                               const SpriteAnimParams& p, float dt)
{
        if (def.states.empty()) return;
        if (a.state < 0 || a.state >= (int)def.states.size())
                sprite_anim_enter(a, def, def.default_state >= 0 ? def.default_state : 0);
        if (a.state < 0 || a.state >= (int)def.states.size()) return;

        a.state_elapsed += dt;

        // 1) 任意态过渡（优先级高）
        for (const SpriteTransition& t : def.trans) {
                if (t.from != -1 || t.to == a.state) continue;
                if (t.to < 0 || t.to >= (int)def.states.size()) continue;
                if (sprite_cond_ok(t, p)) { sprite_anim_enter(a, def, t.to); return; }
        }

        // 2) 当前态出发的过渡：设了 exit_time 的按时间切，其余按条件切
        for (const SpriteTransition& t : def.trans) {
                if (t.from != a.state) continue;
                if (t.to < 0 || t.to >= (int)def.states.size()) continue;
                const bool by_time = t.exit_time >= 0.0f && a.state_elapsed >= t.exit_time;
                if (by_time || (t.exit_time < 0.0f && sprite_cond_ok(t, p))) {
                        sprite_anim_enter(a, def, t.to);
                        return;
                }
        }

        // 3) 推进帧
        const SpriteState& st = def.states[(size_t)a.state];
        if (st.clips.empty()) return;
        if (a.frame < 0 || a.frame >= (int)st.clips.size()) a.frame = 0;

        a.frame_elapsed += dt;
        for (;;) {
                const float dur = st.clips[(size_t)a.frame].duration;
                if (a.frame_elapsed < dur) break;
                a.frame_elapsed -= dur;
                if (a.frame + 1 < (int)st.clips.size()) {
                        a.frame++;
                } else if (st.loop) {
                        a.frame = 0;
                } else {
                        a.frame_elapsed = 0.0f;   // 非循环态停在末帧
                        break;
                }
        }
}

inline const SpriteClip* sprite_anim_current_clip(const SpriteAnim& a, const SpriteAnimDef& def)
{
        if (a.state < 0 || a.state >= (int)def.states.size()) return nullptr;
        const SpriteState& st = def.states[(size_t)a.state];
        if (st.clips.empty()) return nullptr;
        if (a.frame < 0 || a.frame >= (int)st.clips.size()) return &st.clips[0];
        return &st.clips[(size_t)a.frame];
}

#endif // SPRITE_ANIM_H
