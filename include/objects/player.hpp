#ifndef PLAYER_HEADER
#define PLAYER_HEADER

#include "GLFW/glfw3.h"

#include "block.hpp"
#include "chunk.hpp"
#include "chunkWorker.hpp"

#include "core.hpp"
#include "glm/ext/vector_float3.hpp"
#include "glm/geometric.hpp"
#include "globals.hpp"

#include <array>
#include <cstdio>
#include <render.hpp>
#include <physics.hpp>
#include <camera.hpp>

#include <blockRayCast.hpp>

#include <algorithm>

namespace world {
    class player : public physics::physicsObject {
        private:
            const glm::vec3 UP = {0.0f, 0.0f, 1.0f};

        public:
            Camera* playerCamera;

            float baseSpeed = 35.0f;           // Faster acceleration to reach max speed
            float jumpBurstSpeed = 10.5f;      // Jump gives ~1.25 blocks height

            float sprintModifier = 1.65f;

            float friction = 9.0f;            // Strong friction to stop sliding

            float blockReach = 7.5f;

            player() {
                position = glm::vec3(0.5f, 0.5f, 100.0f);
                velocity = glm::vec3(0.0f);
                mass = 150.0f;

                int windowWidth, windowHeight;
                glfwGetFramebufferSize(mainWindow, &windowWidth, &windowHeight);

                playerCamera = new Camera(windowWidth, windowHeight, position);

                makeCameraCurrent(playerCamera);

                Cameras.push_back(playerCamera);
            }

            player(Camera* camera) {
                position = glm::vec3(0.0f, 0.0f, 100.0f);
                velocity = glm::vec3(0.0f);
                mass = 150.0f;

                int windowWidth, windowHeight;
                glfwGetFramebufferSize(mainWindow, &windowWidth, &windowHeight);

                playerCamera = camera;

                makeCameraCurrent(playerCamera);

                if (std::find(Cameras.begin(), Cameras.end(), playerCamera) == Cameras.end()) {
                    Cameras.push_back(playerCamera);
                }
            }

            // expects real-world coordinates
            void handleBlockBreak(glm::ivec3 blockPos) {
                if (blockPos.z >= CHUNK_HEIGHT) { return; }

                glm::ivec2 chunkPos = getChunkPos(blockPos);
                if (!chunkRegistry::exists(chunkPos)) { return; }

                glm::ivec3 localBlockPos = { blockPos.x - chunkPos.x * CHUNK_WIDTH, blockPos.y - chunkPos.y * CHUNK_WIDTH, blockPos.z };

                Chunk* chunk = chunkRegistry::getChunk(chunkPos);
                Block& block = chunk->getBlock(localBlockPos);

                if (block.acessPointer) {
                    std::array<short, 3> posToSearch = {(short)localBlockPos.x, (short)localBlockPos.y, (short)localBlockPos.z}; 
                    auto iterator = std::find_if(
                        chunk->visibleBlockData.begin(),
                        chunk->visibleBlockData.end(),
                        [&](const visibleBlock& entry){ return entry.localPosition == posToSearch; }
                    );

                    if (iterator != chunk->visibleBlockData.end()) {
                        chunk->visibleBlockData.erase(iterator);
                        block.acessPointer = nullptr;
                    }
                }

                block.ID = 0u;
                block.type = BlockType::air;
                chunk->updateBlockIntermediateData(localBlockPos.x, localBlockPos.y, localBlockPos.z);
                
                if (localBlockPos.x == 0)               chunkWorker::assignWork(chunkPos + glm::ivec2(-1, 0), chunkWorker::workType::updateChunk);
                if (localBlockPos.x == CHUNK_WIDTH - 1) chunkWorker::assignWork(chunkPos + glm::ivec2( 1, 0), chunkWorker::workType::updateChunk);
                if (localBlockPos.y == 0)               chunkWorker::assignWork(chunkPos + glm::ivec2(0, -1), chunkWorker::workType::updateChunk);
                if (localBlockPos.y == CHUNK_WIDTH - 1) chunkWorker::assignWork(chunkPos + glm::ivec2(0,  1), chunkWorker::workType::updateChunk);

                chunkWorker::assignWork(chunkPos, chunkWorker::workType::updateChunk);
            }

            void main() {
                printf("player pos: %.0f %.0f %.0f\n", position.x, position.y, position.z);
                playerCamera->handleInputs(mainWindow);

                bool onGround = isOnGround();
                bool sprint = glfwGetKey(mainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

                // --- Vertical (gravity + drag) ---
                if (!onGround) {
                    velocity.z -= physics::GravityAcceleration * deltaTime;
                    velocity.z *= 0.98f;
                } else {
                    if (velocity.z < 0.0f) velocity.z = 0.0f;
                }

                // --- Horizontal input ---
                glm::vec3 forward = glm::normalize(glm::vec3(playerCamera->orientation.x, playerCamera->orientation.y, 0.0f));
                glm::vec3 right   = glm::normalize(glm::cross(forward, UP));

                glm::vec3 inputDir = glm::vec3(0.0f);
                if (glfwGetKey(mainWindow, GLFW_KEY_W) == GLFW_PRESS) inputDir += forward;
                if (glfwGetKey(mainWindow, GLFW_KEY_S) == GLFW_PRESS) inputDir -= forward;
                if (glfwGetKey(mainWindow, GLFW_KEY_A) == GLFW_PRESS) inputDir -= right;
                if (glfwGetKey(mainWindow, GLFW_KEY_D) == GLFW_PRESS) inputDir += right;

                // ToDo: extend this into a survival MC mechanic
                static bool leftWasHeld = false;
                bool leftPressed = glfwGetMouseButton(mainWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;
                if (leftPressed && !leftWasHeld && playerCamera->cameraControlled) {
                    auto block = getBlockInSight(playerCamera->position, playerCamera->orientation, blockReach);
                    if (block.has_value()) { handleBlockBreak(block.value()); }
                }
                leftWasHeld = leftPressed;

                bool hasInput = glm::length(inputDir) > 0.0f;
                if (hasInput)
                    inputDir = glm::normalize(inputDir);

                // Air control penalty
                float inputScale = onGround ? (sprint ? sprintModifier : 1.0f) : 0.02f;
                velocity.x += inputDir.x * baseSpeed * inputScale * (float)deltaTime;
                velocity.y += inputDir.y * baseSpeed * inputScale * (float)deltaTime;

                // --- Jump (before friction, so landing + jump doesn't kill momentum) ---
                bool jumped = false;
                if (glfwGetKey(mainWindow, GLFW_KEY_SPACE) == GLFW_PRESS && onGround) {
                    velocity.z += jumpBurstSpeed;
                    jumped = true;
                }

                // bunny hopping enabler
                if (jumped && glfwGetKey(mainWindow, GLFW_KEY_W) == GLFW_PRESS) { velocity += forward * baseSpeed * 3.65f * (float)deltaTime; }

                // --- Horizontal friction/drag ---
                float drag;
                if (onGround && !jumped) {
                    drag = hasInput ? 0.9f : 0.2f;
                } else if (!onGround) {
                    drag = 0.98f;
                } else {
                    drag = 1.0f; // Just jumped — preserve momentum entirely
                }
                velocity.x *= drag;
                velocity.y *= drag;

                // --- Clamp horizontal and vertical independently ---
                glm::vec2 hVel(velocity.x, velocity.y);
                float hSpeed = glm::length(hVel);
                float maxH = onGround ? physics::maxLandVelocity : physics::maxAirVelocity;
                if (hSpeed > maxH) {
                    hVel = glm::normalize(hVel) * maxH;
                    velocity.x = hVel.x;
                    velocity.y = hVel.y;
                }

                // --- Simulate & sync camera ---
                this->simulate();

                playerCamera->position = position + glm::vec3(0.0f, 0.0f, coliderDimensions.y * 0.35f);

                if (currentCamera == playerCamera) {
                    for (const auto& shader : Shaders) { playerCamera->updateProjection(shader.second); }
                }
            }
    };
}

#endif // PLAYER_HEADER