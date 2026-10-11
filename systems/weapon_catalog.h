#ifndef WEAPON_CATALOG_H
#define WEAPON_CATALOG_H
#include <string>
#include <vector>
#include <unordered_map>
#include <joy/joy.h>

// 武器表：唯一真源是 data/weapons.json（经 resources.json 的 config.weapons 索引）。
// 本文件只做"运行时 catalog"——解析、缓存、按整数 id / key 查询。
//
// 武器贴图为一张 2 行 × 6 列的 32×32 网格合图（sprites/weapons/weapons.png）：
// 第 0~3 列是 8 把武器，第 4~5 列是准星（瞄准图标，非武器，不登记进本表）。
// 因此"取武器贴图"= 加载一次合图 + 按 (row, col) 裁 32×32 的格子，
// 与角色动画按 src_rect 切帧的套路一致，不产生额外的纹理加载。
//
// 武器上限：防止 JSON 异常时武器表无限增长。
static const int MAX_WEAPONS = 32;

// 合图格子尺寸（像素）。JSON 未给 cell_px 时使用此默认值。
static const float kWeaponCellPx = 32.0f;

struct WeaponDef {
    int         id = 0;         // 整数 id，按 JSON 中 weapons 出现顺序分配
    std::string key;            // "sniper"
    std::string name;           // 展示名
    int         row = 0;        // 合图行号（从 0 起）
    int         col = 0;        // 合图列号（从 0 起）

    // ---- 手持表现（draw_held_weapon 用，全部可在 weapons.json 按武器单独配）----
    // hold_dx / hold_dy 以"米"计（与角色世界坐标同单位），最终用 meters_to_pixels 转屏幕像素，
    // 与角色中心的换算一致；均相对角色 sprite 中心。朝向翻转时 dx 自动取反（武器随角色镜像）。
    // 小刀类短武器 scale 调小、dx 稍大让它落在身体侧外；长枪 scale 大、dy 小近似端枪。
    float hold_scale = 0.6f;    // 武器缩放 = 角色缩放 × hold_scale
    float hold_dx    = 0.5f;    // 横向偏移（米，右为正；翻转后自动向左）
    float hold_dy    = 0.25f;   // 纵向偏移（米，下为正）

    // 持握旋转（度，顺时针为正，SDL 语义；渲染时转弧度传 image_draw_ex）。
    // 武器在合图里一律横向、刀尖/枪口朝右；竖放（刀尖朝上）配 -90。
    // 角色朝向翻转时角度自动取反，保证翻转前后竖直方向一致。
    float hold_angle_deg = 0.0f;

    // 武器类别：0=远程(射击) 1=近战(挥动)。决定默认攻击表现与触发判定。
    int   kind = 0;            // 0=ranged 1=melee
    // 攻击序列帧条（用户提供素材后填写）：单条横排，帧宽=图高，原生朝右。
    std::string attack_path;   // 攻击条图片运行时相对路径（空=无素材，仅用旋转/后坐表现）
    int   attack_frames = 0;   // 攻击条帧数
    float attack_fps = 12.0f;  // 攻击条播放帧率
};

// 运行时武器目录：解析、缓存、按 id / key 查询。唯一真源是 data/weapons.json。
class WeaponCatalog {
public:
    // 解析 weapons.json 填充武器表。必须在 AssetManager::Init() 之后调用。
    // 文件缺失/解析失败时回退内置默认表——两端回退到同一份默认值，帧同步仍然一致。
    void load();

    // 按整数 id 取武器定义；越界回退 id=0。至少含 1 项。
    const WeaponDef& get(int id);

    // 按 key 字符串（如 "sniper"）反查；找不到返回 nullptr。
    const WeaponDef* findByName(const std::string& key);

    size_t size() const { return defs_.size(); }

    // 武器合图的运行时相对路径（sprite 走 AssetManager::GetImage，不进资源索引）。
    const std::string& atlasPath() const { return atlas_; }

    // 单个格子的像素尺寸
    float cellPx() const { return cell_px_; }

    // 取该武器在合图中的裁剪框（src_rect）。
    jrect_t cellRect(const WeaponDef& def) const;

private:
    void setBuiltinDefault();                        // 兜底内置表（与 weapons.json 一致）

    std::vector<WeaponDef> defs_;                    // 稠密，按武器 id 索引
    std::unordered_map<std::string, int> byKey_;     // key → id 反查
    std::string atlas_;                              // 合图路径
    float cell_px_ = kWeaponCellPx;                  // 格子尺寸
};

// 全局唯一武器目录实例
inline WeaponCatalog g_weaponCatalog;

#endif
