#version 120
attribute vec3 pos;
attribute vec2 texCoord;

uniform mat4 mvp;

varying vec2 varyTexCoords;

void main() {
    gl_Position = mvp * vec4(pos, 1.0);
    varyTexCoords = texCoord;
}
