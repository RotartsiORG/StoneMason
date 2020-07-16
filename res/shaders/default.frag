#version 330 core

in vec2 varyTexCoords;
out vec4 fragColor;

uniform sampler2D slot0;

void main() {
    fragColor = texture(slot0, varyTexCoords);
}
