#include <config.hpp>
#include <types.hpp>
#include <core.hpp>
#include <debug.hpp>
#include <FormatConsole.hpp>

#include <UBO.hpp>
#include <shader.hpp>
#include <camera.hpp>

void setupShaderMetrices(Shader* shader);


void setupShaders() {
    struct shaderSource {
        std::filesystem::path vertex;
        std::filesystem::path fragment;
    };

    std::unordered_map<std::string, shaderSource> shaderSourceFiles;

    // loading and sorting source files
    for (const auto& file : std::filesystem::recursive_directory_iterator(projectPath(shaderPath))) {
        auto filepath = file.path();
        if (filepath.extension() == ".vert") {
            shaderSourceFiles[filepath.stem().string()].vertex = filepath;
        }
        else if (filepath.extension() == ".frag") {
            shaderSourceFiles[filepath.stem().string()].fragment = filepath;
        }
    }

    // attempting to make shaders from source files
    TinyUInt failed = 0;

    for (const auto& shaderSource : shaderSourceFiles) {
        if (!shaderSource.second.vertex.empty() && !shaderSource.second.fragment.empty()) {
            Shaders[shaderSource.first] = new Shader(shaderSource.second.vertex, shaderSource.second.fragment);
            setupShaderMetrices(Shaders[shaderSource.first]);
        }
        else { failed++; }
    }

    if (debugMode) {
        std::cout << "\n" << formatProcess("Loaded ") << Shaders.size() << " Shader" << ((Shaders.size() > 1u) ? "s" : "") << ((failed > 0u) ? &"; failed " [ failed] : "") << std::endl;
    }
}

void setupShaderMetrices(Shader* shader) {
    int windowWidth, windowHeight;
    glfwGetFramebufferSize(mainWindow, &windowWidth, &windowHeight);

    shader->activate();

    // Initialize the camera (make sure it's only initialized once)
    if (!currentCamera) {
        currentCamera = new Camera(windowWidth, windowHeight, glm::vec3(0.5f, -5.0f, 1.0f));
    }

    currentCamera->updateProjection(windowWidth, windowHeight, shader);

    // Initial application of the model matrix (can be overridden in render loop)
    shader->applyModelMatrix();
}

void APIENTRY MessageCallback(GLenum source, GLenum type, GLuint id, GLenum severity,
    GLsizei length, const GLchar *message, const void *userParam) {
    std::cout << formatError("OpenGL Debug") << ": " << colorText(message, ANSII_YELLOW) << std::endl;
}