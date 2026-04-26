#include "biom.hpp"
#include "glm/fwd.hpp"
#include "shader.hpp"
#include <core.hpp>
#include <config.hpp>
#include <cstdio>
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

#define WORLD_WIDTH 16
#define WORLD_LENGTH 16

world::Chunk* chunks[WORLD_WIDTH * WORLD_LENGTH];

void renderSetup() {
    world::chunkGenerator generator = world::chunkGenerator();
    world::biom* plainsBiom = &world::biomRegistry["plains"];

    for (unsigned int x = 0; x < WORLD_WIDTH; ++x) {
        for (unsigned int y = 0; y < WORLD_LENGTH; ++y) {
            chunks[x * WORLD_WIDTH + y] = generator.generate({x, y}, plainsBiom);
            printf("Making chunk: %u, %u\n", x, y);
        }
    }

    printf("%s", "\nMeshing... "); fflush(stdout);
    world::chunkRegistry::uploadMeshes(true);
    printf("%s", "done\n");
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