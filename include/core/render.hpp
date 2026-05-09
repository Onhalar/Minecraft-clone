#ifndef CORE_RENDER_HEADER
#define CORE_RENDER_HEADER

#include <mesh.hpp>
#include <texture.hpp>
#include <shader.hpp>
#include <camera.hpp>

inline Texture* mainTextureAtlas = nullptr;
inline Camera* currentCamera = nullptr;

inline std::vector<Camera*> Cameras = {};

using shaderList = std::unordered_map<std::string, Shader*>;
inline shaderList Shaders = {};
inline std::filesystem::path shaderPath("shaders/");

inline mesh::MassRenderer* chunkRenderer = nullptr;

inline void makeCameraCurrent(Camera* camera) {
    currentCamera = camera;
    for (auto shader : Shaders) { camera->updateProjection(shader.second); }
}

#endif // CORE_RENDER_HEADER