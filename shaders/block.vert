#version 330 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 UV;

out vec3 currentPosition;
out vec2 texturePosition;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main() {
    // Transform vertex position to world space
    currentPosition = vec3(model * vec4(vertexPos, 1.0f));

    texturePosition = UV;
    // Transform vertex to clip space
    gl_Position = projection * view * model * vec4(vertexPos, 1.0f);
}