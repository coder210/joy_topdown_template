#ifndef CHARACTER_CATALOG_H
#define CHARACTER_CATALOG_H
#include <string>
#include <vector>
#include <unordered_map>
#include <joy/jmath.h>

// 角色表：唯一真源是 data/characters.json（经 resources.json 的 config.characters 索引）。
// 本文件只做"运行时 catalog"——解析、缓存、按整数 id / key 查询，不再内嵌任何角色数值。
//
// 角色上限：防止 JSON 异常时角色表无限增长。
static const int MAX_CHARACTERS = 16;

// 逻辑动画槽（对应 characters.json 的 anims 键）。
// 顺序不可随意调整：janim 内部按 current_state ∈ [2,5] 判定"攻击态"以防止重复触发，
// 故 ANIM_ATTACK 必须落在下标 2。
enum CharacterAnimId {
    ANIM_IDLE   = 0,
    ANIM_WALK   = 1,
    ANIM_ATTACK = 2,
    ANIM_HIT    = 3,
    ANIM_DEATH  = 4,
    ANIM_KNOCKED = 5,   // 击退/硬直反应动画；放末位以保持 ANIM_ATTACK=2（janim 防重复触发攻击的约束）
    ANIM_COUNT  = 6,
};

// 单个动画：精灵文件名（不含扩展名）。
// file 为空表示该动画素材缺失，渲染时自动降级到 idle。
struct CharacterAnim {
    std::string file;
};

struct CharacterDef {
    int         id = 0;              // 整数 id（协议 char_id），按 JSON 中出现顺序分配
    std::string key;                 // "warrior"
    std::string name;                // 展示名
    int         max_hp = 100;        // stats.hp
    fp_t        move_speed = 0;      // stats.speed（米/秒，已定点化）
    std::string sprite_dir;          // 精灵目录（运行时相对路径）
    std::string starting_weapon;     // 初始武器（如 "knife"）
    std::vector<std::string> weapons; // 持有的武器 key 列表（如 ["sword","axe"]）；空则回落 [starting_weapon]
    CharacterAnim anims[ANIM_COUNT]; // 按 CharacterAnimId 索引
};

// 运行时角色目录：解析、缓存、按 id / key 查询。唯一真源是 data/characters.json。
// 底层用稠密 vector 按 char_id 索引（O(1) 直接下标），并附带 key→id 反查——
// 即"字典式命名访问"，但性能等同数组，优于 std::map / unordered_map 的整数键查找。
class CharacterCatalog {
public:
    // 解析 characters.json 填充角色表。必须在 AssetManager::Init() 之后调用。
    // 文件缺失/解析失败时回退内置默认表——两端回退到同一份默认值，帧同步仍然一致。
    void load();

    // 按整数 id 取角色定义；越界回退 id=0。至少含 1 项。
    const CharacterDef& get(int id);

    // 按 key 字符串（如 "survivor_a"）反查；找不到返回 nullptr。
    const CharacterDef* findByName(const std::string& key);   // 如 "warrior"

    // 角色数量（至少 1）
    size_t size() const { return defs_.size(); }

    // 取动画精灵的完整运行时路径（sprite_dir + file + ".png"）。
    // 该动画缺失（file 为空）时自动降级到 idle；连 idle 都不可用则返回空串。
    std::string animPath(const CharacterDef& def, CharacterAnimId anim) const;

private:
    void setBuiltinDefault();                        // 兜底内置表（warrior）

    std::vector<CharacterDef> defs_;                 // 稠密，按 char_id 索引
    std::unordered_map<std::string, int> byKey_;     // key → id 反查
};

// 全局唯一角色目录实例
inline CharacterCatalog g_characterCatalog;

#endif
