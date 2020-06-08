#version 120
attribute vec3 pos;
attribute vec2 _UNUSED_texCoord;

void main() {
    gl_Position = vec4(pos, 1.0);
}
