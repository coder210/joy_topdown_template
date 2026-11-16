/* ============================================================
 * joy_topdown_template · 通用宿主（cpp 脚本载体）
 *
 * 这里没有游戏逻辑：窗口、场景、物理、网络、绘制、音频全在游戏插件里
 * （game.cpp → game.dll）。宿主只做四件事：
 *   1. SDL 主回调（SDL_AppInit / Event / Iterate / Quit）；
 *   2. 把 CWD 对齐到 exe 目录（资源与插件都是相对路径）；
 *   3. 运行时加载游戏插件，取唯一入口 joy_game_get_hooks()；
 *   4. 把 SDL 事件与每帧驱动转发给插件；退出时先 shutdown 再卸载。
 *
 * 这样同一个 exe 可以跑不同游戏插件：`joy_topdown_template --game=other.dll`。
 *
 * 后台验证（CI / 冒烟）：`--frames N` 跑 N 帧后自动退出，配合
 * SDL_VIDEODRIVER=dummy 可在无显示器环境验证启动是否正常。
 * ============================================================ */
#define SDL_MAIN_USE_CALLBACKS 1
#include <SDL3/SDL_main.h>
#include <SDL3/SDL.h>

#include "game_hooks.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(_WIN32)
#  include <direct.h>
#  define HOST_CHDIR _chdir
#else
#  include <unistd.h>
#  define HOST_CHDIR chdir
#endif

#ifndef GAME_DLL_NAME
#  if defined(_WIN32)
#    define GAME_DLL_NAME "game.dll"
#  elif defined(__APPLE__)
#    define GAME_DLL_NAME "libgame.dylib"
#  else
#    define GAME_DLL_NAME "libgame.so"
#  endif
#endif

typedef const GameHooks* (*joy_game_get_hooks_fn)(void);

#ifdef GAME_STATIC
/* 静态形态（iOS / wasm / Android 单库）：插件已编进本 exe，链接期解析。 */
extern "C" const GameHooks* joy_game_get_hooks(void);
#endif

static SDL_SharedObject* g_lib   = NULL;
static const GameHooks*  g_hooks = NULL;
static int               g_frames_limit = 0;   /* >0 时跑满即退出（冒烟） */
static int               g_frame_count  = 0;

static const char* find_arg_value(int argc, char* argv[], const char* prefix)
{
    const size_t n = strlen(prefix);
    for (int i = 1; i < argc; ++i) {
        if (strncmp(argv[i], prefix, n) == 0) return argv[i] + n;
    }
    return NULL;
}

/* 资源（assets/ 铺在 exe 旁）与游戏插件都是相对路径：先把 CWD 对齐到 exe 目录，
 * 否则从 IDE / 仓库根 / 快捷方式启动会找不到资源，贴图静默退化成纯色块。 */
static void align_working_dir()
{
    const char* base = SDL_GetBasePath();   /* SDL3.2+ 返回内部缓存的只读路径 */
    if (!base) return;
    if (HOST_CHDIR(base) != 0) {
        fprintf(stderr, "[host] chdir(%s) failed\n", base);
    }
}

static bool validate_hooks(const GameHooks* hooks)
{
    if (!hooks || hooks->abi_version != GAME_ABI ||
        hooks->size < sizeof(GameHooks) ||
        !hooks->init || !hooks->on_event || !hooks->on_frame || !hooks->shutdown) {
        fprintf(stderr, "[host] 插件与宿主契约不匹配（abi/size/hooks）\n");
        return false;
    }
    return true;
}

static bool load_game(const char* name)
{
#ifdef GAME_STATIC
    (void)name;
    g_hooks = joy_game_get_hooks();
    return validate_hooks(g_hooks);
#else
    char path[1024];
    const char* base = SDL_GetBasePath();
    if (base) {
        snprintf(path, sizeof(path), "%s%s", base, name);
    } else {
        snprintf(path, sizeof(path), "%s", name);
    }

    g_lib = SDL_LoadObject(path);
    if (!g_lib) {
        /* 回退：按裸名字交给系统搜索（CWD 已对齐 exe 目录） */
        g_lib = SDL_LoadObject(name);
    }
    if (!g_lib) {
        fprintf(stderr, "[host] 加载游戏插件失败: %s (%s)\n", path, SDL_GetError());
        return false;
    }

    joy_game_get_hooks_fn get_hooks =
        reinterpret_cast<joy_game_get_hooks_fn>(SDL_LoadFunction(g_lib, "joy_game_get_hooks"));
    if (!get_hooks) {
        fprintf(stderr, "[host] 插件缺少 joy_game_get_hooks: %s\n", SDL_GetError());
        return false;
    }

    g_hooks = get_hooks();
    return validate_hooks(g_hooks);
#endif
}

/* ---------------- SDL 入口 ---------------- */
SDL_AppResult SDL_AppInit(void** appstate, int argc, char* argv[])
{
    (void)appstate;

    align_working_dir();

    const char* game_name = find_arg_value(argc, argv, "--game=");
    const char* frames    = find_arg_value(argc, argv, "--frames=");
    if (!frames) frames = find_arg_value(argc, argv, "--smoke=");
    if (frames) g_frames_limit = atoi(frames);

    if (!load_game(game_name ? game_name : GAME_DLL_NAME)) return SDL_APP_FAILURE;

    if (!g_hooks->init(g_hooks->game, argc, argv)) {
        fprintf(stderr, "[host] 游戏初始化失败\n");
        return SDL_APP_FAILURE;
    }

    printf("[host] game plugin = %s (contract abi=%u)\n",
           game_name ? game_name : GAME_DLL_NAME, GAME_ABI);
    fflush(stdout);
    return SDL_APP_CONTINUE;
}

SDL_AppResult SDL_AppEvent(void* appstate, SDL_Event* e)
{
    (void)appstate;
    if (!g_hooks) return SDL_APP_SUCCESS;

    switch (g_hooks->on_event(g_hooks->game, e)) {
    case 1:  return SDL_APP_SUCCESS;
    case -1: return SDL_APP_FAILURE;
    default: return SDL_APP_CONTINUE;
    }
}

SDL_AppResult SDL_AppIterate(void* appstate)
{
    (void)appstate;
    if (!g_hooks) return SDL_APP_SUCCESS;

    g_hooks->on_frame(g_hooks->game);

    if (g_frames_limit > 0 && ++g_frame_count >= g_frames_limit) {
        printf("[host] 冒烟结束: frames=%d\n", g_frame_count);
        fflush(stdout);
        return SDL_APP_SUCCESS;
    }
    return SDL_APP_CONTINUE;
}

void SDL_AppQuit(void* appstate, SDL_AppResult result)
{
    (void)appstate;
    (void)result;

    /* 先让插件释放全部 joy 句柄，再卸载库；顺序反了会留下悬空调用 */
    if (g_hooks && g_hooks->shutdown) g_hooks->shutdown(g_hooks->game);
    g_hooks = NULL;
#ifndef GAME_STATIC
    if (g_lib) {
        SDL_UnloadObject(g_lib);
        g_lib = NULL;
    }
#endif
}
