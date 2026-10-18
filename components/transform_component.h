#ifndef TRANSFORM_COMPONENT_H
#define TRANSFORM_COMPONENT_H
#include <joy/jmath.h>

struct TransformComponent {
	vec2_t position;
	float rotation;
	vec2_t scale;
	// 渲染插值是否已初始化：首帧把 position/rotation 直接对齐到逻辑值，避免从 (0,0) 滑入；
	// 之后每帧用指数平滑朝逻辑值逼近（圆角 + 补帧）。仅客户端渲染用，不参与帧同步。
	bool interp_init = false;
};

#endif
