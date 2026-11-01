#include "character_catalog.h"
#include "../asset_manager.h"
#include <joy/external/cJSON.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>

// 移速安全区间（米/秒）。上限 15 保证单 tick 位移 < 1 格（20Hz 下 step<0.75m），
// 否则离散碰撞检测可能漏检穿墙。两端施加同一钳制，帧同步仍然一致。
static const float kSpeedMin = 0.5f;
static const float kSpeedMax = 15.0f;

// anims 键（顺序对应 CharacterAnimId）
static const char* kAnimKeys[ANIM_COUNT] = { "idle", "walk", "attack", "hit", "death", "knocked" };

// 内置默认表：characters.json 缺失/解析失败时的兜底。
// 必须与 resources 中 warrior（Scribble 绿色角色）的语义保持一致，且两端相同。
void CharacterCatalog::setBuiltinDefault()
{
    defs_.clear();
    byKey_.clear();
    CharacterDef d;
    d.id   = 0;
    d.key  = "warrior";
    d.name = "战士";
    d.max_hp = 140;
    d.move_speed = fp_from_float(2.4f);
    d.sprite_dir = "sprites/characters/green/";
    d.weapon = "sword";                  // 对应武器（单一固定，无切换）
    d.color  = 0xFF4394;                 // 默认绿（与内置 warrior 一致）
    d.anims[ANIM_IDLE].file = "idle";   // Scribble 角色为单帧静态，其余动画渲染降级 idle
    defs_.push_back(d);
    byKey_[d.key] = d.id;
}

static bool read_file_text(const char* path, std::string& out)
{
    // 走 joy 的 utils_read_file（内部用 SDL_LoadFile）——
    // 安卓资源打包在 APK 内，不是文件系统路径，std::fopen 读不到，
    // 结果就是 characters.json 读失败、只能回落到内置默认（1 个角色）。
    size_t sz = 0;
    char* data = utils_read_file(path, &sz);
    if (!data) return false;
    out.assign(data, sz);
    SDL_free(data);
    return true;
}

void CharacterCatalog::load()
{
    setBuiltinDefault();   // 先装兜底，解析成功则整体替换

    const char* path = AssetManager::GetInstance()
        ? AssetManager::GetInstance()->GetResourcePath(AssetManager::kCatConfig, "characters")
        : nullptr;
    if (!path) {
        log_info("[character] no config.characters entry, using built-in defaults");
        return;
    }

    std::string text;
    if (!read_file_text(path, text)) {
        log_info("[character] %s not found, using built-in defaults", path);
        return;
    }

    cJSON* root = cJSON_Parse(text.c_str());
    if (!root) {
        log_error("[character] parse %s failed, using built-in defaults", path);
        return;
    }

    std::vector<CharacterDef> parsed;
    int next_id = 0;

    // cJSON v1.x 无 cJSON_ArrayForEach：按 child 链表遍历，顺序即文件中的出现顺序。
    // 整数 id 依此顺序分配（未给 JSON 增加 id 字段，保持其原结构）。
    for (cJSON* ch = root->child; ch; ch = ch->next) {
        if (ch->type != cJSON_Object || !ch->string) continue;

        CharacterDef d;
        d.id  = next_id++;
        d.key = ch->string;

        cJSON* v = cJSON_GetObjectItem(ch, "name");
        d.name = (v && v->type == cJSON_String && v->valuestring) ? v->valuestring : d.key;

        v = cJSON_GetObjectItem(ch, "sprite_dir");
        if (v && v->type == cJSON_String && v->valuestring) d.sprite_dir = v->valuestring;

        v = cJSON_GetObjectItem(ch, "weapon");
        if (v && v->type == cJSON_String && v->valuestring) d.weapon = v->valuestring;

        v = cJSON_GetObjectItem(ch, "color");
        if (v && v->type == cJSON_String && v->valuestring)
            d.color = (uint32_t)std::strtoul(v->valuestring, nullptr, 16);   // "0xFF4394" → 0xFF4394

        cJSON* stats = cJSON_GetObjectItem(ch, "stats");
        if (stats) {
            cJSON* hp = cJSON_GetObjectItem(stats, "hp");
            if (hp && hp->type == cJSON_Number) d.max_hp = (int)hp->valuedouble;
            cJSON* sp = cJSON_GetObjectItem(stats, "speed");
            if (sp && sp->type == cJSON_Number) d.move_speed = fp_from_float((float)sp->valuedouble);
        }

        cJSON* anims = cJSON_GetObjectItem(ch, "anims");
        if (anims) {
            for (int a = 0; a < ANIM_COUNT; a++) {
                cJSON* av = cJSON_GetObjectItem(anims, kAnimKeys[a]);
                if (av && av->type == cJSON_String && av->valuestring && av->valuestring[0])
                    d.anims[a].file = av->valuestring;
            }
        }

        parsed.push_back(d);
        if ((int)parsed.size() >= MAX_CHARACTERS) break;
    }
    cJSON_Delete(root);

    if (parsed.empty()) {
        log_error("[character] %s has no character entry, using built-in defaults", path);
        return;
    }

    // 移速钳制：异常值会导致瞬移/穿墙。两端同样钳制，结果仍一致。
    for (size_t i = 0; i < parsed.size(); i++) {
        float sp = fp_to_float(parsed[i].move_speed);
        if (sp < kSpeedMin || sp > kSpeedMax) {
            log_error("[character] '%s' speed=%.2f m/s out of range [%.1f,%.1f], clamped",
                parsed[i].key.c_str(), sp, kSpeedMin, kSpeedMax);
            parsed[i].move_speed = fp_from_float(sp < kSpeedMin ? kSpeedMin : kSpeedMax);
        }
    }

    defs_.swap(parsed);
    byKey_.clear();
    for (const auto& d : defs_) byKey_[d.key] = d.id;
    log_info("[character] loaded %s: %d character(s)", path, (int)defs_.size());
}

const CharacterDef& CharacterCatalog::get(int id)
{
    if (defs_.empty()) setBuiltinDefault();   // 防御：load 之前被查询时回退内置默认
    if (id < 0 || id >= (int)defs_.size()) id = 0;
    return defs_[id];
}

const CharacterDef* CharacterCatalog::findByName(const std::string& key)
{
    if (defs_.empty()) setBuiltinDefault();
    auto it = byKey_.find(key);
    if (it == byKey_.end()) return nullptr;
    return &defs_[it->second];
}

std::string CharacterCatalog::animPath(const CharacterDef& def, CharacterAnimId anim) const
{
    if (anim < 0 || anim >= ANIM_COUNT) anim = ANIM_IDLE;
    if (def.anims[anim].file.empty()) anim = ANIM_IDLE;   // 素材缺失 → 降级
    if (def.anims[ANIM_IDLE].file.empty()) return std::string();

    std::string p = def.sprite_dir;
    if (!p.empty() && p[p.size() - 1] != '/') p += "/";
    p += def.anims[anim].file;
    p += ".png";
    return p;
}
