#ifndef TRANSFORM_COMPONENT_H
#define TRANSFORM_COMPONENT_H
#include <joy/jmath.h>

struct TransformComponent {
	vec2_t position;
	float rotation;
	vec2_t scale;
};

#endif
