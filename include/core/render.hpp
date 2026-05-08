#ifndef CORE_RENDER_HEADER
#define CORE_RENDER_HEADER

#include <camera.hpp>
#include <mesh.hpp>
#include <texture.hpp>
#include <shader.hpp>

inline Camera* currentCamera = nullptr;
inline Texture* mainTextureAtlas = nullptr;

using shaderList = std::unordered_map<std::string, Shader*>;
inline shaderList Shaders = {};
inline std::filesystem::path shaderPath("shaders/");

inline mesh::MassRenderer* chunkRenderer = nullptr;


#endif // CORE_RENDER_HEADER