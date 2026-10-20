# 可执行文件名（单二进制：单机/Host/客户端/独立服务端，运行时 --mode 切换）
set(EXECUTABLE_NAME "joy_topdown_template")
message(STATUS "EXECUTABLE_NAME = '${EXECUTABLE_NAME}'")


# 如果是 Android，必须命名为 main
if (ANDROID)
    set(MOBILE_ASSETS_DIR "${MOBILE_MAIN_DIR}/assets")
    message(STATUS "MOBILE_ASSETS_DIR = '${MOBILE_ASSETS_DIR}'")
    set(EXECUTABLE_NAME main)
    add_library(${EXECUTABLE_NAME} SHARED)
    # 移动端（触屏模拟成鼠标）下，禁止用触屏位置覆盖鼠标瞄准，避免无鼠标时瞄准错乱
    target_compile_definitions(${EXECUTABLE_NAME} PRIVATE MOBILE_TOUCH)
else()
    add_executable(${EXECUTABLE_NAME})
endif()

# 逻辑分辨率 960x640（480x320 的 2 倍）：PPM 同步翻倍 50 → 100。
# 目的：渲染/文字更精细（字号按 2 倍烘焙），但"可视世界范围 + 角色显示大小"与原来完全一致。
# 若不改 PPM，可视范围会变成 2 倍(19.2x12.8 米)、角色看起来小一半。
# 见 joy/joy.h 中 PIXELS_PER_METER 的覆盖说明；默认 50，仅本项目覆盖。
target_compile_definitions(${EXECUTABLE_NAME} PRIVATE PIXELS_PER_METER=100)

# 源文件与头文件显式列出（不依赖 GLOB）：重命名/删除某文件时若漏改此列表，
# configure 阶段即报错，不会出现"忘了重新 cmake 导致 vcxproj 仍指向已删文件"的静默失败；
# 头文件列入后也会出现在 IDE 工程树中（systems 等纯头文件实现亦可浏览）。
# 新增文件时请在此手动追加。entt.hpp 为第三方大文件，不列入以免 IDE 卡顿。
set(PROJECT_FILES
    # ---- 根目录 ----
    main.cpp
    asset_manager.cpp
    settings.cpp
    asset_manager.h
    context.h
    events.h
    logical_presentation.h
    profiler.h
    protocol.h
    settings.h
    ui_style.h
    # ---- components ----
    components/action_state_component.h
    components/ai_component.h
    components/anim_state_component.h
    components/connection_component.h
    components/enemy_component.h
    components/game_runtime_component.h
    components/global_state_component.h
    components/health_component.h
    components/id_component.h
    components/local_only_component.h
    components/logic_position_component.h
    components/logic_rect_component.h
    components/logic_velocity_component.h
    components/net_component.h
    components/net_session_component.h
    components/pathfinding_component.h
    components/player_component.h
    components/player_event_input.h
    components/player_state_input.h
    components/spawn_point_component.h
    components/system_registry_component.h
    components/transform_component.h
    components/bullet_component.h
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
    # ---- network ----
    network/network_manager.cpp
    network/network_manager.h
    # ---- passes ----
    passes/camera_follow_pass.h
    passes/drawing_collision_debug_pass.h
    passes/drawing_map_pass.h
    passes/drawing_minimap_pass.h
    passes/draw_enemy_pass.h
    passes/draw_fps_pass.h
    passes/draw_player_pass.h
    passes/enemy_counter_hud_pass.h
    passes/fog_pass_2d.h
    passes/frame_display_pass.h
    passes/interpolate_pass.h
    passes/player_status_hud_pass.h
    passes/render_math.h
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
    systems/character_catalog.h
    systems/weapon_catalog.h
    systems/cleanup_system.h
    systems/collision_system.h
    systems/input_consume_input_system.h
    systems/input_consume_join_system.h
    systems/input_consume_leave_system.h
    systems/map_def.h
    systems/map_spawn_system.h
    systems/melee_attack_system.h
    systems/player_shoot_system.h
    systems/bullet_system.h
    systems/movement_system.h
    systems/separation_system.h
    systems/pathfinding_system.h
    systems/player_death_system.h
    systems/snapshot_system.h
    systems/spawn_def.h
    systems/spawn_enemy_system.h
    systems/state_checksum.h
)

# 添加源文件到目标
target_sources(${EXECUTABLE_NAME}
    PRIVATE
        ${PROJECT_FILES}
        iosLaunchScreen.storyboard   # 直接使用文件名，已在当前目录
)

# Apple 平台额外图标
if(APPLE)
    add_definitions(-DLUA_USE_IOS)
    target_sources(${EXECUTABLE_NAME} PRIVATE logo.png)
endif()

# 设置语言标准：编译标准保持 C++17（entt.hpp 等第三方库依赖 C++17 特性，无法降级）。
# 但项目自身业务代码约定只用 C++11 语法与库（见 map_def/protocol 等文件），便于阅读维护。
set_target_properties(${EXECUTABLE_NAME} PROPERTIES
    CXX_STANDARD 17
    CXX_STANDARD_REQUIRED ON
)

# MSVC 下源文件按 UTF-8 解析（项目文件为 UTF-8 无 BOM，避免 GBK 代码页误读中文注释吞行）
if(MSVC)
    target_compile_options(${EXECUTABLE_NAME} PRIVATE /utf-8)
endif()

# Web 平台输出 HTML
if(EMSCRIPTEN)
    set(CMAKE_EXECUTABLE_SUFFIX ".html" CACHE INTERNAL "")
endif()

# 引入 joy 框架源码（纯文件引入，不再构建独立库 target）
include(${CMAKE_SOURCE_DIR}/joy/joy_add.cmake)
joy_add_sources(${EXECUTABLE_NAME})

# 添加项目自身目录为头文件搜索路径
target_include_directories(${EXECUTABLE_NAME} PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")

# ------------------- 资产处理 -------------------
# 资产目录位于根目录的 assets/，使用 PROJECT_SOURCE_DIR 定位
# CMAKE_CURRENT_SOURCE_DIR换成项目目录
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

# 根据不同平台处理资产
if (APPLE)
    # macOS/iOS: 放入 Resources 目录
    macro(add_resource FILE)
        file(RELATIVE_PATH relpath "${ASSETS_ROOT}" "${FILE}")
        get_filename_component(dir "${relpath}" DIRECTORY)
        target_sources(${EXECUTABLE_NAME} PRIVATE "${FILE}")
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
    message(STATUS "PRELOAD_FLAGS: ${PRELOAD_FLAGS}")
    # Emscripten WebSocket 支持（使用浏览器原生 WebSocket API）
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
        add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "${ASSETS_ROOT}/${filename}" "${outname}"
        )
    endmacro()

    # 关卡目录先清空再拷贝：copy_if_different 只增不删，若只靠下面的 foreach 拷贝，
    # 从 assets 删掉的旧关卡(.lua)会留在运行目录变成"幽灵关卡"——游戏会当它已制作并加载
    # （level_map_available 查的就是运行路径），导致选关页显示可进、进游戏却用到过期地图。
    # 这里只清 levels/，不影响其他资产；随后 foreach 会立刻把现行 levelN.lua 拷回来。
    add_custom_command(TARGET ${EXECUTABLE_NAME} POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E remove_directory
                "${CMAKE_BINARY_DIR}/$<CONFIGURATION>/joy_topdown_template/levels"
        COMMENT "Cleaning stale level maps in output dir"
    )

    foreach(asset_file ${ASSET_FILES})
        message(STATUS "Copying asset: ${asset_file}")
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
source_group(TREE "${CMAKE_CURRENT_SOURCE_DIR}/.." FILES ${PROJECT_FILES})

# 调试信息（可选）
message(STATUS "Project current dir: ${CMAKE_CURRENT_LIST_DIR}")
message(STATUS "Project source dir: ${CMAKE_CURRENT_SOURCE_DIR}")
message(STATUS "Project binary dir: ${CMAKE_CURRENT_BINARY_DIR}")