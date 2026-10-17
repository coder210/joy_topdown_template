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
//    画在身前正前方（对齐参考 rougelite，弓始终可见）；
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

// has_mouse_aim / mouse_aim_deg：武器朝向的瞄准角（度，世界坐标，0=右、顺时针为正），
// 由 ActionStateComponent.aim_yaw 还原（鼠标量化角随输入同步到所有客户端，两端一致）。
// 所有武器（近战 + 远程）一律由此驱动朝向：收势指向光标、攻击沿光标方向；
// 键盘上下左右不参与方向（否则移动朝向会和鼠标朝向打架）；无瞄准数据时竖直朝上。
// 持握距离倍率：把武器(含握点/手)沿瞄准方向整体推离角色。1.0 = 原距离，>1 更远。
// 只放大径向(沿瞄准方向)分量，不放大侧向的"手高"侧移，避免武器往一边歪。
static const float kWeaponHoldDistScale = 1.35f;

static void draw_held_weapon(Context* ctx, int weapon_id, float cx, float cy,
                             float role_scale, const ActionStateComponent& ac, bool flip_h,
                             bool has_mouse_aim, float mouse_aim_deg, int char_id)
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
        // 弓的动画顺序（表现层）：有箭帧(待机/搭箭) → 无箭帧(箭离弦) → 回到有箭帧。
        // 判定**不能**用"是否处于攻击状态"：连按攻击键会让 attack_timer 反复重置、
        // attacking 恒为真，弓就永久卡在无箭帧。改用"空弦计时"——每次真的射出箭后
        // 只显示 ARROW_GONE_TICKS 的无箭帧，然后自动回到有箭帧（重新搭箭），
        // 连射也能看到完整的 有箭→无箭→有箭 循环。
        useArrow = (ac.arrow_gone_timer == 0);
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
                } else {                                     // 远程(弓)：拉弓后撤 → 复位
                        // 撒放/回位进度 0→1→0：前 SHOOT_DRAW_TICKS 撒放(0→1)，其余回位(1→0)。
                        // 纯视觉、不参与逻辑（箭已在按下瞬间生成）；不再依赖 pending_shoot_timer，
                        // 起手处 drawAmt=0 → reach=1.15 与待机位置一致，点击瞬间不会跳变。
                        float drawAmt;
                        const int elapsed = ActionStateComponent::ATTACK_DURATION - ac.attack_timer;
                        if (elapsed <= ActionStateComponent::SHOOT_DRAW_TICKS) {
                                drawAmt = (float)elapsed
                                        / (float)ActionStateComponent::SHOOT_DRAW_TICKS;
                        } else {
                                const int rec = ActionStateComponent::ATTACK_DURATION
                                              - ActionStateComponent::SHOOT_DRAW_TICKS;
                                drawAmt = (rec > 0) ? (float)ac.attack_timer / (float)rec : 0.0f;
                        }
                        if (drawAmt < 0.0f) drawAmt = 0.0f;
                        else if (drawAmt > 1.0f) drawAmt = 1.0f;
                        reach = 1.15f - 0.47f * drawAmt;     // 后撤：1.15 → 0.68 → 1.15
                        aim = base;                          // 弓始终指向瞄准方向，不挥扫
                }
        }

        // 取武器格子：
        //  - 近战：weapons.png 合图中该武器所在格（不变）；
        //  - 远程(弓)：两帧贴图在 tilesheet.png（row9）——
        //    useArrow(非攻击态) 用 col5"有箭的弓"(待机双手持有箭)；
        //    攻击中用 col6"无箭的弓"(箭已离弦)；攻击结束自动回到有箭帧。
        //    tilesheet 加载失败时回退 weapons.png。
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
                pos.y = cy - rr * 0.9f * kWeaponHoldDistScale;
        } else if (!attacking) {
                // 远程(弓)：沿瞄准方向(鼠标)正前方持握，与键盘朝向无关
                const float rad0 = aim * 3.1415926f / 180.0f;
                pos.x = cx + cosf(rad0) * rr * 1.15f * kWeaponHoldDistScale;
                pos.y = cy + sinf(rad0) * rr * 1.15f * kWeaponHoldDistScale;
        } else {
                // 攻击：近战挥扫时带手高侧移(ry)；远程(弓)只沿瞄准方向前后移动，
                // 不加侧移——否则拉弓时弓会向一侧偏（瞄准朝上时表现为向右）。
                const float perp = (wd.kind == 0) ? 0.0f : ry;
                pos.x = cx + cosf(rad) * rr * kWeaponHoldDistScale - sinf(rad) * perp;
                pos.y = cy + sinf(rad) * rr * kWeaponHoldDistScale + cosf(rad) * perp;
        }
        vec2_t scl    = { scale, scale };                  // 不随角色翻转，武器独立旋转
        // 旋转轴 = 握点（素材上"手握的位置"，如弓上的蓝点标记）：武器绕握点旋转，
        // 握点落在 pos（角色手部）。未配置握点（<0）的素材退回格中心。
        vec2_t origin = { (wd.grip_x >= 0.0f ? wd.grip_x : cell.w * 0.5f),
                          (wd.grip_y >= 0.0f ? wd.grip_y : cell.h * 0.5f) };

        // 旋转角 = 瞄准角 - 图集姿态修正（竖放武器 hold_angle=-90 → +90 修正为刀尖指向 aim）
        const float ang = (aim - wd.hold_angle_deg) * 3.1415926f / 180.0f;
        image_draw_ex(wimg, &cell, pos, ang, scl, origin);

        // 手：把角色的"手"贴图（sprite_dir/hand.png，绿色拳头）画在握点上，盖在武器上
        // → 手握住武器的握点（如弓上的蓝点）。手不随武器旋转（拳头是圆的），
        //   大小与角色同比例（role_scale）。
        // 手的位置可用 hand_dx/hand_dy（格内像素，随武器朝向一起旋转）单独微调——
        // 只动手、不动武器（武器位置由上面的 origin/grip 决定）。素材缺失时静默跳过。
        vec2_t handPos = pos;
        if (wd.hand_dx != 0.0f || wd.hand_dy != 0.0f) {
                const float hc = cosf(ang), hs = sinf(ang);
                const float lx = wd.hand_dx * scale;
                const float ly = wd.hand_dy * scale;
                handPos.x = pos.x + lx * hc - ly * hs;
                handPos.y = pos.y + lx * hs + ly * hc;
        }
        const CharacterDef& cd = g_characterCatalog.get(char_id);
        std::string handPath = cd.sprite_dir;
        if (!handPath.empty() && handPath[handPath.size() - 1] != '/') handPath += "/";
        handPath += "hand.png";
        image_p handImg = am->GetImage(ctx->r2d, handPath.c_str());
        if (handImg) {
                float hw = 0.0f, hh = 0.0f;
                if (image_get_size(handImg, &hw, &hh) && hw > 0.0f && hh > 0.0f) {
                        jrect_t handCell = { 0.0f, 0.0f, hw, hh };
                        vec2_t hscl    = { role_scale, role_scale };
                        vec2_t horigin = { hw * 0.5f, hh * 0.5f };
                        image_draw_ex(handImg, &handCell, handPos, 0.0f, hscl, horigin);

                        // 拉弦手：拉弓阶段(useArrow，弦上搭箭)在"箭与弦相交"处再画一只手。
                        // 该点取自素材实测：tilesheet col5(320,576) 格内弦的 V 形顶点/箭尾搭弦点
                        // ≈(32,50)，相对握点(origin，未配置时=格中心32,32)沿箭尾方向约 18px。
                        // 随武器旋转角变换到屏幕：握点 + R(ang)·(nock-grip)·scale——
                        // 瞄右时在弓后(左)、瞄上时在弓下，永远位于弓与射手之间。
                        if (wd.kind == 0 && useArrow) {
                                const float ndx = (32.0f - origin.x) * scale;
                                const float ndy = (50.0f - origin.y) * scale;
                                const float hc2 = cosf(ang), hs2 = sinf(ang);
                                vec2_t stringPos = { pos.x + ndx * hc2 - ndy * hs2,
                                                     pos.y + ndx * hs2 + ndy * hc2 };
                                image_draw_ex(handImg, &handCell, stringPos, 0.0f, hscl, horigin);
                        }
                }
        }
}

// ---------------- 玩家血条：水墨朱砂·胶囊细条 ----------------
// 引擎 shape_draw_polygon 走 SDL_RenderGeometry，在逻辑呈现(LETTERBOX)下不渲染；
// image_draw_ex 走 SDL_RenderTextureRotated（角色/武器都在用）可靠且支持旋转。
// 故血条用 CPU 逐像素生成的胶囊纹理（抗锯齿圆角、两端半圆）+ 旋转绘制。
// 轨道纹理固定（缓存一次）；填充纹理随 HP 换色/换长，每帧更新像素。
static const int HPBAR_W   = 36;   // 内芯长（像素）
static const int HPBAR_H   = 12;   // 内芯厚（像素）
static const int HPBAR_PAD = 1;    // 外描边 1px

static image_p s_hpbar_track = NULL;   // 轨道+描边（38×8）
static image_p s_hpbar_fill  = NULL;   // 填充（36×6，流式，每帧更新）

// 通用胶囊像素：在 buf(W×H, RGBA32) 画一条从 x=0 到 x=fillLen 的胶囊（两端半圆、1px 羽化）。
static void hpbar_gen_capsule(uint8_t* buf, int W, int H, float fillLen,
                              uint8_t R, uint8_t G, uint8_t B, uint8_t A)
{
        memset(buf, 0, (size_t)W * H * 4);
        if (fillLen <= 0.0f) return;
        if (fillLen > (float)W) fillLen = (float)W;
        float cy = H * 0.5f;
        float rad = H * 0.5f;
        float x0 = rad, x1 = fillLen - rad;
        if (x1 < x0) {                       // 太短：退化为圆帽
                rad = fillLen * 0.5f;
                if (rad < 0.5f) rad = 0.5f;
                x0 = x1 = fillLen * 0.5f;
        }
        for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                        float px = x + 0.5f, py = y + 0.5f;
                        float dx = (px < x0) ? (x0 - px) : ((px > x1) ? (px - x1) : 0.0f);
                        float dy = py - cy;
                        float cov = rad - sqrtf(dx * dx + dy * dy) + 0.5f;   // 1px 抗锯齿
                        if (cov <= 0.0f) continue;
                        if (cov > 1.0f) cov = 1.0f;
                        uint8_t* p = buf + ((size_t)y * W + x) * 4;
                        p[0] = R; p[1] = G; p[2] = B;
                        p[3] = (uint8_t)(A * cov);
                }
        }
}

// 轨道+描边：外 1px 深墨描边、内部半透明墨（同一墨色，靠 alpha 深浅区分）。
static void hpbar_gen_track(uint8_t* buf, int W, int H)
{
        memset(buf, 0, (size_t)W * H * 4);
        float cy = H * 0.5f;
        float rOut = H * 0.5f;
        float rIn  = rOut - (float)HPBAR_PAD;
        float x0o = rOut, x1o = W - rOut;
        for (int y = 0; y < H; y++) {
                for (int x = 0; x < W; x++) {
                        float px = x + 0.5f, py = y + 0.5f;
                        float dx = (px < x0o) ? (x0o - px) : ((px > x1o) ? (px - x1o) : 0.0f);
                        float dy = py - cy;
                        float covO = rOut - sqrtf(dx * dx + dy * dy) + 0.5f;
                        if (covO <= 0.0f) continue;
                        if (covO > 1.0f) covO = 1.0f;
                        float covI = rIn - sqrtf(dx * dx + dy * dy) + 0.5f;
                        uint8_t* p = buf + ((size_t)y * W + x) * 4;
                        p[0] = 40; p[1] = 40; p[2] = 45;
                        if (covI >= 1.0f) p[3] = 120;                    // 内部：半透明墨
                        else              p[3] = (uint8_t)(205.0f * covO); // 描边：更实
                }
        }
}

static void ensure_hpbar_textures(jr2d_p r2d)
{
        if (!s_hpbar_track) {
                const int W = HPBAR_W + HPBAR_PAD * 2, H = HPBAR_H + HPBAR_PAD * 2;
                s_hpbar_track = image_create_empty(r2d, W, H);
                if (s_hpbar_track) {
                        uint8_t buf[(HPBAR_W + 2) * (HPBAR_H + 2) * 4];
                        hpbar_gen_track(buf, W, H);
                        jrect_t r = { 0, 0, (float)W, (float)H };
                        image_update(s_hpbar_track, &r, buf, W * 4);
                        image_set_blend_mode(s_hpbar_track, JR2D_BLEND_BLEND);
                }
        }
        if (!s_hpbar_fill) {
                s_hpbar_fill = image_create_empty(r2d, HPBAR_W, HPBAR_H);
                if (s_hpbar_fill) image_set_blend_mode(s_hpbar_fill, JR2D_BLEND_BLEND);
        }
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
                                has_mouse_aim, mouse_aim_deg, pc.char_id);

                image_draw_ex(img, &clip.src_rect, pos, 0.0f, scl, origin);

                if (weapon_front)
                        draw_held_weapon(ctx, pc.weapon_id, sx, sy, scale, ac, a.flip_h,
                                has_mouse_aim, mouse_aim_deg, pc.char_id);

                // 玩家血条：两矩形（黑底+绿填充）旋转贴附——长轴沿瞄准方向、挂在其右手法线侧，
                // 始终与武器平行、随瞄准绕角色转，不会"平行/垂直"乱翻。长度由血量决定。
                // 世界空间绘制（随摄像机移动）。纯渲染，不进快照/帧同步。
                if (registry.all_of<HealthComponent>(e)) {
                        auto& hc = registry.get<HealthComponent>(e);
                        float ratio = (hc.max_hp > 0) ? ((float)hc.hp / (float)hc.max_hp) : 0.0f;
                        if (ratio < 0.0f) ratio = 0.0f;
                        if (ratio > 1.0f) ratio = 1.0f;

                        // 瞄准方向：优先 aim_yaw（鼠标/键盘已量化并随输入同步），否则回退身体朝向
                        float aim_deg = mouse_aim_deg;
                        if (!has_mouse_aim) {
                                switch (ac.dir) {
                                        case 0: aim_deg = 90.0f;  break;  // 下
                                        case 1: aim_deg = 180.0f; break;  // 左
                                        case 2: aim_deg = 0.0f;   break;  // 右
                                        default: aim_deg = 270.0f; break;  // 上
                                }
                        }
                        const float PI = 3.14159265f;
                        float rad = aim_deg * PI / 180.0f;
                        float dx = cosf(rad), dy = sinf(rad);     // 瞄准方向（屏幕 y 向下：0=右,90=下）
                        float nx = -dy, ny = dx;                  // 右手法线侧（绕角色的右侧）

                        // 水墨胶囊血条：内芯 36×6、外 1px 墨描边、间距 3。长轴=瞄准方向。
                        const float barT = (float)HPBAR_H;   // 内芯厚
                        const float halfBody = 25.0f;        // 玩家精灵半高/半宽（显示约 50px）
                        const float gap      = 3.0f;         // 与玩家之间的间隔
                        float off = halfBody + gap + barT * 0.5f + (float)HPBAR_PAD;  // 沿法线侧外移
                        float cx = sx + nx * off;            // 血条中心
                        float cy = sy + ny * off;

                        ensure_hpbar_textures(ctx->r2d);

                        // 填充色 = 角色主色：取自 CharacterDef.color（characters.json 按角色配置，
                        // 值即该角色贴图的主体色，故血条与角色同色）。任何时候都用角色色，
                        // 不随血量变色（只靠长度表达剩余血量）。
                        const CharacterDef& cdef = g_characterCatalog.get(pc.char_id);
                        uint8_t fr = (uint8_t)((cdef.color >> 16) & 0xFF);
                        uint8_t fg = (uint8_t)((cdef.color >> 8)  & 0xFF);
                        uint8_t fb = (uint8_t)( cdef.color        & 0xFF);

                        // 填充纹理每帧按当前 HP 长度/颜色重绘（透明圆角胶囊）
                        if (s_hpbar_fill) {
                                uint8_t buf[HPBAR_W * HPBAR_H * 4];
                                hpbar_gen_capsule(buf, HPBAR_W, HPBAR_H,
                                                  (float)HPBAR_W * ratio, fr, fg, fb, 235);
                                jrect_t r = { 0, 0, (float)HPBAR_W, (float)HPBAR_H };
                                image_update(s_hpbar_fill, &r, buf, HPBAR_W * 4);
                        }

                        // 1:1 绘制（纹理已是目标像素尺寸），绕中心旋转 rad（瞄准角）
                        auto draw_bar = [&](image_p tex, float wpx, float hpx) {
                                if (!tex) return;
                                jrect_t src = { 0, 0, wpx, hpx };
                                vec2_t pos = { cx, cy };
                                vec2_t scl = { 1.0f, 1.0f };
                                vec2_t org = { wpx * 0.5f, hpx * 0.5f };
                                image_draw_ex(tex, &src, pos, rad, scl, org);
                        };
                        draw_bar(s_hpbar_track, (float)(HPBAR_W + HPBAR_PAD * 2), (float)(HPBAR_H + HPBAR_PAD * 2));
                        draw_bar(s_hpbar_fill,  (float)HPBAR_W, (float)HPBAR_H);
                }
        }
}

#endif