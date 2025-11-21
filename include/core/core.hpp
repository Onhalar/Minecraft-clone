#ifndef CORE_PROJECT_HEADER
#define CORE_PROJECT_HEADER

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <string>

#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <filesystem>

#include <shader.hpp>
#include <unordered_map>

#include <texture.hpp>

#ifndef STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>
#endif

using shaderList = std::unordered_map<std::string, Shader*>;
inline shaderList Shaders = {};
inline std::filesystem::path shaderPath("shaders/");

#include <camera.hpp>
inline Camera* currentCamera = nullptr;
inline Texture* mainTextureAtlas = nullptr;
inline GLFWwindow* mainWindow = nullptr;


// project path
inline std::filesystem::path projectDir("/");
inline std::string projectPath(const std::string& path) { return (projectDir / std::filesystem::path(path)).string(); }
inline std::string projectPath(const std::filesystem::path& path) { return (projectDir / path).string(); }


#endif // CORE_PROJECT_HEADER