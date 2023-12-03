layout(location = 0) in vec3 Position;
layout(location = 1) in vec2 TexCoords;

out vec2 fsTexCoords;

void main() {
    fsTexCoords = TexCoords;
    gl_Position = vec4(Position, 1.0);
}