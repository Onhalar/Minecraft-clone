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
#include <render.hpp>
#include <physics.hpp>
#include <camera.hpp>

#include <blockRayCast.hpp>

#include <algorithm>
#include <unordered_map>
#include <chrono>

namespace world {
    class player : public physics::physicsObject {
        private:
            const glm::vec3 UP = {0.0f, 0.0f, 1.0f};

            struct ablitiesStruct {
                bool walking = true;
                bool flying  = true;
            };

            ablitiesStruct abilities = ablitiesStruct();

            // Persistent per-frame state shared across helper methods
            bool onGround = false;
            bool sprint   = false;
            bool flight   = false;

            // ---------------------------------------------------------------
            // syncCamera — push simulated position into the camera and refresh
            //              projection matrices for all shaders.
            // ---------------------------------------------------------------
            void syncCamera() {
                playerCamera->position = position + glm::vec3(0.0f, 0.0f, coliderDimensions.y * 0.35f);
                if (currentCamera == playerCamera) {
                    for (const auto& shader : Shaders) { playerCamera->updateProjection(shader.second); }
                }
            }

            // ---------------------------------------------------------------
            // getInputDirections — returns the normalised horizontal wish-dir
            //                      and the raw vertical flight scalar.
            // ---------------------------------------------------------------
            struct InputVectors {
                glm::vec3 horizontal;   // normalised WASD direction (flat)
                float     vertical;     // +1 up / -1 down (flight only)
                bool      hasHorizontal;
            };

            InputVectors getInputDirections() {
                glm::vec3 forward = glm::normalize(glm::vec3(playerCamera->orientation.x, playerCamera->orientation.y, 0.0f));
                glm::vec3 right   = glm::normalize(glm::cross(forward, UP));

                glm::vec3 dir = glm::vec3(0.0f);
                if (abilities.walking || abilities.flying) {
                    if (glfwGetKey(mainWindow, GLFW_KEY_W) == GLFW_PRESS) dir += forward;
                    if (glfwGetKey(mainWindow, GLFW_KEY_S) == GLFW_PRESS) dir -= forward;
                    if (glfwGetKey(mainWindow, GLFW_KEY_A) == GLFW_PRESS) dir -= right;
                    if (glfwGetKey(mainWindow, GLFW_KEY_D) == GLFW_PRESS) dir += right;
                }

                bool hasHorizontal = glm::length(dir) > 0.0f;
                if (hasHorizontal) dir = glm::normalize(dir);

                float vertical = 0.0f;
                if (flight) {
                    if (glfwGetKey(mainWindow, GLFW_KEY_SPACE)        == GLFW_PRESS) vertical += 1.0f;
                    if (glfwGetKey(mainWindow, GLFW_KEY_LEFT_CONTROL) == GLFW_PRESS) vertical -= 1.0f;
                }

                // enabling and disabling flight
                static bool wasSpacePressed = false;
                bool isSpacePressed = glfwGetKey(mainWindow, GLFW_KEY_SPACE) == GLFW_PRESS;
                
                if (isSpacePressed && !wasSpacePressed && abilities.flying) {
                    if (isDoublePressed(GLFW_KEY_SPACE)) { flight = !flight; }
                }
                wasSpacePressed = isSpacePressed;

                return { dir, vertical, hasHorizontal };
            }

            // ---------------------------------------------------------------
            // handleBlockInteraction — single-click left-mouse block break.
            //   wasCameraFocused: camera focus state captured before
            //   handleInputs() so a click that *gains* focus doesn't also break.
            // ---------------------------------------------------------------
            void handleBlockInteraction(bool wasCameraFocused) {
                // ToDo: extend this into a survival MC mechanic
                static bool leftWasHeld = false;
                bool leftPressed = glfwGetMouseButton(mainWindow, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS;

                if (leftPressed && !leftWasHeld && playerCamera->cameraControlled && wasCameraFocused) {
                    auto block = getBlockInSight(playerCamera->position, playerCamera->orientation, blockReach);
                    if (block.has_value()) { handleBlockBreak(block.value()); }
                }

                leftWasHeld = leftPressed;
            }

            // ---------------------------------------------------------------
            // applyGravity — vertical velocity integration when walking.
            //   Must be called before movement so gravity is baked in before
            //   the collision sim runs.
            // ---------------------------------------------------------------
            void applyGravity() {
                if (!onGround) {
                    velocity.z -= physics::GravityAcceleration * deltaTime;
                    velocity.z *= 0.98f;
                } else {
                    // On ground: kill any residual downward velocity
                    if (velocity.z < 0.0f) velocity.z = 0.0f;
                }
            }

            // ---------------------------------------------------------------
            // handleFlight — accelerate, drag, and clamp while airborne in
            //                flight mode, then simulate + sync camera.
            // Returns true so the caller can early-out after this.
            // ---------------------------------------------------------------
            void handleFlight(const InputVectors& input) {
                float scale = sprint ? flightSprintModifier : 1.0f;

                velocity.x += input.horizontal.x * flightSpeed * scale * (float)deltaTime;
                velocity.y += input.horizontal.y * flightSpeed * scale * (float)deltaTime;
                velocity.z += input.vertical      * flightSpeed * scale * (float)deltaTime;

                // Uniform drag on all three axes — feels like floating through air
                velocity *= flightDrag;

                // Clamp to a sane max (vertical gets the same cap as horizontal)
                float speed     = glm::length(velocity);
                float maxFlight = physics::maxAirVelocity * scale;
                if (speed > maxFlight)
                    velocity = glm::normalize(velocity) * maxFlight;

                this->simulate();
                syncCamera();
            }

            // ---------------------------------------------------------------
            // handleWalking — ground/air acceleration, jumping, bunny-hop,
            //                 friction, horizontal speed cap, then simulate
            //                 + sync camera.
            // ---------------------------------------------------------------
            void handleWalking(const InputVectors& input) {
                glm::vec3 forward = glm::normalize(glm::vec3(playerCamera->orientation.x, playerCamera->orientation.y, 0.0f));

                // Horizontal acceleration (tiny air-control penalty when airborne)
                float inputScale = onGround ? (sprint ? sprintModifier : 1.0f) : 0.02f;
                velocity.x += input.horizontal.x * baseSpeed * inputScale * (float)deltaTime;
                velocity.y += input.horizontal.y * baseSpeed * inputScale * (float)deltaTime;

                // Jump — applied before friction so the burst isn't immediately damped
                bool jumped = false;
                if (glfwGetKey(mainWindow, GLFW_KEY_SPACE) == GLFW_PRESS && onGround && abilities.walking) {
                    velocity.z += jumpBurstSpeed;
                    jumped = true;
                }

                // Bunny-hop: extra forward kick when jumping while holding W
                if (jumped && glfwGetKey(mainWindow, GLFW_KEY_W) == GLFW_PRESS && abilities.walking) {
                    velocity += forward * baseSpeed * 3.65f * (float)deltaTime;
                }

                // Horizontal friction / drag
                float drag;
                if (onGround && !jumped) {
                    drag = input.hasHorizontal ? 0.9f : 0.2f;
                } else if (!onGround) {
                    drag = 0.98f;
                } else {
                    drag = 1.0f; // Just jumped — preserve full momentum
                }
                velocity.x *= drag;
                velocity.y *= drag;

                // Clamp horizontal speed independently of vertical
                glm::vec2 hVel(velocity.x, velocity.y);
                float hSpeed = glm::length(hVel);
                float maxH   = onGround ? physics::maxLandVelocity : physics::maxAirVelocity;
                if (hSpeed > maxH) {
                    hVel       = glm::normalize(hVel) * maxH;
                    velocity.x = hVel.x;
                    velocity.y = hVel.y;
                }

                this->simulate();
                syncCamera();
            }

            // ---------------------------------------------------------------
            // handleBlockBreak — expects real-world coordinates.
            // ---------------------------------------------------------------
            void handleBlockBreak(glm::ivec3 blockPos) {
                if (blockPos.z >= CHUNK_HEIGHT) { return; }

                glm::ivec2 chunkPos = getChunkPos(blockPos);
                if (!chunkRegistry::exists(chunkPos)) { return; }

                glm::ivec3 localBlockPos = {
                    blockPos.x - chunkPos.x * CHUNK_WIDTH,
                    blockPos.y - chunkPos.y * CHUNK_WIDTH,
                    blockPos.z
                };

                Chunk* chunk  = chunkRegistry::getChunk(chunkPos);
                Block& block  = chunk->getBlock(localBlockPos);

                if (block.acessPointer) {
                    std::array<short, 3> posToSearch = {
                        (short)localBlockPos.x, (short)localBlockPos.y, (short)localBlockPos.z
                    };
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

                block.ID   = 0u;
                block.type = BlockType::air;
                chunk->bakeChunk();
                chunk->updateBlockIntermediateData(localBlockPos.x, localBlockPos.y, localBlockPos.z);

                if (localBlockPos.x == 0)               chunkWorker::assignWork(chunkPos + glm::ivec2(-1,  0), chunkWorker::workType::remeshChunk);
                if (localBlockPos.x == CHUNK_WIDTH - 1) chunkWorker::assignWork(chunkPos + glm::ivec2( 1,  0), chunkWorker::workType::remeshChunk);
                if (localBlockPos.y == 0)               chunkWorker::assignWork(chunkPos + glm::ivec2( 0, -1), chunkWorker::workType::remeshChunk);
                if (localBlockPos.y == CHUNK_WIDTH - 1) chunkWorker::assignWork(chunkPos + glm::ivec2( 0,  1), chunkWorker::workType::remeshChunk);

                chunkWorker::assignWork(chunkPos, chunkWorker::workType::remeshChunk);
            }

            bool isDoublePressed(unsigned int glfwKey) {
                static const std::chrono::milliseconds doublePressWait(250);
                static std::unordered_map<unsigned int, std::chrono::steady_clock::time_point> registry = {};

                auto iter = registry.find(glfwKey);
                if (iter != registry.end()) {
                    auto registerTime = registry[glfwKey];

                    registry.erase(iter);
                    if (std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - registerTime) <= doublePressWait) { return true; }
                    else { return false; }
                }
                else {
                    registry[glfwKey] = std::chrono::steady_clock::now();
                    return false;
                }
            }

        public:
            Camera* playerCamera;

            float baseSpeed      = 35.0f;  // Faster acceleration to reach max speed
            float jumpBurstSpeed = 8.0f;   // Jump gives ~1.25 blocks height
            float sprintModifier = 1.65f;
            float friction       = 9.0f;   // Strong friction to stop sliding

            // Flight-specific tuning
            float flightSpeed          = 50.0f;  // Higher than walk so flight feels free
            float flightDrag           = 0.95f;  // Applied to all three axes while flying
            float flightSprintModifier = 2.5f;

            float blockReach = 7.5f;

            // ---------------------------------------------------------------
            // main — per-frame update entry point.
            // ---------------------------------------------------------------
            void main() {
                bool wasCameraFocused = playerCamera->cameraControlled;
                playerCamera->handleInputs(mainWindow);

                // Resolve per-frame state
                onGround = isOnGround();
                sprint   = glfwGetKey(mainWindow, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS;

                if (!abilities.flying || (onGround && abilities.walking)) { flight = false; }

                handleBlockInteraction(wasCameraFocused);

                InputVectors input = getInputDirections();

                if (flight) {
                    handleFlight(input);
                } else {
                    applyGravity();
                    handleWalking(input);
                }
            }
            
            player() {
                position = glm::vec3(0.5f, 0.5f, 100.0f);
                velocity = glm::vec3(0.0f);
                mass     = 150.0f;

                int windowWidth, windowHeight;
                glfwGetFramebufferSize(mainWindow, &windowWidth, &windowHeight);

                playerCamera = new Camera(windowWidth, windowHeight, position);
                makeCameraCurrent(playerCamera);
                Cameras.push_back(playerCamera);
            }

            player(Camera* camera) {
                position = glm::vec3(0.0f, 0.0f, 100.0f);
                velocity = glm::vec3(0.0f);
                mass     = 150.0f;

                int windowWidth, windowHeight;
                glfwGetFramebufferSize(mainWindow, &windowWidth, &windowHeight);

                playerCamera = camera;
                makeCameraCurrent(playerCamera);

                if (std::find(Cameras.begin(), Cameras.end(), playerCamera) == Cameras.end()) {
                    Cameras.push_back(playerCamera);
                }
            }
    };
}

#endif // PLAYER_HEADER