#include <common.frag>

in vec3 WorldPos;
layout(location = 0) out vec4 FragColor;

layout(location = 3) uniform sampler2D RectMap;

void main() {
    vec2 uv = SphericalUVMap(normalize(WorldPos));
    vec3 color = texture(RectMap, uv).rgb;

    FragColor = vec4(color, 1.0);
}