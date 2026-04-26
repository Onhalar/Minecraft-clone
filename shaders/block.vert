#version 460 core

layout (location = 0) in vec3 vertexPos;
layout (location = 1) in vec2 UV;

layout(std430, binding = 0) readonly buffer ChunkData {
    vec4 chunkPositions[];  // xyz = chunk coord, w unused
};

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

out vec2 vUV;

void main() {
    // chunkPositions holds chunk coords, multiply by 16 to get world space
    gl_Position = projection * view * model * vec4(vertexPos, 1.0);
    vUV = UV;
}