#include "biom.hpp"
#include "glm/fwd.hpp"
#include "shader.hpp"
#include <core.hpp>
#include <config.hpp>
#include <globals.hpp>
#include <types.hpp>
#include <debug.hpp>
#include <FormatConsole.hpp>

#include <mesh.hpp>
#include <chunk.hpp>
#include <chunkGeneration.hpp>

#include <VAO.hpp>
#include <VBO.hpp>
#include <EBO.hpp>

world::Chunk* chunk1;
world::Chunk* chunk2;

void renderSetup() {

    world::chunkGenerator generator = world::chunkGenerator();

    chunk1 = generator.generate({0, 0}, &world::biomRegistry["plains"]);
    chunk2 = generator.generate({0, -1}, &world::biomRegistry["plains"]);

    world::chunkRegistry::stitchRegistryMesh(true, true);

    /*std::cout << "indicies" << std::endl;
    for (int i = 1; i <= chunk.mesh->indices.size(); ++i) {
        std::cout << chunk.mesh->indices[i-1] << (i % 3 == 0 ? "\n" : " ");
    }
    std::cout << "vertices" << std::endl;
    for (int i = 1; i <= chunk.mesh->vertices.size(); ++i) {
        std::cout << chunk.mesh->vertices[i-1] << (i % 3 == 0 ? "\n" : " ");
    }
    std::cout << std::endl;*/
}



void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    mainTextureAtlas->bind();
    world::chunkRegistry::worldMesh.render(Shaders["block"]);

    glfwSwapBuffers(mainWindow);
}



void resize(GLFWwindow *window, int width, int height) {
    if (!(width | height)) {
        isMinimized = true;
        return;
    }
    else { isMinimized = false; }

    // Set the viewport first
    glViewport(0, 0, width, height);
    
    // Ensure the mainShader is active before updating projection
    for (const auto& shaderPair : Shaders) {
        auto shader = shaderPair.second;
        if (shader) {
            shader->activate();
            currentCamera->updateProjection(width, height, shader);
        }
    }
}