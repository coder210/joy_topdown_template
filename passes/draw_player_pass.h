#ifndef DRAW_PLAYER_PASS_H
#define DRAW_PLAYER_PASS_H
#include <joy/joy.h>
#include <joy/janim.h>          /* joy 原生帧动画状态机：支持精灵表裁剪 + 状态过渡 */
#include <string>
#include <unordered_map>
#include <cstring>
#include <cmath>
#include "../components/global_state_component.h"
#include "../components/net_session_component.h"
#include "../components/id_component.h"
#include "../components/player_component.h"
#include "../components/action_state_component.h"
#include "../components/player_state_input.h"
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

// 弓的两帧贴图（tilesheet.png，64×64 网格第 9 行，纯渲染用）：
//   col5 = (320,576) 拉满搭箭的弓（拉弓阶段）
//   col6 = (384,576) 无箭的弓（待机 / 松放后）
//   col7 = (448,576) 箭弹体（飞行中的箭，供子弹渲染用）
// 两帧原生箭尖朝上，与 weapons.png 的弓同为竖放（hold_angle=-90），旋转公式不变。
static const char* kBowTilesheetPath = "joy_topdown_template/tilesheet.png";
static const float kBowCellPx = 64.0f;
static jrect_t bow_cell_with_arrow() { jrect_t r = { 320.0f, 576.0f, kBowCellPx, kBowCellPx }; return r; }
static jrect_t bow_cell_no_arrow()   { jrect_t r = { 384.0f, 576.0f, kBowCellPx, kBowCellPx }; return r; }

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
// 绘制玩家所持武器（叠加画在角色 sprite 之上，参考 rougelite 类游戏的
// "武器独立于角色、绕握点随朝向旋转"的表现）：
//  - 收势（待机/行走）：均取世界角 270°（竖直），不随移动朝向乱指。
//    近战竖举在肩后（垫在角色身后绘制）；远程(弓)正前方持握（沿朝向推出）
//    画在身前（对齐参考 rougelite：弓手把弓持在身前一侧，始终可见）；
//  - 近战：攻击时锁定 attack_dir，沿基角 ±75° 挥扫一道弧线（ease-out），并伴随
//    握点半径脉冲（前刺感），画在角色身前；
//  - 远程(弓)：攻击时原地拉弓蓄力（握点后撤、弦上搭箭）后松放回弹，不挥扫；
//    上述进度取自 attack_timer 递减，帧同步两端一致，纯渲染推导、不进快照、不改协议。
// 武器取合图中该武器所在的格子；大小/握距/手高由 weapons.json 每把武器单独配置
// （hold_scale / hold_dx / hold_dy），hold_dx 兼作握点绕角色旋转的半径（米），
// hold_dy 为垂直手高（米）。武器不再随角色水平镜像，独立做 360° 旋转。
// 图集原生朝向修正：近战武器竖放（刀尖朝上，hold_angle=-90），旋转角 = aim - hold_angle
// 即可让其刀尖指向 aim；横放的弓/盾（hold_angle=0）直接指向 aim。
// weapon_id < 0 表示空手。纯渲染，不参与帧同步、不进快照。

// dir（0=下 1=左 2=右 3=上，见 input_to_dir）→ 世界角（度，屏幕坐标 y 向下、
// 顺时针为正，0=朝右、90=朝下；屏幕 y 向下，故"下"为 +90 而非 -90）。
static float dir_to_angle_deg(int dir)
{
        switch (dir) {
        case 0: return 90.0f;    // 下
        case 1: return 180.0f;   // 左
        case 2: return 0.0f;     // 右
        case 3: return 270.0f;   // 上
        default: return 0.0f;
        }
}

// has_mouse_aim / mouse_aim_deg：本机玩家的鼠标瞄准（度，世界坐标，0=右、顺时针为正）。
// 仅用于远程(弓)的朝向表现：收势指向光标、攻击沿光标方向拉弓；近战完全不受影响。
// 关键点：弓的朝向**只认鼠标**，键盘上下左右不参与（否则移动朝向会跟鼠标朝向打架）。
// 服务端/无鼠标时为 false，弓竖直朝上（无瞄准数据）。纯渲染，不参与帧同步。
static void draw_held_weapon(Context* ctx, int weapon_id, float cx, float cy,
                             float role_scale, const ActionStateComponent& ac, bool flip_h,
                             bool has_mouse_aim, float mouse_aim_deg)
{
        if (weapon_id < 0) return;
        auto* am = AssetManager::GetInstance();
        if (!am) return;

        const std::string atlas = g_weaponCatalog.atlasPath();
        if (atlas.empty()) return;
        image_p img = am->GetImage(ctx->r2d, atlas.c_str());
        if (!img) return;

        const WeaponDef& wd = g_weaponCatalog.get(weapon_id);
        // 武器格子与越界保护延迟到攻击分支之后再取（弓拉弓时要换成"搭箭弓"格）

        const float scale = role_scale * wd.hold_scale;

        // 瞄准角 / 攻击表现：所有武器（近战 + 远程）一律由鼠标瞄准驱动朝向，
        // 与弓一致——量化角随输入同步，键盘上下左右完全不参与。无瞄准数据时竖直朝上。
        float aim   = 270.0f;    // 收势：竖直朝上（刀尖/弓背朝上）
        float reach = 1.0f;      // 握点半径脉冲（攻击时前刺 / 拉弓后退）
        bool  useArrow = false;  // 弓拉弓阶段在弦上搭一支箭
        if (has_mouse_aim)
                aim = mouse_aim_deg;   // 所有武器：只认鼠标

        const bool attacking = (ac.state == ActionStateComponent::ATTACK
                && ac.attack_timer > 0);
        if (attacking) {
                // 远程(弓)与近战都沿鼠标瞄准方向；无鼠标时近战沿用键盘 attack_dir 挥扫
                const float base = (wd.kind == 0 || has_mouse_aim) ? aim : dir_to_angle_deg(ac.attack_dir);
                // 进度 0→1（attack_timer 从 ATTACK_DURATION 逐 tick 递减）
                const float p = 1.0f - (float)ac.attack_timer
                        / (float)ActionStateComponent::ATTACK_DURATION;

                if (wd.kind == 1) {                          // 近战：沿基角 ±75° 挥扫（ease-out 起手快收势慢）
                        const float kSwingDeg = 75.0f;
                        const float e = 1.0f - (1.0f - p) * (1.0f - p);
                        aim   = base - kSwingDeg + 2.0f * kSwingDeg * e;
                        reach = 1.0f + 0.30f * sinf(p * 3.1415926f);
                } else {                                     // 远程(弓)：拉弓蓄力 → 松放
                        // 0~0.6 拉满、0.6~1 松放；拉满时握点回收(弓靠近身体)、松放瞬间前送
                        const float drawAmt = (p < 0.6f) ? (p / 0.6f)
                                                        : (1.0f - (p - 0.6f) / 0.40f);
                        reach = 1.0f - 0.32f * drawAmt;                       // 拉弓后撤
                        if (p >= 0.6f)
                                reach += 0.10f * sinf((p - 0.6f) / 0.40f * 3.1415926f); // 松放回弹前刺
                        aim = base;                                        // 弓始终指向瞄准方向，不挥扫
                        useArrow = (p < 0.6f);                           // 拉弓至松放前搭箭在弦
                }
        }

        // 取武器格子：
        //  - 近战：weapons.png 合图中该武器所在格（不变）；
        //  - 远程(弓)：两帧贴图在 tilesheet.png（row9）——拉弓搭箭(p<0.6)用 col5"有箭的弓"，
        //    待机与松放后(p>=0.6，箭已射出)用 col6"无箭的弓"。tilesheet 加载失败时回退 weapons.png。
        jrect_t cell = g_weaponCatalog.cellRect(wd);
        image_p wimg = img;
        if (wd.kind == 0) {
                image_p tsimg = am->GetImage(ctx->r2d, kBowTilesheetPath);
                if (tsimg) {
                        wimg = tsimg;
                        cell = useArrow ? bow_cell_with_arrow() : bow_cell_no_arrow();
                }
        }
        if (cell.h <= 0.0f) return;

        // 握点：攻击时以角色中心为原点，把"握距(前)/手高(下)"偏移随瞄准角一起旋转。
        // 收势分两态：
        //  - 有鼠标瞄准：所有武器沿瞄准方向正前方持握（与弓一致，武器随鼠标旋转可见）；
        //  - 无鼠标(服务端/键盘)：近战竖举肩后（原行为），远程(弓)仍画身前。
        const float rad = aim * 3.1415926f / 180.0f;
        const float rr  = meters_to_pixels(wd.hold_dx) * reach;
        const float ry  = meters_to_pixels(wd.hold_dy);
        vec2_t pos;
        if (!attacking && wd.kind == 1 && !has_mouse_aim) {
                pos.x = cx + (flip_h ? -ry : ry) * 1.8f;
                pos.y = cy - rr * 0.9f;
        } else if (!attacking) {
                // 远程(弓)：沿瞄准方向(鼠标)正前方持握，与键盘朝向无关
                const float rad0 = aim * 3.1415926f / 180.0f;
                pos.x = cx + cosf(rad0) * rr * 1.15f;
                pos.y = cy + sinf(rad0) * rr * 1.15f;
        } else {
                pos.x = cx + cosf(rad) * rr - sinf(rad) * ry;
                pos.y = cy + sinf(rad) * rr + cosf(rad) * ry;
        }
        vec2_t scl    = { scale, scale };                  // 不随角色翻转，武器独立旋转
        vec2_t origin = { cell.w * 0.5f, cell.h * 0.5f };  // 绕贴图中心旋转

        // 旋转角 = 瞄准角 - 图集姿态修正（竖放武器 hold_angle=-90 → +90 修正为刀尖指向 aim）
        const float ang = (aim - wd.hold_angle_deg) * 3.1415926f / 180.0f;
        image_draw_ex(wimg, &cell, pos, ang, scl, origin);
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

                // Scribble 素材为 64x64 单帧；统一按"绘制高度 50px"缩放（武器同用此基准）
                const float scale = (clip.src_rect.h > 0.0f)
                        ? (50.0f / clip.src_rect.h) : 1.0f;
                vec2_t pos = { sx, sy };
                // image_draw_ex 会把负 scale 转为 SDL_FLIP_HORIZONTAL（见 jr2d.c），
                // 因此朝右时传负的 x 缩放即可实现镜像，无需引擎新增 flip 参数。
                vec2_t scl = { a.flip_h ? -scale : scale, scale };
                vec2_t origin = { clip.src_rect.w * 0.5f, clip.src_rect.h * 0.5f };

                // 瞄准角取自 ActionStateComponent.aim_yaw：由本机鼠标量化后随 C2S 输入上传，
                // 两端消费同一个整数，故所有客户端画出的"武器指向"一致（不直接用本机鼠标，
                // 否则只有自己看得到）。255=无瞄准数据 → 回退到"朝向"表现。
                bool  has_mouse_aim = false;
                float mouse_aim_deg = 0.0f;
                if (ac.aim_yaw != kAimYawInvalid) {
                        mouse_aim_deg = aim_yaw_degrees(ac.aim_yaw);
                        has_mouse_aim = true;
                }

                // 武器绘制顺序：攻击必在身前（挥扫/拉弓）；非攻击时近战垫在身后
                // （肩后竖举）、远程(弓)持于身前画在身前（对齐参考 rougelite，弓始终可见）
                const bool attacking = (!dead && ac.state == ActionStateComponent::ATTACK
                        && ac.attack_timer > 0);
                bool weapon_front = attacking;
                if (!attacking && pc.weapon_id >= 0)
                        weapon_front = (g_weaponCatalog.get(pc.weapon_id).kind == 0);
                if (!weapon_front)
                        draw_held_weapon(ctx, pc.weapon_id, sx, sy, scale, ac, a.flip_h,
                                has_mouse_aim, mouse_aim_deg);

                image_draw_ex(img, &clip.src_rect, pos, 0.0f, scl, origin);

                if (weapon_front)
                        draw_held_weapon(ctx, pc.weapon_id, sx, sy, scale, ac, a.flip_h,
                                has_mouse_aim, mouse_aim_deg);
        }
}

#endif