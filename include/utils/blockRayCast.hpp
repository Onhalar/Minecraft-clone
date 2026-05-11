#ifndef BLOCK_RAYCAST_HEADER
#define BLOCK_RAYCAST_HEADER

#include <chunk.hpp>
#include <block.hpp>

#include <glm/glm.hpp>

#include <optional>
#include <mutex>

namespace world {

    // returns world coordinates of a block being loooked at
    inline std::optional<glm::ivec3> getBlockInSight(
        const glm::vec3& viewPos,
        const glm::vec3& orientation,
        const float& maxDistance)
    {
        // Normalize direction; bail if degenerate
        float len = glm::length(orientation);
        if (len < 1e-6f) { return std::nullopt; }
        glm::vec3 dir = orientation / len;

        // Current voxel coordinates (floor to handle negative coords correctly)
        int vx = (int)std::floor(viewPos.x);
        int vy = (int)std::floor(viewPos.y);
        int vz = (int)std::floor(viewPos.z);

        // Step direction per axis
        int stepX = (dir.x >= 0.f) ? 1 : -1;
        int stepY = (dir.y >= 0.f) ? 1 : -1;
        int stepZ = (dir.z >= 0.f) ? 1 : -1;

        // How far along the ray we must travel to cross one full voxel on each axis
        float tDeltaX = (dir.x != 0.f) ? std::abs(1.f / dir.x) : 1e30f;
        float tDeltaY = (dir.y != 0.f) ? std::abs(1.f / dir.y) : 1e30f;
        float tDeltaZ = (dir.z != 0.f) ? std::abs(1.f / dir.z) : 1e30f;

        // Distance to the first voxel boundary on each axis
        auto distToNext = [](float pos, float step) -> float {
            return (step > 0.f) ? (std::floor(pos + 1.f) - pos) : (pos - std::floor(pos));
        };
        float tMaxX = distToNext(viewPos.x, (float)stepX) * tDeltaX;
        float tMaxY = distToNext(viewPos.y, (float)stepY) * tDeltaY;
        float tMaxZ = distToNext(viewPos.z, (float)stepZ) * tDeltaZ;

        float tCurrent = 0.f;

        while (tCurrent < maxDistance) {
            // Bounds check: z must be within [0, CHUNK_HEIGHT)
            if (vz >= 0 && vz < CHUNK_HEIGHT) {
                // Resolve world (vx, vy) to chunk + local coords
                // Use floor division so negatives work correctly
                int cx = (int)std::floor((float)vx / CHUNK_WIDTH);
                int cy = (int)std::floor((float)vy / CHUNK_WIDTH);
                glm::ivec2 chunkPos(cx, cy);

                int localX = vx - cx * CHUNK_WIDTH;
                int localY = vy - cy * CHUNK_WIDTH;

                std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
                auto it = chunkRegistry::registry.find(chunkPos);
                if (it != chunkRegistry::registry.end() && it->second) {
                    Block& b = it->second->getBlock(
                        (unsigned char)localX,
                        (unsigned char)localY,
                        (unsigned short)vz);

                    if (b.type == BlockType::solid) {
                        return (std::optional<glm::ivec3>) glm::ivec3{
                            localX + chunkPos.x * CHUNK_WIDTH,
                            localY + chunkPos.y * CHUNK_WIDTH,
                            vz
                        }; // found it
                    }
                }
            }

            // DDA: advance to the nearest next voxel boundary
            if (tMaxX < tMaxY && tMaxX < tMaxZ) {
                tCurrent = tMaxX;
                tMaxX   += tDeltaX;
                vx      += stepX;
            } else if (tMaxY < tMaxZ) {
                tCurrent = tMaxY;
                tMaxY   += tDeltaY;
                vy      += stepY;
            } else {
                tCurrent = tMaxZ;
                tMaxZ   += tDeltaZ;
                vz      += stepZ;
            }
        }

        return std::nullopt;
    }
}

#endif // BLOCK_RAYCAST_HEADER