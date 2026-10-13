#include "weapon_catalog.h"
#include "../asset_manager.h"
#include <joy/external/cJSON.h>
#include <cstdio>
#include <cstring>

// 内置默认表：weapons.json 缺失/解析失败时的兜底。
// 必须与 data/weapons.json 的语义保持一致，且两端相同。
void WeaponCatalog::setBuiltinDefault()
{
    defs_.clear();
    byKey_.clear();
    atlas_   = "joy_topdown_template/sprites/weapons/weapons.png";
    cell_px_ = kWeaponCellPx;

    static const struct {
        const char* key; const char* name; int row; int col;
        float hold_scale, hold_dx, hold_dy, hold_angle_deg;
        const char* kind;   // "melee" / "ranged"
    } kDefaults[] = {
        //            name      row col  scale   dx      dy      angle    kind
        { "sword",      "长剑",   0, 0,  0.55f, 0.40f, 0.12f, -90.0f, "melee"  },
        { "longsword",  "巨剑",   0, 1,  0.70f, 0.45f, 0.05f, -90.0f, "melee"  },
        { "axe",        "手斧",   0, 2,  0.55f, 0.40f, 0.12f, -90.0f, "melee"  },
        { "axe_large",  "巨斧",   0, 3,  0.65f, 0.40f, 0.05f, -90.0f, "melee"  },
        { "axe_double", "双刃斧", 1, 0,  0.65f, 0.40f, 0.05f, -90.0f, "melee"  },
        { "axe_blades", "双斧",   1, 1,  0.65f, 0.40f, 0.05f, -90.0f, "melee"  },
        { "dagger",     "匕首",   1, 2,  0.45f, 0.35f, 0.15f, -90.0f, "melee"  },
        { "hammer",     "战锤",   1, 3,  0.55f, 0.40f, 0.12f, -90.0f, "melee"  },
        { "spear",      "长矛",   2, 0,  0.70f, 0.45f, 0.05f, -90.0f, "melee"  },
        { "pole",       "长杆",   2, 1,  0.70f, 0.45f, 0.05f, -90.0f, "melee"  },
        { "staff",      "法杖",   2, 2,  0.70f, 0.45f, 0.05f, -90.0f, "melee"  },
        { "bow",        "弓",     3, 0,  0.85f, 0.40f, 0.10f, -90.0f, "ranged" },
        { "bow_arrow",  "搭箭弓", 3, 0,  0.85f, 0.40f, 0.10f, -90.0f, "ranged" },
        { "arrow",      "箭矢",   3, 1,  0.40f, 0.30f, 0.10f, -90.0f, "ranged" },
        { "shield_curved",   "圆盾", 3, 2, 0.50f, 0.30f, 0.20f,  0.0f, "melee" },
        { "shield_straight", "塔盾", 3, 3, 0.50f, 0.30f, 0.20f,  0.0f, "melee" },
    };
    const int n = (int)(sizeof(kDefaults) / sizeof(kDefaults[0]));
    for (int i = 0; i < n; i++) {
        WeaponDef d;
        d.id             = i;
        d.key            = kDefaults[i].key;
        d.name           = kDefaults[i].name;
        d.row            = kDefaults[i].row;
        d.col            = kDefaults[i].col;
        d.hold_scale     = kDefaults[i].hold_scale;
        d.hold_dx        = kDefaults[i].hold_dx;
        d.hold_dy        = kDefaults[i].hold_dy;
        d.hold_angle_deg = kDefaults[i].hold_angle_deg;
        d.kind           = (strcmp(kDefaults[i].kind, "melee") == 0) ? 1 : 0;
        defs_.push_back(d);
        byKey_[d.key] = d.id;
    }
}

static bool weapon_read_file_text(const char* path, std::string& out)
{
    std::FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    std::fseek(f, 0, SEEK_END);
    long len = std::ftell(f);
    std::fseek(f, 0, SEEK_SET);
    if (len <= 0) { std::fclose(f); return false; }
    out.assign((size_t)len, '\0');
    size_t got = std::fread(&out[0], 1, (size_t)len, f);
    std::fclose(f);
    return got == (size_t)len;
}

void WeaponCatalog::load()
{
    setBuiltinDefault();   // 先装兜底，解析成功则整体替换

    const char* path = AssetManager::GetInstance()
        ? AssetManager::GetInstance()->GetResourcePath(AssetManager::kCatConfig, "weapons")
        : nullptr;
    if (!path) {
        log_info("[weapon] no config.weapons entry, using built-in defaults");
        return;
    }

    std::string text;
    if (!weapon_read_file_text(path, text)) {
        log_info("[weapon] %s not found, using built-in defaults", path);
        return;
    }

    cJSON* root = cJSON_Parse(text.c_str());
    if (!root) {
        log_error("[weapon] parse %s failed, using built-in defaults", path);
        return;
    }

    // 合图信息与武器条目分开解析：atlas / cell_px 是整表共享，weapons 是逐条。
    std::string atlas;
    float cell = kWeaponCellPx;
    cJSON* av = cJSON_GetObjectItem(root, "atlas");
    if (av && av->type == cJSON_String && av->valuestring && av->valuestring[0])
        atlas = av->valuestring;
    cJSON* cv = cJSON_GetObjectItem(root, "cell_px");
    if (cv && cv->type == cJSON_Number && cv->valuedouble > 0.0)
        cell = (float)cv->valuedouble;

    std::vector<WeaponDef> parsed;
    int next_id = 0;
    cJSON* ws = cJSON_GetObjectItem(root, "weapons");
    if (ws) {
        // cJSON v1.x 无 cJSON_ArrayForEach：按 child 链表遍历，顺序即文件中的出现顺序。
        for (cJSON* ch = ws->child; ch; ch = ch->next) {
            if (ch->type != cJSON_Object || !ch->string) continue;

            WeaponDef d;
            d.id  = next_id++;
            d.key = ch->string;

            cJSON* v = cJSON_GetObjectItem(ch, "name");
            d.name = (v && v->type == cJSON_String && v->valuestring) ? v->valuestring : d.key;

            v = cJSON_GetObjectItem(ch, "row");
            if (v && v->type == cJSON_Number) d.row = (int)v->valuedouble;

            v = cJSON_GetObjectItem(ch, "col");
            if (v && v->type == cJSON_Number) d.col = (int)v->valuedouble;

            // 持握参数（可选）：未配置时沿用 WeaponDef 的默认值
            v = cJSON_GetObjectItem(ch, "hold_scale");
            if (v && v->type == cJSON_Number && v->valuedouble > 0.0)
                d.hold_scale = (float)v->valuedouble;
            v = cJSON_GetObjectItem(ch, "hold_dx");
            if (v && v->type == cJSON_Number) d.hold_dx = (float)v->valuedouble;
            v = cJSON_GetObjectItem(ch, "hold_dy");
            if (v && v->type == cJSON_Number) d.hold_dy = (float)v->valuedouble;
            v = cJSON_GetObjectItem(ch, "hold_angle");
            if (v && v->type == cJSON_Number) d.hold_angle_deg = (float)v->valuedouble;

            // 武器类别（可选）：melee / ranged；缺省按 ranged 处理
            v = cJSON_GetObjectItem(ch, "kind");
            if (v && v->type == cJSON_String)
                d.kind = (strcmp(v->valuestring, "melee") == 0) ? 1 : 0;

            // 攻击序列帧条（可选）：用户提供素材后填写 path/frames/fps；
            // 缺失时武器仍会旋转瞄准，仅无额外挥动/枪口帧表现。
            cJSON* atk = cJSON_GetObjectItem(ch, "attack");
            if (atk && atk->type == cJSON_Object) {
                v = cJSON_GetObjectItem(atk, "path");
                if (v && v->type == cJSON_String && v->valuestring && v->valuestring[0])
                    d.attack_path = v->valuestring;
                v = cJSON_GetObjectItem(atk, "frames");
                if (v && v->type == cJSON_Number) d.attack_frames = (int)v->valuedouble;
                v = cJSON_GetObjectItem(atk, "fps");
                if (v && v->type == cJSON_Number) d.attack_fps = (float)v->valuedouble;
            }

            parsed.push_back(d);
            if ((int)parsed.size() >= MAX_WEAPONS) break;
        }
    }
    cJSON_Delete(root);

    if (parsed.empty()) {
        log_error("[weapon] %s has no weapon entry, using built-in defaults", path);
        return;
    }

    defs_.swap(parsed);
    if (!atlas.empty()) atlas_ = atlas;
    cell_px_ = cell;
    byKey_.clear();
    for (const auto& d : defs_) byKey_[d.key] = d.id;
    log_info("[weapon] loaded %s: %d weapon(s)", path, (int)defs_.size());
}

const WeaponDef& WeaponCatalog::get(int id)
{
    if (defs_.empty()) setBuiltinDefault();   // 防御：load 之前被查询时回退内置默认
    if (id < 0 || id >= (int)defs_.size()) id = 0;
    return defs_[id];
}

const WeaponDef* WeaponCatalog::findByName(const std::string& key)
{
    if (defs_.empty()) setBuiltinDefault();
    auto it = byKey_.find(key);
    if (it == byKey_.end()) return nullptr;
    return &defs_[it->second];
}

jrect_t WeaponCatalog::cellRect(const WeaponDef& def) const
{
    jrect_t r;
    r.x = (float)def.col * cell_px_;
    r.y = (float)def.row * cell_px_;
    r.w = cell_px_;
    r.h = cell_px_;
    return r;
}
