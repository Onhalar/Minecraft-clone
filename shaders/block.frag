#version 460 core

in vec2 vUV;

uniform sampler2D texture0;

out vec4 FragColor;

void main() {
    FragColor = texture(texture0, vUV);
    //FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}