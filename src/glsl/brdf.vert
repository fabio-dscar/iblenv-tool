layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoords;

out vec2 oTexCoords;

void main() {
    oTexCoords = TexCoords;
    gl_Position = vec4(Position, 1.0);
}