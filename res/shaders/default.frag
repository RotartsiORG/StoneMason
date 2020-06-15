#version 120

varying vec2 varyTexCoords;

uniform sampler2D slot0;

void main() {
    gl_FragColor = texture2D(slot0, varyTexCoords);
}
