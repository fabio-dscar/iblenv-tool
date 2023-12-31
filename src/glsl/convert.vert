layout(location = 0) in vec3 Position;

out vec3 WorldPos;

layout(location = 0) uniform mat4 Projection;
layout(location = 1) uniform mat4 View;
layout(location = 2) uniform mat4 Model;

void main() {
    WorldPos = Position;
    gl_Position = Projection * View * Model * vec4(WorldPos, 1.0);
}