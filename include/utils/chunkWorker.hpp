#ifndef CHUNK_WORKER_HEADER
#define CHUNK_WORKER_HEADER

#include <globals.hpp>
#include <chunkGeneration.hpp>
#include <chunk.hpp>

#include <mutex>
#include <deque>
#include <vector>
#include <algorithm>
#include <condition_variable>
#include <cmath>

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
            static inline std::deque<task> addQueue = {};     // Lower priority: chunk generation
            static inline std::deque<task> removeQueue = {}; // Higher priority: chunk removal
            static inline std::deque<world::Chunk*> readyToUpload = {};
            static inline bool shouldTerminate = false;
            static inline glm::fvec3 currentPlayerPosition;


            static inline std::thread workerThread;

                static bool assignWork(glm::ivec2 chunkPosition, workType type = workType::addChunk) {
                    std::lock_guard<std::mutex> lock(workerMutex);

                    // Check the relevant queue for duplicates only — no cross-queue search needed
                    // since an add and remove for the same chunk are meaningfully different ops.
                    auto& queue = (type == workType::removeChunk) ? removeQueue : addQueue;

                    if (std::find_if(
                            queue.begin(),
                            queue.end(),
                            [&chunkPosition](const task& t) { return t.chunkPosition == chunkPosition; }
                        ) != queue.end()) {
                        return false;
                    }

                    queue.emplace_back(type, chunkPosition);
                    cv.notify_one();
                    return true;
                }

                static void clearAssignedWork() {
                    std::lock_guard<std::mutex> lock(workerMutex);
                    addQueue.clear();
                    removeQueue.clear();

                    cv.notify_all(); // Wake up worker thread to let it exit if it's waiting
                }

                static void checkForNewChunks(glm::fvec3& playerPosition) {
                    currentPlayerPosition = playerPosition;

                    glm::ivec2 playerChunkPos = glm::floor(playerPosition / (float)CHUNK_WIDTH);

                    // Collect all candidate positions within a circle, then sort
                    // closest-first so the worker generates nearest chunks first.
                    std::vector<glm::ivec2> candidates;
                    candidates.reserve((2 * renderDistance + 1) * (2 * renderDistance + 1));

                    for (int x = -renderDistance; x <= renderDistance; ++x) {
                        for (int y = -renderDistance; y <= renderDistance; ++y) {
                            // Use squared Euclidean distance for a circular boundary
                            if (x * x + y * y > renderDistance * renderDistance) { continue; }

                            glm::ivec2 chunkPos = playerChunkPos + glm::ivec2(x, y);
                            if (!world::chunkRegistry::exists(chunkPos)) {
                                candidates.push_back(chunkPos);
                            }
                        }
                    }

                    // Sort by squared distance so nearest chunks are queued first
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

                    std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
                    for (const auto& [chunkPos, chunk] : chunkRegistry::registry) {
                        int dx = chunkPos.x - playerChunkPos.x;
                        int dy = chunkPos.y - playerChunkPos.y;

                        // Euclidean (circular) remove boundary — matches the circular load boundary
                        if (dx * dx + dy * dy > renderDistance * renderDistance) {
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
                        chunk->uploadMesh();
                    }
                }

                static void workerThreadFunc() {
                    while (!shouldTerminate) {
                        glm::ivec2 chunkPos;
                        workType type;

                        {
                            std::unique_lock<std::mutex> lock(workerMutex);
                            // Wake up if either queue has work
                            cv.wait(lock, [] { return shouldTerminate || !removeQueue.empty() || !addQueue.empty(); });
                            if (shouldTerminate) { break; }

                            // Drain removes first — always prefer them to avoid buffer overflow
                            if (!removeQueue.empty()) {
                                auto currentTask = removeQueue.front();
                                removeQueue.pop_front();
                                chunkPos = currentTask.chunkPosition;
                                type = currentTask.type;
                            } else {
                                auto currentTask = addQueue.front();
                                addQueue.pop_front();
                                chunkPos = currentTask.chunkPosition;
                                type = currentTask.type;
                            }
                        }

                        if (type == workType::addChunk) {
                            glm::ivec2 currentPlayerChunkPos = glm::floor(currentPlayerPosition / (float)CHUNK_WIDTH);
                            int dx = chunkPos.x - currentPlayerChunkPos.x;
                            int dy = chunkPos.y - currentPlayerChunkPos.y;

                            // Skip stale add tasks — player may have moved away since it was queued.
                            // Use the same circular boundary as the load/remove checks.
                            if (dx * dx + dy * dy > renderDistance * renderDistance) { continue; }
                                
                            Chunk* chunk = worldGenerator->generate(chunkPos, true);

                            if (chunk) {
                                
                                if (chunkRegistry::exists({chunkPos.x, chunkPos.y - 1})) {

                                    chunkRegistry::registryMutex.lock();
                                    Chunk* neighbour = chunkRegistry::registry[{chunkPos.x, chunkPos.y - 1}];
                                    chunkRegistry::registryMutex.unlock();

                                    neighbour->regenerateSideIntermideateData(ChunkSides::front);
                                    neighbour->stitchMesh();

                                    if (neighbour->mesh && !neighbour->mesh->empty()) {
                                        std::lock_guard<std::mutex> lock(workerMutex);
                                        readyToUpload.push_back(neighbour);
                                    }
                                }
                                if (chunkRegistry::exists({chunkPos.x, chunkPos.y + 1})) {

                                    chunkRegistry::registryMutex.lock();
                                    Chunk* neighbour = chunkRegistry::registry[{chunkPos.x, chunkPos.y + 1}];
                                    chunkRegistry::registryMutex.unlock();

                                    neighbour->regenerateSideIntermideateData(ChunkSides::back);
                                    neighbour->stitchMesh();

                                    if (neighbour->mesh && !neighbour->mesh->empty()) {
                                        std::lock_guard<std::mutex> lock(workerMutex);
                                        readyToUpload.push_back(neighbour);
                                    }
                                }
                                if (chunkRegistry::exists({chunkPos.x + 1, chunkPos.y})) {

                                    chunkRegistry::registryMutex.lock();
                                    Chunk* neighbour = chunkRegistry::registry[{chunkPos.x + 1, chunkPos.y}];
                                    chunkRegistry::registryMutex.unlock();

                                    neighbour->regenerateSideIntermideateData(ChunkSides::left);
                                    neighbour->stitchMesh();

                                    if (neighbour->mesh && !neighbour->mesh->empty()) {
                                        std::lock_guard<std::mutex> lock(workerMutex);
                                        readyToUpload.push_back(neighbour);
                                    }
                                }
                                if (chunkRegistry::exists({chunkPos.x - 1, chunkPos.y})) {

                                    chunkRegistry::registryMutex.lock();
                                    Chunk* neighbour = chunkRegistry::registry[{chunkPos.x - 1, chunkPos.y}];
                                    chunkRegistry::registryMutex.unlock();

                                    neighbour->regenerateSideIntermideateData(ChunkSides::right);
                                    neighbour->stitchMesh();

                                    if (neighbour->mesh && !neighbour->mesh->empty()) {
                                        std::lock_guard<std::mutex> lock(workerMutex);
                                        readyToUpload.push_back(neighbour);
                                    }
                                }

                                chunk->generateIntermediateData();
                                chunk->stitchMesh();


                                if (chunk->mesh && !chunk->mesh->empty()) {
                                    std::lock_guard<std::mutex> lock(workerMutex);
                                    readyToUpload.push_back(chunk);
                                }
                            }
                        }
                        else if (type == workType::removeChunk) { 
                            Chunk* chunkToRemove = nullptr;
                            
                            {
                                std::lock_guard<std::mutex> lock(world::chunkRegistry::registryMutex);
                                auto it = world::chunkRegistry::registry.find(chunkPos);
                                if (it != world::chunkRegistry::registry.end()) {
                                    // Only deregister if not in ready queue
                                    if (std::find(readyToUpload.begin(), readyToUpload.end(), it->second) == readyToUpload.end()) {
                                        chunkToRemove = it->second;
                                    }
                                }
                            }
                            
                            // Deregister OUTSIDE the workerMutex lock to avoid deadlock
                            if (chunkToRemove) {
                                world::chunkRegistry::deregisterChunk(chunkPos);
                            }
                        }
                    }
                }
    };
}

#endif // CHUNK_WORKER_HEADER