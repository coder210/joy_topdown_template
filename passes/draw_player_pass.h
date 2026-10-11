#ifndef DRAW_PLAYER_PASS_H
#define DRAW_PLAYER_PASS_H
#include <joy/joy.h>
#include <joy/janim.h>          /* joy 原生帧动画状态机：支持精灵表裁剪 + 状态过渡 */
#include <string>
#include <unordered_map>
#include <cstring>
#include "../components/global_state_component.h"
#include "../components/id_component.h"
#include "../components/player_component.h"
#include "../components/action_state_component.h"
#include "../components/health_component.h"
#include "../components/transform_component.h"
#include "../systems/character_catalog.h"
#include "../systems/weapon_catalog.h"
#include "../components/anim_state_component.h"
#include "../asset_manager.h"
#include "../protocol.h"

struct Context;

/* ============================================================
 * 玩家序列帧渲染（characters.json 驱动）
 *  - 动画定义/播放用 joy 的 janim（anim_def_t + anim_t + anim_update）
 *  - 素材为横向等帧精灵表，帧宽 = 图高，帧数 = 图宽 / 图高（自动探测）
 *  - 纯渲染层：只写 anim_t，绝不回写 LogicPosition/Health 等逻辑组件，
 *    不参与帧同步、不进快照。
 * ============================================================ */

 // 播放帧率（帧/秒）。素材未携带帧率，统一取 12 FPS。
static const float kSpriteFPS = 12.0f;

// 渲染层缓存（不属于帧同步状态）：
//  - 纹理缓存统一收归 AssetManager::tex_cache_（见 sprite_get_image），本文件不再持有；
//  - 每实体的动画播放状态改为 AnimStateComponent 挂在实体上（实体销毁由 ECS 自动回收，
//    消除此前裸全局 map 的泄漏）；
//  - 仅保留按角色类型共享的动画定义缓存（key=char_id），随场景销毁清理。
static std::unordered_map<int, anim_def_t>      g_anim_defs;      // key: char_id
// 按路径指针缓存已解析纹理：clip.path 是 anim_def_t 内稳定的 char[] 地址（整局不变），
// 用指针作 key 可省去每帧的字符串哈希查找（AssetManager 内部仍是 unordered_map<string>）。
static std::unordered_map<const char*, image_p> g_img_cache;      // key: clip.path

// 清空玩家动画定义缓存（纹理归 AssetManager::ClearTexCache 管理）
static void player_anim_cache_clear()
{
        g_anim_defs.clear();
        g_img_cache.clear();
}

static image_p sprite_get_image(Context* ctx, const char* path)
{
        if (!path || !*path) return NULL;
        auto it = g_img_cache.find(path);          // 指针比较，O(1) 无字符串哈希
        if (it != g_img_cache.end()) return it->second;
        // 纹理缓存统一走 AssetManager（与字体/地图资源同一入口，生命周期由销毁时统一释放）
        auto* am = AssetManager::GetInstance();
        image_p img = (am && ctx) ? am->GetImage(ctx->r2d, path) : NULL;
        return g_img_cache[path] = img;
}

// 把一整条横向精灵表展开为若干 clip（每帧一个 src_rect）
static void sprite_add_clips(Context* ctx, anim_def_t* def, int state, const std::string& path)
{
        if (path.empty()) return;
        // 构建期直接走 AssetManager（按 string 稳定缓存纹理），不进 g_img_cache：
        // 这里的 path 是 animPath() 返回的临时 std::string，其 c_str() 出作用域即失效，
        // 不能用作 g_img_cache 的指针 key（会存悬空指针）。渲染期 clip.path 来自 def 内
        // 稳定 char[]，才由 sprite_get_image 走指针缓存。
        auto* am = AssetManager::GetInstance();
        image_p img = (am && ctx) ? am->GetImage(ctx->r2d, path.c_str()) : NULL;
        if (!img) return;

        float w = 0, h = 0;
        if (!image_get_size(img, &w, &h) || h <= 0.0f) return;
        int frames = (int)(w / h);
        if (frames <= 0) return;
        if (frames > ANIM_MAX_CLIPS) frames = ANIM_MAX_CLIPS;

        for (int i = 0; i < frames; i++) {
                jrect_t r = { (float)i * h, 0.0f, h, h };
                anim_add_clip(def, state, path.c_str(), 1.0f / kSpriteFPS, r);
        }
}

// 构建（并缓存）某角色的动画状态机
static const anim_def_t& sprite_anim_def(Context* ctx, int char_id)
{
        auto it = g_anim_defs.find(char_id);
        if (it != g_anim_defs.end()) return it->second;

        const CharacterDef& cd = g_characterCatalog.get(char_id);
        anim_def_t def;
        memset(&def, 0, sizeof(def));

        // 状态下标必须与 CharacterAnimId 对齐：
        // janim 内部按 current_state ∈ [2,5] 判定"攻击态"以防止重复触发，故 attack 必须是 2。
        int s_idle = anim_add_state(&def, "idle", true);
        int s_walk = anim_add_state(&def, "walk", true);
        int s_attack = anim_add_state(&def, "attack", false);
        int s_hit = anim_add_state(&def, "hit", false);
        int s_death = anim_add_state(&def, "death", false);
        int s_knocked = anim_add_state(&def, "knocked", false);
        (void)s_hit;
        // hit / knocked 目前是"已定义但无触发源"的休眠状态：项目暂无伤害/击退结算点去置触发
        // （hit 无过渡；knocked 的 knockback_timer 未被置满），故两者现都不播放。
        // 日后想让"受击"有反应，二选一接上即可（hit 直接复用 / knocked 置 knockback_timer）。

        sprite_add_clips(ctx, &def, s_idle, g_characterCatalog.animPath(cd, ANIM_IDLE));
        sprite_add_clips(ctx, &def, s_walk, g_characterCatalog.animPath(cd, ANIM_WALK));
        sprite_add_clips(ctx, &def, s_attack, g_characterCatalog.animPath(cd, ANIM_ATTACK));  // 素材缺失则无 clip
        sprite_add_clips(ctx, &def, s_hit, g_characterCatalog.animPath(cd, ANIM_HIT));
        sprite_add_clips(ctx, &def, s_death, g_characterCatalog.animPath(cd, ANIM_DEATH));
        sprite_add_clips(ctx, &def, s_knocked, g_characterCatalog.animPath(cd, ANIM_KNOCKED));

        anim_set_default_state(&def, s_idle);

        // 静止 ↔ 步行（按 speed 参数）
        int t = anim_add_transition(&def, s_idle, s_walk);
        anim_transition_add_cond(&def, t, "speed", ANIM_COND_GREATER, 0.0f);
        t = anim_add_transition(&def, s_walk, s_idle);
        anim_transition_add_cond(&def, t, "speed", ANIM_COND_EQUALS, 0.0f);
        // 任意态 → 攻击（trigger）；攻击播完 → 回到 idle
        t = anim_add_transition(&def, -1, s_attack);
        anim_transition_add_cond(&def, t, "attack", ANIM_COND_TRIGGER, 0.0f);
        t = anim_add_transition(&def, s_attack, s_idle);
        anim_transition_set_exit_time(&def, t,
                (float)ActionStateComponent::ATTACK_DURATION / 20.0f);  // 20Hz 逻辑 tick

        // 任意态 → 击退（trigger）；击退播完 → 回到 idle
        t = anim_add_transition(&def, -1, s_knocked);
        anim_transition_add_cond(&def, t, "knocked", ANIM_COND_TRIGGER, 0.0f);
        t = anim_add_transition(&def, s_knocked, s_idle);
        anim_transition_set_exit_time(&def, t,
                (float)ActionStateComponent::KNOCKED_DURATION / 20.0f);  // 20Hz 逻辑 tick

        g_anim_defs[char_id] = def;
        return g_anim_defs[char_id];
}

// 玩家序列帧：推进动画状态机 + 绘制（合并原 player_anim_pass 与 draw_player_pass，
// 单次遍历玩家实体，省去两次 view 迭代与两次动画定义查表；纯渲染、不参与帧同步、不进快照）。
// dt 为渲染帧真实步长（由 GameScene 在 Update 阶段存下，Render 阶段传入）。
// 绘制玩家所持武器（叠加画在角色 sprite 之上）。
// 武器取合图中该武器所在的格子；大小与手持偏移由每把武器在 weapons.json 里
// 单独配置（hold_scale / hold_dx / hold_dy），短刀小而靠外、长枪大而平端。
// 位置相对角色中心，偏移以米计（最终用 meters_to_pixels 转像素，与角色世界坐标同单位）。
// 注意：武器图集原生朝右、角色图集原生朝左，两者朝向相反，故武器的镜像与前后偏移
// 都取与角色相反的方向 —— 角色朝左（flip_h=false）时武器被镜像并画在前方（左侧）。
// hold_angle 是额外姿态角（度，顺时针为正）：0 = 横向朝前（枪/刀默认），-90 = 竖放刀尖朝上；
// 武器被镜像的一侧（flip_h=false）角度自动取反，保证左右两个朝向下姿态一致。
// weapon_id < 0 表示空手。纯渲染，不参与帧同步、不进快照。
static void draw_held_weapon(Context* ctx, int weapon_id, float cx, float cy, bool flip_h, float role_scale)
{
        if (weapon_id < 0) return;
        auto* am = AssetManager::GetInstance();
        if (!am) return;

        const std::string atlas = g_weaponCatalog.atlasPath();
        if (atlas.empty()) return;
        image_p img = am->GetImage(ctx->r2d, atlas.c_str());
        if (!img) return;

        const WeaponDef& wd = g_weaponCatalog.get(weapon_id);
        jrect_t cell = g_weaponCatalog.cellRect(wd);
        if (cell.h <= 0.0f) return;

        const float scale = role_scale * wd.hold_scale;
        // 手持偏移以"米"计（与角色世界坐标同单位），用 meters_to_pixels 转屏幕像素，与 cx/cy 一致；
        // 翻转时 dx 自动取反（武器随角色镜像）。
        const float dx = meters_to_pixels(wd.hold_dx);
        const float dy = meters_to_pixels(wd.hold_dy);
        vec2_t pos = { cx + (flip_h ? dx : -dx), cy + dy };
        vec2_t scl = { flip_h ? scale : -scale, scale };   // 武器与角色原生朝向相反：镜像/偏移取反，保证在前方且朝前
        vec2_t origin = { cell.w * 0.5f, cell.h * 0.5f };

        // 旋转（度→弧度；image_draw_ex 的角度按弧度处理，见 jtilemap.c:1035 用法）。
        // SDL 语义为先水平翻转再顺时针旋转，故武器被镜像的一侧（flip_h=false）角度取反，
        const float ang = (flip_h ? wd.hold_angle_deg : -wd.hold_angle_deg)
                * 3.1415926f / 180.0f;
        image_draw_ex(img, &cell, pos, ang, scl, origin);
}

static void player_render_pass(entt::registry& registry, Context* ctx, float dt)
{
        auto& cam = registry.ctx().get<GlobalState>().camera_position;
        auto view = registry.view<PlayerComponent, ActionStateComponent, TransformComponent>();

        for (auto e : view) {
                auto& pc = view.get<PlayerComponent>(e);
                auto& ac = view.get<ActionStateComponent>(e);
                auto& t = view.get<TransformComponent>(e);

                // 每实体动画状态挂在 AnimStateComponent 上（实体销毁自动回收，无泄漏）；
                // 首次创建时一次性缓存其角色动画定义，避免每帧按 char_id 查 g_anim_defs。
                auto& asc = registry.get_or_emplace<AnimStateComponent>(e);
                if (!asc.def) asc.def = &sprite_anim_def(ctx, pc.char_id);
                const anim_def_t& def = *asc.def;
                anim_t& a = asc.anim;

                // 死亡：janim 的 params 只有 speed/dir/attack，无 death 条件，
                // 故由外部强制切到 death 状态（janim 会挡住从 death 误触发 attack）。
                bool dead = registry.all_of<HealthComponent>(e)
                        && registry.get<HealthComponent>(e).dead;
                if (dead && a.current_state != ANIM_DEATH) {
                        a.current_state = ANIM_DEATH;
                        a.current_frame = 0;
                        a.frame_elapsed = 0;
                        a.state_elapsed = 0;
                }

                anim_params_t p;
                p.speed = (!dead && ac.state == ActionStateComponent::WALK) ? 1.0f : 0.0f;
                p.dir = ac.dir;
                // 攻击 trigger：attack_timer 刚被置满的那一帧才触发一次
                p.attack = (!dead && ac.state == ActionStateComponent::ATTACK &&
                        ac.attack_timer == ActionStateComponent::ATTACK_DURATION);
                // 击退 trigger：knockback_timer 刚被置满的那一帧才触发一次
                p.knocked = (!dead && ac.knockback_timer == ActionStateComponent::KNOCKED_DURATION);

                anim_update(&a, &def, &p, dt);

                // 朝向：图集只有朝左的一套，故向右走时需要水平翻转。
                // 上/下沿用上一次的朝向，避免上下走时左右抖动。
                // dir: 0=下 1=左 2=右 3=上（见 input_to_dir）
                if (ac.dir == 2)      a.flip_h = true;    // 右：翻转朝右
                else if (ac.dir == 1) a.flip_h = false;   // 左：图集本就朝左
                // 上/下：保持 a.flip_h 不变

                // ---- 绘制 ----
                anim_clip_t clip;
                if (!anim_get_current_clip(&a, &def, &clip)) continue;

                image_p img = sprite_get_image(ctx, clip.path);
                if (!img) continue;

                // 逻辑坐标(米) → 屏幕像素
                float sx = meters_to_pixels(t.position.x - cam.x);
                float sy = meters_to_pixels(t.position.y - cam.y);

                // 素材帧高 32px；按 50 PPM 缩放到约 1 格，避免角色过小
                const float scale = (clip.src_rect.h > 0.0f)
                        ? (50.0f / clip.src_rect.h) : 1.0f;
                vec2_t pos = { sx, sy };
                // image_draw_ex 会把负 scale 转为 SDL_FLIP_HORIZONTAL（见 jr2d.c），
                // 因此朝右时传负的 x 缩放即可实现镜像，无需引擎新增 flip 参数。
                vec2_t scl = { a.flip_h ? -scale : scale, scale };
                vec2_t origin = { clip.src_rect.w * 0.5f, clip.src_rect.h * 0.5f };

                image_draw_ex(img, &clip.src_rect, pos, 0.0f, scl, origin);

                // 所持武器：叠加在角色之上示意握持；空手（weapon_id < 0）时不绘制
                draw_held_weapon(ctx, pc.weapon_id, sx, sy, a.flip_h, scale);
        }
}

#endif