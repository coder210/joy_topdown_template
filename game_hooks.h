#ifndef GAME_HOOKS_H
#define GAME_HOOKS_H
/* ============================================================
 * 宿主 <-> 游戏插件（cpp 脚本）的唯一契约
 *
 * 形态对齐 joy 的 examples/cpp：exe 是通用宿主，游戏逻辑编成原生共享库，
 * 运行时由宿主加载并取唯一入口 joy_game_get_hooks()。
 *
 * 与 joy 官方示例的差异（有意为之，见 README）：
 *   官方 cpp 示例的游戏 DLL 只链接 JoyApi 函数指针表，不链接引擎，因此
 *   只能使用 joy.idl 里 status:on 的 739 条 API；本模板是完整游戏，用到
 *   日志 / 文件 IO / 控件 / msgpack / tilemap 等大量 vtable 未覆盖能力，
 *   所以游戏库自己链接 joy::all（引擎与 SDL 各只有一份，在游戏库内），
 *   宿主只有 SDL + 加载器，不链接 joy。
 *
 * 方向约定：
 *   宿主 -> 游戏：SDL 事件原样转发（同一份 SDL3 共享库，ABI 一致）+ 每帧驱动
 *   游戏 -> 宿主：init 失败 / exit_code 通过返回值上报
 * ============================================================ */
#include <stdint.h>
#include <stdbool.h>
#include <SDL3/SDL_events.h>

#define GAME_ABI 1u

typedef struct GameHooks {
        uint32_t abi_version;
        uint32_t size;
        void*    game;   /* 游戏库私有状态，宿主原样回传 */

        /* 建窗口 / 渲染器、加载资源、进入初始场景；false = 初始化失败 */
        bool (*init)(void* game, int argc, char** argv);
        /* 每帧一次；返回 0 = 继续，1 = 正常退出，-1 = 异常退出 */
        int (*on_event)(void* game, const SDL_Event* e);
        /* 每帧驱动（帧率由游戏内部的 game_timer 限制） */
        void (*on_frame)(void* game);
        /* 释放全部 joy 句柄；宿主随后卸载游戏库 */
        void (*shutdown)(void* game);
} GameHooks;

/* 静态形态（iOS / wasm / Android 单库）下直接编进宿主，不导出符号。 */
#if defined(GAME_STATIC)
#  define GAME_EXPORT
#elif defined(_WIN32)
#  define GAME_EXPORT __declspec(dllexport)
#else
#  define GAME_EXPORT __attribute__((visibility("default")))
#endif

#ifdef __cplusplus
extern "C" {
#endif
/* 唯一导出入口：返回静态 hooks 表。 */
GAME_EXPORT const GameHooks* joy_game_get_hooks(void);
#ifdef __cplusplus
}
#endif

#endif /* GAME_HOOKS_H */
