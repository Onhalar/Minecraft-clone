#version 330 core

in vec3 currentPosition;
in vec2 texturePosition;

uniform sampler2D texture0;

out vec4 FragColor;

void main() {
    FragColor = texture(texture0, texturePosition);
    //FragColor = vec4(1.0, 1.0, 1.0, 1.0);
}