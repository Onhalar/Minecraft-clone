#ifndef CHUNK_WORK_ASSIGNER_HEADER
#define CHUNK_WORK_ASSIGNER_HEADER

#include <globals.hpp>
#include <chunkGeneration.hpp>
#include <chunk.hpp>

#include <mutex>
#include <deque>
#include <algorithm>
#include <condition_variable>

#include <thread>

namespace world {
    class chunkWorker {
        private:
            enum workType: bool {
                addChunk,
                removeChunk
            };

            struct task {
                workType type;
                glm::ivec2 chunkPosition;

                task(workType type, glm::ivec2 chunkPosition): type(type), chunkPosition(chunkPosition) {}
            };
            
        public:
            static inline std::mutex workerMutex;
            static inline std::condition_variable cv;
            static inline std::deque<task> assignedWork = {};
            static inline std::deque<world::Chunk*> readyToUpload = {};  // Chunks ready for GPU upload
            static inline bool shouldTerminate = false;

            static inline std::thread workerThread;

                static bool assignWork(glm::ivec2 chunkPosition, workType type = workType::addChunk) {
                    std::lock_guard<std::mutex> lock(workerMutex);

                    if (std::find_if(
                            assignedWork.begin(),
                            assignedWork.end(),
                            [&chunkPosition](const task& t) { return t.chunkPosition == chunkPosition; }
                        ) != assignedWork.end()) {
                        return false;
                    }

                    assignedWork.emplace_back(type, chunkPosition);
                    cv.notify_one();
                    return true;
                }

                static void clearAssignedWork() {
                    std::lock_guard<std::mutex> lock(workerMutex);
                    assignedWork.clear();
                }

                static void checkForNewChunks(glm::fvec3& playerPosition) {
                    glm::ivec2 playerChunkPos = glm::floor(playerPosition / (float)CHUNK_WIDTH);
                    float radiusSq = (float)renderDistance * (float)renderDistance;

                    // Start from center and spiral outward
                    for (int layer = 0; layer <= renderDistance; ++layer) {
                        if (layer == 0) {
                            // Center chunk
                            glm::ivec2 chunkPos = playerChunkPos;
                            if (!world::chunkRegistry::exists(chunkPos)) { assignWork(chunkPos); }
                        } 
                        else {
                            // Spiral around current layer
                            for (int x = -layer; x <= layer; ++x) {
                                for (int y = -layer; y <= layer; ++y) {
                                    // Only process chunks on the outer edge of this layer
                                    if (std::abs(x) != layer && std::abs(y) != layer) { continue; }
                                    
                                    glm::ivec2 chunkPos = playerChunkPos + glm::ivec2(x, y);
                                    
                                    // Optional: check circular distance
                                    float distSq = (float)(x * x + y * y);
                                    if (distSq > radiusSq) { continue; }
                                    
                                    if (!world::chunkRegistry::exists(chunkPos)) { assignWork(chunkPos, workType::addChunk); }
                                }
                            }
                        }
                    }
                }

                static void checkForFarChunks(glm::fvec3& playerPosition) {
                    glm::ivec2 playerChunkPos = glm::floor(playerPosition / (float)CHUNK_WIDTH);
                    float radiusSq = (float)(renderDistance * renderDistance);

                    std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
                    for (const auto& [chunkPos, chunk] : chunkRegistry::registry) {
                        float dx = (float)(chunkPos.x - playerChunkPos.x);
                        float dy = (float)(chunkPos.y - playerChunkPos.y);
                        float distSq = dx * dx + dy * dy;

                        if (distSq > radiusSq) {
                            assignWork(chunkPos, workType::removeChunk);
                        }
                    }
                }

                // RUN ONLY FROM OPENGL CONTEXT THREAD
                static void uploadQueuedMeshes() {
                    std::lock_guard<std::mutex> lock(workerMutex);
                    while (!readyToUpload.empty()) {
                        world::Chunk* chunk = readyToUpload.front();
                        readyToUpload.pop_front();
                        chunk->uploadMesh();  // GPU upload on main thread
                    }
                }

                static void workerThreadFunc() {
                    while (!shouldTerminate) {
                        glm::ivec2 chunkPos;
                        workType type;

                        {
                            std::unique_lock<std::mutex> lock(workerMutex);
                            cv.wait(lock, [] { return !assignedWork.empty(); });  // Wait for work

                            auto currentTask = assignedWork.front();
                            chunkPos = currentTask.chunkPosition;
                            type = currentTask.type;

                            assignedWork.pop_front();
                        }

                        if (type == workType::addChunk) {
                            Chunk* chunk = worldGenerator->generate(chunkPos, false);  // Generate without registering

                            if (chunk) {
                                chunk->stitchMesh();  // Prepare mesh data

                                std::lock_guard<std::mutex> lock(workerMutex);
                                readyToUpload.push_back(chunk);  // Queue for main thread upload
                            }

                        }
                        else if (type == workType::removeChunk) { world::chunkRegistry::deregisterChunk(chunkPos); }
                    }
                }
    };
}

#endif // CHUNK_WORK_ASSIGNER_HEADER