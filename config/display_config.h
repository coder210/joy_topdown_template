#ifndef CONFIG_DISPLAY_CONFIG_H
#define CONFIG_DISPLAY_CONFIG_H

// ============================================================
// 显示配置的唯一事实源（默认值）。
//
// 实际构建时由 CMakeLists.txt 的 DISPLAY_SCALE 统一推导，并用 -D 覆盖：
//   DISPLAY_SCALE=1 → 窗口/逻辑 480x320，PPM=50， 字号 24/14/28
//   DISPLAY_SCALE=2 → 窗口/逻辑 960x640，PPM=100，字号 48/28/56
//
// 调整分辨率只改 CMake 的 DISPLAY_SCALE 一个值。
// 不要在其它文件里另写窗口尺寸 / 逻辑高度 / 字号绝对值。
//
// 注意：PIXELS_PER_METER 不在此处定义——它由 joy.h 的
// `#ifndef PIXELS_PER_METER` 兜底（默认 50），CMake 以 -D 覆盖。
// ============================================================

#ifndef DISPLAY_WINDOW_W
#define DISPLAY_WINDOW_W 480
#endif

#ifndef DISPLAY_WINDOW_H
#define DISPLAY_WINDOW_H 320
#endif

// 逻辑高度（= 窗口高度 × 倍率）。UI 设计稿基准恒为 480x320，
// 各场景用 `s = lh / 320` 做比例缩放，因此这里只影响"逻辑像素总数"。
#ifndef DISPLAY_DESIGN_H
#define DISPLAY_DESIGN_H 320
#endif

// 字号（烘焙尺寸，逻辑像素）
#ifndef DISPLAY_FONT_MAIN
#define DISPLAY_FONT_MAIN 24
#endif
#ifndef DISPLAY_FONT_SMALL
#define DISPLAY_FONT_SMALL 14
#endif
#ifndef DISPLAY_FONT_LARGE
#define DISPLAY_FONT_LARGE 28
#endif

#endif // CONFIG_DISPLAY_CONFIG_H
