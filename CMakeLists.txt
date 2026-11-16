# ============================================================
# joy_topdown_template —— 薄宿主 + cpp 脚本插件（native plugin）
#
#   joy_topdown_template(.exe)  宿主：只链接 SDL3，负责 SDL 主回调、加载插件、
#                               转发事件与每帧驱动（host.cpp，无游戏逻辑）
#   game(.dll/.so/.dylib)       游戏插件（cpp 脚本）：窗口/场景/物理/网络/绘制
#                               全在这里，自身链接 joy::all（引擎只有这一份）
#
# 静态形态（GAME_STATIC，iOS / wasm / Android 默认开启）把游戏插件直接编进
# 宿主，链接期解析 joy_game_get_hooks()，避开代码签名 / 动态链接配置。
#
# 与 joy examples/cpp 的差别：官方示例的游戏库只链接 JoyApi 函数指针表，因此
# 只能用 IDL 里 status:on 的 API；本模板是完整游戏，用到日志 / 文件 IO / 控件 /
# msgpack / tilemap 等 vtable 未覆盖能力，所以游戏库自链接 joy::all。
#
# 独立仓库模式：本文件直接作为仓库根 CMakeLists 使用（拆仓后）。
# 单仓模式（被上层根 CMake add_subdirectory）下跳过顶层块，行为不变。
# ============================================================
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    cmake_minimum_required(VERSION 3.16)
    project(joy_topdown_template C CXX)

    set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
    set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
    set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>")
    set(CMAKE_INSTALL_PREFIX "${CMAKE_BINARY_DIR}" CACHE INTERNAL "")

    if ((APPLE AND NOT CMAKE_SYSTEM_NAME MATCHES "Darwin") OR EMSCRIPTEN)
        set(BUILD_SHARED_LIBS OFF CACHE INTERNAL "")
        set(SDL_SHARED OFF)
    else()
        set(SDL_SHARED ON)
    endif()

    if(MSVC)
        if(NOT CMAKE_GENERATOR STREQUAL "Ninja")
            add_definitions(/MP)
        endif()
        set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()

    # 第三方源码：引擎以 submodule 形式位于 third_party/joy（含 SDL / msgpack / spine）
    set(JOY_REPO_DIR "${CMAKE_CURRENT_SOURCE_DIR}/third_party/joy")
    add_subdirectory("${JOY_REPO_DIR}/msgpack" "${CMAKE_BINARY_DIR}/msgpack")
    add_subdirectory("${JOY_REPO_DIR}/spine"   "${CMAKE_BINARY_DIR}/spine")
    add_subdirectory("${JOY_REPO_DIR}/SDL"     "${CMAKE_BINARY_DIR}/SDL")

    enable_testing()
endif()

# 引擎注入入口：joy::all 是 INTERFACE 源码注入 target，谁链接它、joy 的 .c 就
# 编进谁。只有游戏插件链接它；宿主不链接，否则同一进程会有两套引擎状态和
# 两套 joy_obj 句柄表。
if(NOT TARGET joy_all)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/third_party/joy"
                     "${CMAKE_BINARY_DIR}/joy")
endif()

# 可执行文件名（宿主：单机/Host/客户端/独立服务端，运行时 --mode 切换）
set(EXECUTABLE_NAME "joy_topdown_template")
set(GAME_TARGET "game")
message(STATUS "EXECUTABLE_NAME = '${EXECUTABLE_NAME}', game plugin = '${GAME_TARGET}'")

# ---- 显示配置：唯一配置点 ----
# DISPLAY_SCALE=1 → 480x320（设计基准）；2 → 960x640。
# 窗口 / 逻辑高度 / PPM / 字号全部由它推导；业务代码只引用 config/display_config.h 的宏。
# 例：cmake -DDISPLAY_SCALE=2 .. 即整体放大到 960x640（可视世界仍为 9.6x6.4 米）。
set(DISPLAY_SCALE 1 CACHE STRING "渲染倍率：1=480x320, 2=960x640")
math(EXPR DISPLAY_W          "480 * ${DISPLAY_SCALE}")
math(EXPR DISPLAY_H          "320 * ${DISPLAY_SCALE}")
math(EXPR DISPLAY_PPM        "50  * ${DISPLAY_SCALE}")
math(EXPR DISPLAY_FONT_MAIN  "24  * ${DISPLAY_SCALE}")
math(EXPR DISPLAY_FONT_SMALL "14  * ${DISPLAY_SCALE}")
math(EXPR DISPLAY_FONT_LARGE "28  * ${DISPLAY_SCALE}")

set(DISPLAY_DEFS
    PIXELS_PER_METER=${DISPLAY_PPM}
    DISPLAY_WINDOW_W=${DISPLAY_W}
    DISPLAY_WINDOW_H=${DISPLAY_H}
    DISPLAY_DESIGN_H=${DISPLAY_H}
    DISPLAY_FONT_MAIN=${DISPLAY_FONT_MAIN}
    DISPLAY_FONT_SMALL=${DISPLAY_FONT_SMALL}
    DISPLAY_FONT_LARGE=${DISPLAY_FONT_LARGE}
)

# ---- 构建形态：iOS / wasm / Android 默认静态（插件编进宿主）----
set(_game_static_default OFF)
if(IOS OR EMSCRIPTEN OR ANDROID)
    set(_game_static_default ON)
endif()
option(JOY_GAME_STATIC
       "把游戏插件编进宿主（iOS / wasm / Android 默认开启）" ${_game_static_default})

# ---- 宿主源码（不含任何游戏逻辑）----
set(HOST_FILES
    host.cpp
    game_hooks.h
)

# ---- 游戏插件源码（cpp 脚本）----
# 源文件与头文件显式列出（不依赖 GLOB）：重命名/删除某文件时若漏改此列表，
# configure 阶段即报错，不会出现"忘了重新 cmake 导致 vcxproj 仍指向已删文件"
# 的静默失败；头文件列入后也会出现在 IDE 工程树中。entt.hpp 为第三方大文件，
# 不列入以免 IDE 卡顿。
set(GAME_FILES
    # ---- config（显示配置：唯一事实源）----
    config/display_config.h
    # ---- 根部 ----
    game.cpp
    asset_manager.cpp
    settings.cpp
    asset_manager.h
    context.h
    events.h
    logical_presentation.h
    profiler.h
    protocol.h
    settings.h
    ui_sfx.h
    ui_style.h
    # ---- components（实体组件）----
    components/action_state_component.h
    components/ai_component.h
    components/anim_state_component.h
    components/bullet_component.h
    components/connection_component.h
    components/enemy_component.h
    components/health_component.h
    components/id_component.h
    components/knockback_component.h
    components/local_only_component.h
    components/logic_position_component.h
    components/logic_rect_component.h
    components/logic_velocity_component.h
    components/pathfinding_component.h
    components/player_component.h
    components/player_event_input.h
    components/player_state_input.h
    components/spawn_point_component.h
    components/transform_component.h
    # ---- world（世界级资源，非实体组件）----
    world/entity_factory.h
    world/frame_command_buffer.h
    world/game_runtime.h
    world/global_state.h
    world/system_registry.h
    # ---- layers ----
    layers/debug_layer.cpp
    layers/gameplay_controls_layer.cpp
    layers/menu_layer.cpp
    layers/pause_menu_layer.cpp
    layers/debug_layer.h
    layers/gameplay_controls_layer.h
    layers/layer.h
    layers/menu_layer.h
    layers/pause_menu_layer.h
    # ---- net（传输抽象 + 帧同步基础设施）----
    net/net_session.h
    net/sync/frame_command_stream.h
    net/transport/i_transport.h
    net/transport/joy_transport.cpp
    net/transport/joy_transport.h
    # ---- network ----
    network/lan_discovery.h
    network/network_manager.cpp
    network/network_manager.h
    network/room_info.h
    # ---- passes ----
    passes/camera_follow_pass.h
    passes/drawing_collision_debug_pass.h
    passes/drawing_map_pass.h
    passes/drawing_minimap_pass.h
    passes/draw_bullet_pass.h
    passes/draw_enemy_pass.h
    passes/draw_fps_pass.h
    passes/draw_player_pass.h
    passes/enemy_counter_hud_pass.h
    passes/fog_pass_2d.h
    passes/frame_display_pass.h
    passes/hpbar.h
    passes/interpolate_pass.h
    passes/player_status_hud_pass.h
    passes/render_math.h
    passes/sprite_anim.h
    # ---- scenes ----
    scenes/character_select_scene.cpp
    scenes/game_scene.cpp
    scenes/game_scene_common.cpp
    scenes/level_select_scene.cpp
    scenes/loading_scene.cpp
    scenes/main_scene.cpp
    scenes/online_scene.cpp
    scenes/scene.cpp
    scenes/scene_manager.cpp
    scenes/settings_scene.cpp
    scenes/splash_scene.cpp
    scenes/victory_scene.cpp
    scenes/character_select_scene.h
    scenes/game_scene.h
    scenes/game_scene_common.h
    scenes/level_select_scene.h
    scenes/loading_scene.h
    scenes/main_scene.h
    scenes/online_scene.h
    scenes/scene.h
    scenes/scene_manager.h
    scenes/settings_scene.h
    scenes/splash_scene.h
    scenes/victory_scene.h
    # ---- systems ----
    systems/character_catalog.cpp
    systems/map_def.cpp
    systems/weapon_catalog.cpp
    systems/ai_system.h
    systems/bullet_system.h
    systems/character_catalog.h
    systems/cleanup_system.h
    systems/collision_system.h
    systems/enemy_maintain_system.h
    systems/input_consume_input_system.h
    systems/input_consume_join_system.h
    systems/input_consume_leave_system.h
    systems/knockback_system.h
    systems/map_def.h
    systems/map_spawn_system.h
    systems/melee_attack_system.h
    systems/movement_system.h
    systems/pathfinding_system.h
    systems/player_death_system.h
    systems/player_shoot_system.h
    systems/separation_system.h
    systems/snapshot_system.h
    systems/spawn_def.h
    systems/spawn_enemy_system.h
    systems/state_checksum.h
    systems/weapon_catalog.h
)

# ------------------- 目标组织 -------------------
if(ANDROID)
    # SDL3 Android 由 SDLActivity dlopen libmain.so 再 dlsym("SDL_main")，不是
    # 独立可执行文件；插件在 Android 默认静态编入，只产出一个 libmain.so。
    add_library(${EXECUTABLE_NAME} SHARED ${HOST_FILES})
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES OUTPUT_NAME "main")
else()
    add_executable(${EXECUTABLE_NAME} ${HOST_FILES})
endif()

# 宿主只依赖 SDL（主回调 / 事件 / 加载器）
target_link_libraries(${EXECUTABLE_NAME} PRIVATE SDL3::SDL3)
target_include_directories(${EXECUTABLE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

if(JOY_GAME_STATIC)
    target_sources(${EXECUTABLE_NAME} PRIVATE ${GAME_FILES})
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE GAME_STATIC=1 ${DISPLAY_DEFS})
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE joy::all)
    set(GAME_DEP_TARGET ${EXECUTABLE_NAME})
else()
    add_library(${GAME_TARGET} SHARED ${GAME_FILES})
    set_target_properties(${GAME_TARGET} PROPERTIES OUTPUT_NAME "game")
    target_link_libraries(${GAME_TARGET} PRIVATE joy::all)
    target_compile_definitions(${GAME_TARGET} PRIVATE ${DISPLAY_DEFS})
    target_include_directories(${GAME_TARGET} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
    # 插件与宿主演进绑定：构建宿主时先构建插件，产物同目录（见输出目录设置）
    add_dependencies(${EXECUTABLE_NAME} ${GAME_TARGET})
    set(GAME_DEP_TARGET ${GAME_TARGET})
endif()

# 注入的 joy 引擎源码在 IDE 里收进 joy\... 筛选器（须在定义 target 的目录调用）
joy_ide_source_groups()

# 设置语言标准：保持 C++17（entt.hpp 等第三方库依赖 C++17 特性，无法降级）。
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)
if(TARGET ${GAME_TARGET})
    set_target_properties(${GAME_TARGET} PROPERTIES
        CXX_STANDARD 17
        CXX_STANDARD_REQUIRED ON
    )
endif()

# MSVC 下源文件按 UTF-8 解析（项目文件为 UTF-8 无 BOM，避免 GBK 代码页误读中文注释吞行）
if(MSVC)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /utf-8)
    if(TARGET ${GAME_TARGET})
        target_compile_options(${GAME_TARGET} PRIVATE /utf-8)
    endif()
endif()

# Web 平台输出 HTML
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

# ------------------- 资产处理 -------------------
# 资产目录位于根目录的 assets/，使用 PROJECT_SOURCE_DIR 定位
set(ASSETS_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/assets")

# 递归搜索资产文件（排除版本控制目录）
file(GLOB_RECURSE ALL_ASSET_FILES "${ASSETS_ROOT}/*")
set(ASSET_FILES "")
foreach(file ${ALL_ASSET_FILES})
    if(NOT file MATCHES "\\.svn/" AND NOT file MATCHES "\\.git/")
        file(RELATIVE_PATH rel_path "${ASSETS_ROOT}" "${file}")
        list(APPEND ASSET_FILES "${rel_path}")
    endif()
endforeach()

if (APPLE)
    # macOS/iOS: 放入 Resources 目录
    macro(add_resource FILE)
        file(RELATIVE_PATH relpath "${ASSETS_ROOT}" "${FILE}")
        get_filename_component(dir "${relpath}" DIRECTORY)
        target_sources(${GAME_DEP_TARGET} PRIVATE "${FILE}")
        set_property(SOURCE "${FILE}" PROPERTY MACOSX_PACKAGE_LOCATION "Resources/${dir}")
    endmacro()
    foreach(asset_file ${ASSET_FILES})
        add_resource("${ASSETS_ROOT}/${asset_file}")
    endforeach()

elseif(EMSCRIPTEN)
    # Emscripten: 使用 --preload-file
    set(PRELOAD_FLAGS "")
    foreach(asset_file ${ASSET_FILES})
        list(APPEND PRELOAD_FLAGS "--preload-file \"${ASSETS_ROOT}/${asset_file}@${asset_file}\"")
    endforeach()
    target_link_libraries(${EXECUTABLE_NAME} PRIVATE
        ${PRELOAD_FLAGS}
    )
else()
    # 其他平台（Windows, Linux, Android 等）
    if(UNIX AND NOT APPLE)
        target_link_libraries(${EXECUTABLE_NAME} PRIVATE m)
    endif()

    # 辅助宏：复制资产到目标位置
    macro(copy_helper filename)
        if (ANDROID)
            # Android: 复制到 MOBILE_ASSETS_DIR（由外部传入）
            if (NOT MOBILE_ASSETS_DIR)
                message(FATAL_ERROR "Building for Android, but MOBILE_ASSETS_DIR is not set")
            endif()
            set(outname "${MOBILE_ASSETS_DIR}/${filename}")
        else()
            # 其他桌面平台：复制到可执行文件同目录
            set(outname "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${filename}")
        endif()
        # 目标目录在部分 CMake 版本下 copy_if_different 不会自动创建，
        # 全新构建时父目录尚不存在会导致 copy 失败；显式 mkdir 保证跨版本一致。
        get_filename_component(_asset_rel_dir "${filename}" DIRECTORY)
        if (ANDROID)
            set(_asset_outdir "${MOBILE_ASSETS_DIR}/${_asset_rel_dir}")
        else()
            set(_asset_outdir "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/${_asset_rel_dir}")
        endif()
        add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "${_asset_outdir}"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASSETS_ROOT}/${filename}" "${outname}"
        )
    endmacro()

    # 关卡目录先清空再拷贝：copy_if_different 只增不删，若只靠下面的 foreach 拷贝，
    # 从 assets 删掉的旧关卡(levelN.tmj)会留在运行目录变成"幽灵关卡"——游戏会当它已制作并加载
    # （level_map_available 查的就是运行路径），导致选关页显示可进、进游戏却用到过期地图。
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/levels"
        COMMENT "Cleaning stale level maps in output dir"
    )

    foreach(asset_file ${ASSET_FILES})
        copy_helper("${asset_file}")
    endforeach()
endif()

# ------------------- 平台特定属性 -------------------
set(RESOURCE_FILES "logo.png")
if(IOS)
    set(RESOURCE_FILES "iosLaunchScreen.storyboard;logo.png")
endif()

set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    MACOSX_BUNDLE TRUE
    MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_SOURCE_DIR}/Info.plist.in"
    XCODE_GENERATE_SCHEME TRUE
    XCODE_ATTRIBUTE_BUNDLE_IDENTIFIER "com.game.${PROJECT_NAME}"
    XCODE_ATTRIBUTE_PRODUCT_BUNDLE_IDENTIFIER "com.game.${PROJECT_NAME}"
    XCODE_ATTRIBUTE_CURRENTYEAR "${CURRENTYEAR}"
    RESOURCE "${RESOURCE_FILES}"
)
if(MSVC)
    set_target_properties(${EXECUTABLE_NAME} PROPERTIES
        VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_BINARY_DIR}/$<CONFIGURATION>"
    )
endif()
# Visual Studio 启动项目设置（作用于当前目录）
set_property(DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}" PROPERTY VS_STARTUP_PROJECT "${EXECUTABLE_NAME}")

# macOS .app：游戏插件要落在 Contents/MacOS 才能被宿主按相对路径加载
if(APPLE AND NOT IOS AND TARGET ${GAME_TARGET})
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "$<TARGET_FILE:${GAME_TARGET}>"
                "$<TARGET_BUNDLE_CONTENT_DIR:${EXECUTABLE_NAME}>/MacOS/$<TARGET_FILE_NAME:${GAME_TARGET}>"
        COMMENT "Deploying game plugin into the macOS bundle")
endif()

# macOS 非 Xcode 生成器时的 bundle 修复（可选）
if(CMAKE_SYSTEM_NAME MATCHES "Darwin" AND NOT CMAKE_GENERATOR MATCHES "Xcode")
    install(TARGETS ${EXECUTABLE_NAME}
        BUNDLE DESTINATION ./install COMPONENT Runtime
        RUNTIME DESTINATION ./install/bin COMPONENT Runtime
    )
    set(DEP_DIR "${CMAKE_BINARY_DIR}")
    install(CODE "
        include(BundleUtilities)
        fixup_bundle(\"\$<TARGET_BUNDLE_DIR:${EXECUTABLE_NAME}>\" \"\" \"${DEP_DIR}\")
    ")
    set(CPACK_GENERATOR "DragNDrop")
    include(CPack)
endif()

# 源文件分组（便于 IDE 显示）
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${HOST_FILES})
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}" FILES ${GAME_FILES})

# ------------------- 冒烟用例：无显示器跑 N 帧，验证宿主+插件能起来 -------------------
if(NOT ANDROID AND NOT EMSCRIPTEN AND NOT CMAKE_CROSSCOMPILING)
    add_test(NAME template_smoke
             COMMAND ${EXECUTABLE_NAME} --frames=120
             WORKING_DIRECTORY "$<TARGET_FILE_DIR:${EXECUTABLE_NAME}>")
    set_tests_properties(template_smoke PROPERTIES
        ENVIRONMENT "SDL_VIDEODRIVER=dummy;SDL_AUDIODRIVER=dummy;SDL_RENDER_DRIVER=software")
endif()

# 调试信息（可选）
message(STATUS "Project current dir: ${CMAKE_CURRENT_LIST_DIR}")
message(STATUS "Project source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Project binary dir: ${CMAKE_CURRENT_BINARY_DIR}")
