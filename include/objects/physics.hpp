#ifndef PHYSICS_HEADER
#define PHYSICS_HEADER

#include "block.hpp"
#include <glm/glm.hpp>
#include <chunk.hpp>
#include <mutex>

#include <globals.hpp>

namespace physics {

    using kilogram = float;
    inline float GravityAcceleration = 9.81f * 2;   // Standard gravity

    inline float maxLandVelocity = 20.5f;      // Minecraft horizontal speed cap
    inline float maxAirVelocity = 78.4f;       // Terminal velocity in Minecraft
    
    // Small epsilon to prevent floating point errors
    constexpr float EPSILON = 0.001f;
    constexpr float COLLISION_MARGIN = 0.001f;

    class physicsObject {
    private: 
        inline glm::ivec2 getChunkPos() { 
            return glm::ivec2(
                static_cast<int>(std::floor(position.x / (float)CHUNK_WIDTH)),
                static_cast<int>(std::floor(position.y / (float)CHUNK_WIDTH))
            );
        }
        
        inline glm::vec3 getFeetPos() { 
            return position - glm::vec3(0.0f, 0.0f, coliderDimensions.y / 2.0f); 
        }
        
        // Get block position from world coordinates
        inline glm::ivec3 getBlockPos(const glm::vec3& worldPos) {
            return glm::ivec3(
                static_cast<int>(std::floor(worldPos.x)),
                static_cast<int>(std::floor(worldPos.y)),
                static_cast<int>(std::floor(worldPos.z))
            );
        }
        
        // Check if a specific block position is solid
        bool isBlockSolid(const glm::ivec3& blockPos) {
            glm::ivec2 chunkPos(
                static_cast<int>(std::floor(static_cast<float>(blockPos.x) / CHUNK_WIDTH)),
                static_cast<int>(std::floor(static_cast<float>(blockPos.y) / CHUNK_WIDTH))
            );
            
            if (blockPos.z < 0 || blockPos.z >= CHUNK_HEIGHT) {
                return blockPos.z < 0; // Solid below world, not above
            }
            
            int localX = (blockPos.x % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
            int localY = (blockPos.y % CHUNK_WIDTH + CHUNK_WIDTH) % CHUNK_WIDTH;
            
            std::lock_guard<std::mutex> lock(world::chunkRegistry::registryMutex);
            auto it = world::chunkRegistry::registry.find(chunkPos);
            if (it == world::chunkRegistry::registry.end() || !it->second) {
                return false;
            }
            
            return it->second->getBlock(localX, localY, blockPos.z).type != world::BlockType::air;
        }
        
        // Collision response using iterative position correction
        void resolveCollisions(glm::vec3& movement) {
            glm::vec3 halfExtents(
                coliderDimensions.x / 2.0f,
                coliderDimensions.x / 2.0f,
                coliderDimensions.y / 2.0f
            );
            
            // Move one axis at a time for proper collision response
            for (int axis = 0; axis < 3; axis++) {
                glm::vec3 axisMovement(0.0f);
                axisMovement[axis] = movement[axis];
                
                if (glm::abs(axisMovement[axis]) < EPSILON) continue;
                
                // Proposed position after moving on this axis
                glm::vec3 proposedPos = position + axisMovement;
                
                // Check collision on this axis
                glm::vec3 minPos = proposedPos - halfExtents;
                glm::vec3 maxPos = proposedPos + halfExtents;
                
                glm::ivec3 minBlock = getBlockPos(minPos);
                glm::ivec3 maxBlock = getBlockPos(maxPos);
                
                bool collided = false;
                
                for (int x = minBlock.x; x <= maxBlock.x && !collided; x++) {
                    for (int y = minBlock.y; y <= maxBlock.y && !collided; y++) {
                        for (int z = minBlock.z; z <= maxBlock.z && !collided; z++) {
                            if (isBlockSolid(glm::ivec3(x, y, z))) {
                                // Block bounds
                                glm::vec3 blockMin(x, y, z);
                                glm::vec3 blockMax(x + 1, y + 1, z + 1);
                                
                                // Check AABB overlap
                                if (minPos.x < blockMax.x && maxPos.x > blockMin.x &&
                                    minPos.y < blockMax.y && maxPos.y > blockMin.y &&
                                    minPos.z < blockMax.z && maxPos.z > blockMin.z) {
                                    
                                    collided = true;
                                    
                                    // Push out of collision
                                    if (axisMovement[axis] > 0) {
                                        // Moving positive
                                        proposedPos[axis] = blockMin[axis] - halfExtents[axis] - COLLISION_MARGIN;
                                        velocity[axis] = 0; // Stop velocity on this axis
                                    } else {
                                        // Moving negative
                                        proposedPos[axis] = blockMax[axis] + halfExtents[axis] + COLLISION_MARGIN;
                                        velocity[axis] = 0; // Stop velocity on this axis
                                    }
                                    movement[axis] = proposedPos[axis] - position[axis];
                                }
                            }
                        }
                    }
                }
                
                position[axis] = proposedPos[axis];
            }
        }

    public:
        glm::vec3 position; // at center of colider
        glm::vec3 velocity;

        kilogram mass;

        glm::vec2 coliderDimensions = {0.6f, 1.8f}; // width, height - rectangular prism

        bool isOnGround() {
            glm::vec3 feetPos = getFeetPos();
            float checkZ = feetPos.z - COLLISION_MARGIN * 2.0f; // reach past the margin resolveCollisions leaves
            float halfWidth = coliderDimensions.x / 2.0f - COLLISION_MARGIN;

            const glm::vec3 corners[] = {
                { feetPos.x - halfWidth, feetPos.y - halfWidth, checkZ },
                { feetPos.x + halfWidth, feetPos.y - halfWidth, checkZ },
                { feetPos.x - halfWidth, feetPos.y + halfWidth, checkZ },
                { feetPos.x + halfWidth, feetPos.y + halfWidth, checkZ },
            };

            for (const auto& corner : corners) {
                if (isBlockSolid(getBlockPos(corner))) return true;
            }
            return false;
        }

        void simulate() {
            if (deltaTime <= 0.0f) return;
            
            // Calculate movement from velocity
            glm::vec3 movement = velocity * (float)deltaTime;
            
            // Resolve collisions with the world
            resolveCollisions(movement);
            
            // Handle being pushed out of blocks we're already inside
            // (can happen from chunk loading, teleportation, etc.)
            pushOutOfBlocks();
        }
        
        void pushOutOfBlocks() {
            glm::vec3 halfExtents(
                coliderDimensions.x / 2.0f,
                coliderDimensions.x / 2.0f,
                coliderDimensions.y / 2.0f
            );
            
            glm::vec3 minPos = position - halfExtents;
            glm::vec3 maxPos = position + halfExtents;
            
            glm::ivec3 minBlock = getBlockPos(minPos);
            glm::ivec3 maxBlock = getBlockPos(maxPos);
            
            for (int x = minBlock.x; x <= maxBlock.x; x++) {
                for (int y = minBlock.y; y <= maxBlock.y; y++) {
                    for (int z = minBlock.z; z <= maxBlock.z; z++) {
                        if (isBlockSolid(glm::ivec3(x, y, z))) {
                            glm::vec3 blockMin(x, y, z);
                            glm::vec3 blockMax(x + 1, y + 1, z + 1);
                            
                            // Calculate overlap on each axis
                            float overlapX = 0.0f;
                            float overlapY = 0.0f;
                            float overlapZ = 0.0f;
                            
                            if (position.x < blockMin.x + halfExtents.x) {
                                overlapX = blockMin.x - maxPos.x;
                            } else if (position.x > blockMax.x - halfExtents.x) {
                                overlapX = blockMax.x - minPos.x;
                            }
                            
                            if (position.y < blockMin.y + halfExtents.y) {
                                overlapY = blockMin.y - maxPos.y;
                            } else if (position.y > blockMax.y - halfExtents.y) {
                                overlapY = blockMax.y - minPos.y;
                            }
                            
                            if (position.z < blockMin.z + halfExtents.z) {
                                overlapZ = blockMin.z - maxPos.z;
                            } else if (position.z > blockMax.z - halfExtents.z) {
                                overlapZ = blockMax.z - minPos.z;
                            }
                            
                            // Push out on axis with smallest overlap
                            if (std::abs(overlapX) < std::abs(overlapY) && std::abs(overlapX) < std::abs(overlapZ)) {
                                position.x += overlapX;
                            } else if (std::abs(overlapY) < std::abs(overlapZ)) {
                                position.y += overlapY;
                            } else {
                                position.z += overlapZ;
                            }
                        }
                    }
                }
            }
        }
    };
}

#endif // PHYSICS_HEADER