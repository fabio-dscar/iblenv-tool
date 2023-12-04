#include <common.frag>

in vec3 WorldPos;
out vec4 FragColor;

layout(location = 0) uniform mat4 Projection;
layout(location = 1) uniform mat4 View;
layout(location = 2) uniform sampler2D RectMap;

void main() {
    vec2 uv = SphericalUVMap(normalize(WorldPos));
    vec3 color = texture(RectMap, uv).rgb;

    FragColor = vec4(color, 1.0);
}