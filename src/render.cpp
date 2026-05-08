#include "chunkWorker.hpp"
#include <core.hpp>
#include <config.hpp>
#include <globals.hpp>
#include <render.hpp>
#include <types.hpp>

#include <debug.hpp>
#include <FormatConsole.hpp>
#include <shader.hpp>

#include <mesh.hpp>
#include <chunk.hpp>
#include <chunkGeneration.hpp>

#include <VAO.hpp>
#include <VBO.hpp>
#include <EBO.hpp>

#include <cstdio>


#define WORLD_WIDTH 32
#define WORLD_LENGTH 32

world::Chunk* chunks[WORLD_WIDTH * WORLD_LENGTH];

void renderSetup() {
}

void render() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    static Shader* blockShader = Shaders["block"];
    mainTextureAtlas->bind();

    chunkRenderer->render(blockShader, world::chunkRegistry::visibleChunks);

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