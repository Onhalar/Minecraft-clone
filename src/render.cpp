#include <core.hpp>
#include <config.hpp>
#include <globals.hpp>
#include <types.hpp>
#include <debug.hpp>
#include <FormatConsole.hpp>

#include <mesh.hpp>
#include <chunk.hpp>

#include <VAO.hpp>
#include <VBO.hpp>
#include <EBO.hpp>

Chunk chunk;

void renderSetup() {
    chunk.blockData[0u][1u][0u] = Block(0u, BlockType::solid);
    chunk.blockData[0u][1u][1u] = Block(0u, BlockType::solid);
    chunk.blockData[0u][0u][0u] = Block(0u, BlockType::solid);

    chunk.bakeChunk();
    chunk.generateIntermediateData();
    chunk.stitchMesh();

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

    chunk.mesh->render(Shaders["block"]);

    glfwSwapBuffers(mainWindow);
}

void resize(GLFWwindow *window, int width, int height) {
    if ((width | height) == 0) {
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