#ifndef ASSET_MANAGER_H
#define ASSET_MANAGER_H
#include <joy/joy.h>
#include <string>
#include <map>
#include <unordered_map>

// 资源索引文件（相对运行时工作目录）：集中存放字体 / 地图等非品牌资源的运行时相对路径。
// 代码侧一律按 (category, id) 查表取路径，路径字符串只存在于本文件一处。
// 这是唯一的"引导路径"——索引自身的位置，无法再外提。
#define RESOURCES_JSON_PATH "data/resources.json"

class AssetManager
{
public:
	// 资源类别（resources.json 的一级键）
	static constexpr const char* kCatFonts  = "fonts";
	static constexpr const char* kCatMaps   = "maps";
	static constexpr const char* kCatConfig = "config";   // 配置类资源（如角色表）
	static constexpr const char* kCatAudio  = "audio";    // 可选音频（脚步 / BGM）；缺失即静音

	// 品牌资源：不进索引（直接访问），但路径同样收敛到此处一处常量，
	// 避免同一路径在多个场景里重复书写。
	static constexpr const char* kPathBrandingLogo = "branding/logo.png";

	static AssetManager* GetInstance();
	static void Init();
	static void Destroy();

	AssetManager(const AssetManager&) = delete;
	AssetManager& operator=(const AssetManager&) = delete;

	~AssetManager();

	font_p GetSimheiFont24();
	font_p GetSimheiFont12();
	font_p GetSimheiFont28();   // 超采样字体：28px 栅格化，绘制时缩到 14px，文字更锐利

	// 按 (类别, ID) 查资源运行时相对路径；未收录返回 nullptr（调用方应回退内置默认路径）。
	// 例：GetResourcePath(kCatFonts, "simhei") / GetResourcePath(kCatMaps, "level1")
	const char* GetResourcePath(const char* category, const char* id) const;

	// 清空纹理缓存（释放所有纹理）
	void ClearTexCache();

	// 按路径取纹理（带缓存）。命中直接返回；否则 image_create 并缓存，
	// 失败也入表避免每帧重试。纹理生命周期由 ClearTexCache / 析构统一释放。
	// 注意：调用方需已持有有效的 r2d（纹理依赖渲染设备上下文）。
	image_p GetImage(jr2d_p r2d, const char* path);

private:
	explicit AssetManager();

	// 解析 resources.json 填充 resource_paths_（覆盖内置默认值）。
	// 缺失或解析失败时保留内置默认值，保证游戏仍可运行。
	void load_resources_json();

	static AssetManager* instance_;
	font_p simhei_font24_;
	font_p simhei_font12_;
	font_p simhei_font28_;
	std::map<std::string, std::string> resource_paths_;  // key: "category/id" → 运行时相对路径
	std::unordered_map<std::string, image_p> tex_cache_;
};

#endif // ASSET_MANAGER_H
