# joy_topdown_template

joy 引擎的顶视角（topdown）游戏模板。**游戏逻辑做成「cpp 脚本」——一个原生插件动态库**，
可执行文件本身只是通用宿主。

## 结构

```text
host.cpp           宿主：SDL 主回调 + 加载插件 + 转发事件/每帧驱动（无游戏逻辑，只链 SDL3）
game_hooks.h       宿主 ↔ 插件唯一契约（GAME_ABI / GameHooks）：插件只导出 joy_game_get_hooks()
game.cpp           游戏插件入口：init / on_event / on_frame / shutdown
scenes/ layers/ systems/ passes/ world/ components/ network/ net/
                   游戏本体（窗口、场景、ECS、帧同步、渲染、网络）——都在插件里
assets/            运行时资源（POST_BUILD 拷到 exe 同目录）
third_party/joy    引擎 submodule（含 SDL / msgpack / spine）
```

- 宿主只链接 `SDL3::SDL3`，产出 `joy_topdown_template.exe`；
- 插件链接 `joy::all`（引擎源码注入，全进程只有这一份引擎状态），产出 `game.dll`；
- 静态形态（iOS / wasm / Android 默认开启，`-DJOY_GAME_STATIC=ON`）把插件编进宿主，
  链接期解析 `joy_game_get_hooks()`，避开代码签名 / 动态链接配置。

与 joy `examples/cpp` 的差别：官方示例的插件只通过 `JoyApi` 函数指针表调引擎，因此只能用
`joy.idl` 里 `status:on` 的 API。本模板是完整游戏，用到日志 / 文件 IO / 控件 / msgpack /
tilemap 等 vtable 尚未覆盖的能力，所以插件自链接 `joy::all`。将来这些能力进了 vtable，
可以逐块切到 `JoyApi`，宿主与契约不用改。

## 构建

```powershell
cmake -S . -B build
cmake --build build --config Debug          # 同时构建 game.dll
```

## 运行

```powershell
./build/Debug/joy_topdown_template.exe                  # 默认加载同目录 game.dll
./build/Debug/joy_topdown_template.exe --mode=host      # single|host|client|server
./build/Debug/joy_topdown_template.exe --game=other.dll # 换一个游戏插件
```

无显示器冒烟（CI 用）：

```powershell
$env:SDL_VIDEODRIVER="dummy"; $env:SDL_AUDIODRIVER="dummy"; $env:SDL_RENDER_DRIVER="software"
./build/Debug/joy_topdown_template.exe --frames=120
ctest --test-dir build -C Debug --output-on-failure
```

## 依赖说明

`third_party/joy` 为 git submodule（当前 pin 在 `4e83f145`，`src/j<域>.c/.h` 新布局）。

已知上游问题（本模板已绕开，未改动引擎）：`jmath.h` 里的 static 定点三角函数
`fp_sin` / `fp_cos` 引用 `SIN_LUT` / `tan_lut`，而 `include/joy/jlut.h` 的两张表没加
`extern "C"`，因此 **C++ TU 一旦调用 `fp_sin/fp_cos` 就会 `LNK2001: ?SIN_LUT@@...`**。
本模板的瞄准方向表因此改为「启动时用 double 求值、立即量化成 Q32.32」，运行期仍只有整数查表。
引擎侧的正解是给 `jlut.h` 的两个声明加 `extern "C"` 包裹。
