#include "asset_manager.h"
#include <joy/external/cJSON.h>
#include <cstdio>
#include <cstring>

// 内置默认关卡数（与 systems/level_sources.h 的 LEVEL_SOURCES_COUNT 对齐）。
// 仅用于 resources.json 缺失时生成兜底地图路径。
static const int kDefaultLevelCount = 5;

// 静态成员初始化
AssetManager* AssetManager::instance_ = nullptr;

AssetManager::AssetManager()
	: simhei_font24_(nullptr), simhei_font12_(nullptr), simhei_font28_(nullptr)
{
	// 内置默认路径：resources.json 缺失/解析失败时的兜底，保证游戏仍可运行。
	// 真源是 resources.json，加载成功后会逐条覆盖这里的值。
	resource_paths_[std::string(kCatFonts) + "/simhei"] = "joy_topdown_template/fonts/simhei.ttf";
	for (int i = 1; i <= kDefaultLevelCount; i++) {
		char key[32], val[128];
		snprintf(key, sizeof(key), "%s/level%d", kCatMaps, i);
		snprintf(val, sizeof(val), "joy_topdown_template/levels/level%d.lua", i);
		resource_paths_[key] = val;
	}
	// 音频为可选资源：文件不存在时 audio_*_create 返回 NULL，而 joy 的 set_gain / play 对 NULL 安全，
	// 游戏静音继续运行。真实文件名以 resources.json 的 audio 段为准。
	resource_paths_[std::string(kCatAudio) + "/footstep"] = "joy_topdown_template/audio/footstep.wav";
	resource_paths_[std::string(kCatAudio) + "/bgm"]      = "joy_topdown_template/audio/bgm.wav";

	// 用索引文件覆盖默认值（资源路径的唯一真源）
	load_resources_json();

	// 字体：按 ID 查表取路径（路径不再硬编码在本文件）
	const char* font_path = GetResourcePath(kCatFonts, "simhei");
	simhei_font24_ = font_create(font_path, 24);
	simhei_font12_ = font_create(font_path, 14);
	simhei_font28_ = font_create(font_path, 28);
}

AssetManager::~AssetManager()
{
	if (simhei_font24_)
		font_destroy(simhei_font24_);
	if (simhei_font12_)
		font_destroy(simhei_font12_);
	if (simhei_font28_)
		font_destroy(simhei_font28_);
	ClearTexCache();
}

void AssetManager::load_resources_json()
{
	std::FILE* f = std::fopen(RESOURCES_JSON_PATH, "rb");
	if (!f) {
		log_info("[asset] %s not found, using built-in defaults", RESOURCES_JSON_PATH);
		return;
	}
	std::fseek(f, 0, SEEK_END);
	long len = std::ftell(f);
	std::fseek(f, 0, SEEK_SET);
	if (len <= 0) { std::fclose(f); return; }

	std::string text((size_t)len, '\0');
	size_t got = std::fread(&text[0], 1, (size_t)len, f);
	std::fclose(f);
	if (got != (size_t)len) return;

	cJSON* root = cJSON_Parse(text.c_str());
	if (!root) {
		log_error("[asset] parse %s failed, using built-in defaults", RESOURCES_JSON_PATH);
		return;
	}

	// cJSON v1.x 无 cJSON_ArrayForEach / cJSON_IsString 等宏，
	// 直接走 child/next 链表并用 type 字段判断类型。
	// 结构：{ "<category>": { "<id>": "<path>", ... }, ... }
	for (cJSON* cat = root->child; cat; cat = cat->next) {
		if (cat->type != cJSON_Object || !cat->string) continue;
		for (cJSON* it = cat->child; it; it = it->next) {
			if (it->type != cJSON_String || !it->string || !it->valuestring) continue;
			resource_paths_[std::string(cat->string) + "/" + it->string] = it->valuestring;
		}
	}
	cJSON_Delete(root);
	log_info("[asset] loaded %s: %d entries", RESOURCES_JSON_PATH, (int)resource_paths_.size());
}

void AssetManager::Init()
{
	if (instance_ == nullptr) {
		instance_ = new AssetManager();
	}
}

void AssetManager::Destroy()
{
	if (instance_) {
		delete instance_;
		instance_ = nullptr;
	}
}

AssetManager* AssetManager::GetInstance()
{
	return instance_;
}

const char* AssetManager::GetResourcePath(const char* category, const char* id) const
{
	if (!category || !id) return nullptr;
	std::string key = std::string(category) + "/" + id;
	std::map<std::string, std::string>::const_iterator it = resource_paths_.find(key);
	return (it == resource_paths_.end()) ? nullptr : it->second.c_str();
}

font_p AssetManager::GetSimheiFont24()
{
	return simhei_font24_;
}

font_p AssetManager::GetSimheiFont12()
{
	return simhei_font12_;
}

font_p AssetManager::GetSimheiFont28()
{
	return simhei_font28_;
}


void AssetManager::ClearTexCache()
{
	for (auto& kv : tex_cache_)
		if (kv.second) image_destroy(kv.second);
	tex_cache_.clear();
}

image_p AssetManager::GetImage(jr2d_p r2d, const char* path)
{
	if (!path || !path[0]) return nullptr;
	std::string key(path);
	auto it = tex_cache_.find(key);
	if (it != tex_cache_.end()) return it->second;
	image_p img = image_create(r2d, path);   // 纹理依赖 r2d，调用方须已初始化渲染上下文
	if (!img) log_error("[asset] image load failed: %s", path);
	tex_cache_[key] = img;   // 失败也缓存，避免每帧重试同一坏路径
	return img;
}
