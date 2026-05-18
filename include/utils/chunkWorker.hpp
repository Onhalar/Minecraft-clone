#ifndef CHUNK_WORKER_HEADER
#define CHUNK_WORKER_HEADER

#include "config.hpp"
#include "glm/fwd.hpp"
#include <atomic>
#include <globals.hpp>
#include <chunkGeneration.hpp>
#include <chunk.hpp>

#include <mutex>
#include <deque>
#include <vector>
#include <algorithm>
#include <condition_variable>
#include <thread>
#include <optional>

namespace world {
    class chunkWorker {
        public:
            enum workType: unsigned char {
                addChunk,
                removeChunk,
                updateChunk,
                remeshChunk
            };
            
            static inline std::mutex workerMutex;
            static inline std::condition_variable cv;

            static inline std::deque<glm::ivec2> addQueue = {};
            static inline std::deque<glm::ivec2> removeQueue = {};
            static inline std::deque<glm::ivec2> updateQueue = {};
            static inline std::deque<glm::ivec2> remeshQueue = {};

            static inline std::deque<world::Chunk*> readyToUpload = {};
            static inline glm::fvec3 currentPlayerPosition;

            static inline std::vector<std::thread> workerThreads;
            static inline unsigned int threadCount = std::thread::hardware_concurrency();

            static inline std::atomic<bool> shouldTerminate = false;
            
            static bool assignWork(glm::ivec2 chunkPosition, workType type = workType::addChunk) {
                std::lock_guard<std::mutex> lock(workerMutex);

                std::deque<glm::ivec2>* queue;
                if (type == workType::addChunk) { queue = &addQueue; }
                else if (type == workType::removeChunk) { queue = &removeQueue; }
                else if (type == workType::updateChunk) { queue = &updateQueue; }
                else if (type == workType::remeshChunk) { queue = &remeshQueue; }

                if (std::find(queue->begin(), queue->end(), chunkPosition) != queue->end()) { return false; }

                queue->emplace_back(chunkPosition);
                cv.notify_one();
                return true;
            }

            static void clearAssignedWork() {
                std::lock_guard<std::mutex> lock(workerMutex);
                addQueue.clear();
                removeQueue.clear();
                cv.notify_all();
            }

            static void checkForNewChunks(glm::fvec3& playerPosition) {
                currentPlayerPosition = playerPosition;
                glm::ivec2 playerChunkPos = glm::floor(playerPosition / (float)CHUNK_WIDTH);

                std::vector<glm::ivec2> candidates;
                candidates.reserve((2 * renderDistance + 1) * (2 * renderDistance + 1));

                int rd = (int)renderDistance;
                for (int x = -rd; x <= rd; ++x) {
                    for (int y = -rd; y <= rd; ++y) {
                        if (x * x + y * y > rd * rd) { continue; }

                        glm::ivec2 chunkPos = playerChunkPos + glm::ivec2(x, y);
                        if (!world::chunkRegistry::exists(chunkPos)) {
                            candidates.push_back(chunkPos);
                        }
                    }
                }

                std::sort(candidates.begin(), candidates.end(),
                    [&playerChunkPos](const glm::ivec2& a, const glm::ivec2& b) {
                        int dax = a.x - playerChunkPos.x, day = a.y - playerChunkPos.y;
                        int dbx = b.x - playerChunkPos.x, dby = b.y - playerChunkPos.y;
                        return (dax * dax + day * day) < (dbx * dbx + dby * dby);
                    });

                for (const auto& chunkPos : candidates) {
                    assignWork(chunkPos, workType::addChunk);
                }
            }

            static void checkForFarChunks(glm::fvec3& playerPosition) {
                currentPlayerPosition = playerPosition;
                glm::ivec2 playerChunkPos = glm::floor(playerPosition / (float)CHUNK_WIDTH);

                std::vector<glm::ivec2> toRemove;
                {
                    std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
                    for (const auto& [chunkPos, chunk] : chunkRegistry::registry) {
                        int dx = chunkPos.x - playerChunkPos.x;
                        int dy = chunkPos.y - playerChunkPos.y;
                        if (dx * dx + dy * dy > renderDistance * renderDistance) {
                            toRemove.push_back(chunkPos);
                        }
                    }
                }

                for (const auto& chunkPos : toRemove) {
                    assignWork(chunkPos, workType::removeChunk);
                }
            }

            // Fixed Stutter: Budget the maximum uploads performed per frame to prevent freezing T0
            static void uploadQueuedMeshes() {
                unsigned int uploadsDone = 0;
                while (uploadsDone < maxUploadsPerFrame) {
                    world::Chunk* chunk = nullptr;
                    bool claimed = false;

                    {
                        std::lock_guard<std::mutex> lock(workerMutex);
                        if (readyToUpload.empty()) { break; }
                        chunk = readyToUpload.front();
                        readyToUpload.pop_front();

                        if (chunk) {
                            chunk->activeWorkers.fetch_add(1, std::memory_order_relaxed);
                            if (chunk->isBeingDeleted.load(std::memory_order_acquire)) {
                                chunk->activeWorkers.fetch_sub(1, std::memory_order_release);
                            } else {
                                claimed = true;
                            }
                        }
                    }

                    if (!claimed) { continue; }

                    chunk->uploadMesh();

                    // Manually decrement since we are bypassing the RAII wrapper here
                    chunk->activeWorkers.fetch_sub(1, std::memory_order_release);
                    ++uploadsDone;
                }
            }

            static void processNeighbour(glm::ivec2 neighbourPos, ChunkSides side) {
                std::optional<ChunkWorkerGuard> guard;
                {
                    std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
                    auto it = chunkRegistry::registry.find(neighbourPos);
                    if (it != chunkRegistry::registry.end() && !it->second->isBeingDeleted.load(std::memory_order_acquire)) {
                        guard.emplace(it->second);
                    }
                }

                if (!guard || !guard->valid()) { return; } 
                Chunk* neighbour = guard->chunk;

                neighbour->regenerateSideIntermideateData(side);
                neighbour->stitchMesh();

                if (neighbour->mesh && !neighbour->mesh->empty()) {
                    std::lock_guard<std::mutex> lock(workerMutex);
                    if (std::find(readyToUpload.begin(), readyToUpload.end(), neighbour) == readyToUpload.end()) {
                        readyToUpload.push_back(neighbour);
                    }
                }
            }

            static void workerThreadFunc() {
                while (!shouldTerminate) {
                    glm::ivec2 chunkPos;
                    workType type;

                    {
                        std::unique_lock<std::mutex> lock(workerMutex);
                        cv.wait(lock, [] { return shouldTerminate || !removeQueue.empty() || !addQueue.empty() || !updateQueue.empty() || !remeshQueue.empty(); });
                        if (shouldTerminate) { break; }

                        if (!removeQueue.empty()) {
                            chunkPos = removeQueue.front(); removeQueue.pop_front();
                            type = workType::removeChunk;
                        }
                        else if (!remeshQueue.empty()) {
                            chunkPos = remeshQueue.front(); remeshQueue.pop_front();
                            type = workType::remeshChunk;
                        }
                        else if (!updateQueue.empty()) {
                            chunkPos = updateQueue.front(); updateQueue.pop_front();
                            type = workType::updateChunk;
                        }
                        else if (!addQueue.empty()) {
                            chunkPos = addQueue.front(); addQueue.pop_front();
                            type = workType::addChunk;
                        }
                    }

                    if (type == workType::addChunk) {
                        glm::ivec2 currentPlayerChunkPos = glm::floor(currentPlayerPosition / (float)CHUNK_WIDTH);
                        int dx = chunkPos.x - currentPlayerChunkPos.x;
                        int dy = chunkPos.y - currentPlayerChunkPos.y;

                        if (dx * dx + dy * dy > renderDistance * renderDistance) { continue; }
                            
                        Chunk* chunk = worldGenerator->generate(chunkPos, true);

                        if (chunk) {
                            processNeighbour({chunkPos.x, chunkPos.y - 1}, ChunkSides::front);
                            processNeighbour({chunkPos.x, chunkPos.y + 1}, ChunkSides::back);
                            processNeighbour({chunkPos.x + 1, chunkPos.y}, ChunkSides::left);
                            processNeighbour({chunkPos.x - 1, chunkPos.y}, ChunkSides::right);

                            ChunkWorkerGuard guard(chunk);
                            if (guard.valid()) {
                                chunk->generateIntermediateData();
                                chunk->stitchMesh();

                                if (chunk->mesh && !chunk->mesh->empty()) {
                                    std::lock_guard<std::mutex> lock(workerMutex);
                                    readyToUpload.push_back(chunk);
                                }
                            }
                        }
                    }

                    else if (type == workType::updateChunk || type == workType::remeshChunk) {
                        std::optional<ChunkWorkerGuard> guard;
                        {
                            std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
                            auto it = chunkRegistry::registry.find(chunkPos);
                            if (it != chunkRegistry::registry.end() && !it->second->isBeingDeleted.load(std::memory_order_acquire)) {
                                guard.emplace(it->second);
                            }
                        }

                        if (!guard || !guard->valid()) { continue; }
                        Chunk* chunk = guard->chunk;

                        if (type == workType::updateChunk) { chunk->generateIntermediateData(); }
                        chunk->stitchMesh();

                        if (chunk->mesh && !chunk->mesh->empty()) {
                            std::lock_guard<std::mutex> lock(workerMutex);
                            readyToUpload.push_back(chunk);
                        }
                    }

                    else if (type == workType::removeChunk) {
                        {
                            std::lock_guard<std::mutex> wlock(workerMutex);
                            Chunk* chunkToRemove = chunkRegistry::getChunk(chunkPos);
                            if (!chunkToRemove) { continue; }

                            readyToUpload.erase(
                                std::remove(readyToUpload.begin(), readyToUpload.end(), chunkToRemove),
                                readyToUpload.end());
                        }

                        world::chunkRegistry::deregisterChunk(chunkPos, true);
                    }
                }
            }
    };
}

#endif // CHUNK_WORKER_HEADER