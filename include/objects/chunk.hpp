#ifndef CHUNK_MANAGER_HEADER
#define CHUNK_MANAGER_HEADER

#include <algorithm>
#include <atomic>
#include <render.hpp>

#include "glm/ext/vector_int2.hpp"
#include <cstddef>
#include <glm/glm.hpp>
#include <mutex>
#include <types.hpp>
#include <glm/glm.hpp>
#include <mesh.hpp>
#include <unordered_map>
#include <vector>
#include <array>

#include <block.hpp>

namespace world {

    // hash function for glm::ivec2
    struct ivec2_hash {
        std::size_t operator()(const glm::ivec2& v) const {
            std::size_t h1 = std::hash<int>()(v.x);
            std::size_t h2 = std::hash<int>()(v.y);
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    #define CHUNK_HEIGHT 255
    #define CHUNK_WIDTH 16

    enum ChunkSides: unsigned char {
        front = 0b00000001,
        back  = 0b00000010,
        left  = 0b00000100,
        right = 0b00001000
    };

    inline glm::ivec2 getChunkPos(glm::ivec3 blockPos) {
        return  {
            (int)std::floor((float)blockPos.x / CHUNK_WIDTH),
            (int)std::floor((float)blockPos.y / CHUNK_WIDTH)
        };
    }

    struct visibleBlock {
        std::array<short, 3> localPosition = {0u, 0u, 0u};
        unsigned char flags = 0;
        Block* origin;

        visibleBlock(Block* origin, const std::array<short, 3>& localPosition)
            : origin(origin), localPosition(localPosition) {}

        visibleBlock(visibleBlock&& other) noexcept
            : localPosition(other.localPosition), flags(other.flags), origin(other.origin) {
            other.origin = nullptr;
            if (origin) { origin->acessPointer = this; }
        }

        visibleBlock& operator=(visibleBlock&& other) noexcept {
            if (this != &other) {
                if (origin) { origin->acessPointer = nullptr; }
                localPosition = other.localPosition;
                flags = other.flags;
                origin = other.origin;
                other.origin = nullptr;
                if (origin) { origin->acessPointer = this; }
            }
            return *this;
        }

        // No copying
        visibleBlock(const visibleBlock&) = delete;
        visibleBlock& operator=(const visibleBlock&) = delete;

        ~visibleBlock() {
            if (origin) {
                origin->acessPointer = nullptr;
            }
            origin = nullptr;
        }
    };

    using BlockData = std::array<Block, CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT>;

    struct Chunk {
        private:
            bool isMeshUploaded = false;

            bool isBlock(const short x, const short y, const unsigned short z, const bool checkSurroundingChunks);
            void updateFlag(Block* block, const unsigned char flag, std::array<short, 3> localPosition, const bool removeFlag);
            void scaleAndApplyVertice(const GLfloat vertX, const GLfloat vertY, const GLfloat vertZ, const std::array<short, 3>& localPosition);
            void scaleAndApplyUVs(const GLfloat UVx, const GLfloat UVy, const glm::fvec4 UVdata);
            void addSideToMesh(const unsigned char& sideFlag, mesh::meshData* side, const std::array<short, 3>& localPosition);

            // Standard slower updates for single block modifications
            void updateBlockMeshFlags(const short x, const short y, const unsigned short z, const unsigned char ignoreFlags, const unsigned char forceFlags);
            
            // Fast-path lookup for high-performance batch generation passes
            void updateBlockMeshFlagsWithNeighbors(const short x, const short y, const unsigned short z, Chunk* north, Chunk* south, Chunk* west, Chunk* east, const unsigned char ignoreFlags, const unsigned char forceFlags);

        public:
            std::recursive_mutex chunkMutex;

            std::atomic<bool> isBeingDeleted{ false };
            std::atomic<int> activeWorkers{ 0 };

            glm::ivec2 position = {0u, 0u}; 
            BlockData blockData;
            mesh::meshData* mesh = nullptr;
            std::vector<visibleBlock> visibleBlockData;

            Chunk(glm::ivec2 position, const bool registerChunk = true): position(position) { if (registerChunk) this->registerChunk(position); }
            Chunk(const bool registerChunk = true): position(glm::ivec2(0, 0)) { if (registerChunk) this->registerChunk(position); }
            Chunk(const Chunk& master, const bool registerChunk = true): position(master.position), blockData(master.blockData), mesh(new mesh::meshData(*master.mesh)) { if (registerChunk) this->registerChunk(position); }

            ~Chunk() {
                isBeingDeleted = true;
                std::lock_guard<std::recursive_mutex> lock(chunkMutex);
                visibleBlockData.clear();
                
                if (mesh) {
                    if (isMeshUploaded) { chunkRenderer->free(this->mesh->meshID); }
                    delete mesh;
                    mesh = nullptr;
                }
            }

            void generateIntermediateData();
            void regenerateSideIntermideateData(const ChunkSides& side);
            void updateBlockIntermediateData(const short x, const short y, const short& z, const bool ignoreBadCoord);

            bool registerChunk(glm::ivec2);
            void deregisterChunk();

            inline Block& getBlock(unsigned char x, unsigned char y, unsigned short z) { return this->blockData[ x + (y << 4) + (z << 8)]; }
            inline Block& getBlock(glm::ivec3 localBlockPos) { return this->blockData[ localBlockPos.x + (localBlockPos.y << 4) + (localBlockPos.z << 8)]; }

            mesh::meshData* stitchMesh();
            uint32_t uploadMesh();            
    };

    class chunkRegistry {
        public:
            static inline std::mutex registryMutex;
            static inline std::unordered_map<glm::ivec2, Chunk*, ivec2_hash> registry = std::unordered_map<glm::ivec2, Chunk*, ivec2_hash>();
            static inline std::vector<uint32_t> visibleChunks = {};

            static void deregisterAll() {
                std::lock_guard<std::mutex> lock(registryMutex);
                for (auto& [key, chunk] : registry) { if (chunk) { delete chunk; chunk = nullptr; } }
            }

            static bool registerChunk(Chunk* chunk, glm::ivec2 position) {
                std::lock_guard<std::mutex> lock(registryMutex);
                if (registry.find(position) != registry.end()) { return false; }

                registry[position] = chunk;
                chunk->position = position;
                return true;
            }

            static bool deregisterChunk(glm::ivec2 position, const bool destroy = true) {
                Chunk* chunk = nullptr;

                {
                    std::lock_guard<std::mutex> lock(registryMutex);
                    auto chunkIter = registry.find(position);
                    if (chunkIter == registry.end()) { return false; }

                    chunk = chunkIter->second;
                    if (chunk && chunk->mesh) {
                        visibleChunks.erase(
                            std::remove(visibleChunks.begin(), visibleChunks.end(), chunk->mesh->meshID),
                            visibleChunks.end());
                    }

                    if (chunk) { chunk->isBeingDeleted = true; }
                    registry.erase(chunkIter);
                }

                if (destroy && chunk) {
                    while (chunk->activeWorkers.load(std::memory_order_acquire) > 0) {
                        std::this_thread::yield();
                    }
                    delete chunk;
                }

                return true;
            }

            static bool deregisterChunk(Chunk* chunk, const bool destroy = true) {
                if (!chunk) { return false; }
                return deregisterChunk(chunk->position, destroy);
            }

            static void uploadMeshes(const bool stitchMeshes = false) {
                std::lock_guard<std::mutex> lock(registryMutex);
                if (stitchMeshes) { for ( const auto [position, chunk] : registry ) { chunk->stitchMesh(); } }

                for ( const auto [position, chunk] : registry ) {
                    visibleChunks.push_back(chunk->uploadMesh());
                }
            }

            static Chunk* getChunk(glm::ivec2 chunkPos, const bool preLockChunk = false) {
                std::lock_guard<std::mutex> lock(registryMutex);
                auto it = registry.find(chunkPos);
                if (it == registry.end()) { return nullptr; }
                Chunk* chunk = it->second;

                if (!chunk || chunk->isBeingDeleted.load(std::memory_order_acquire)) {
                    return nullptr;
                }

                if (preLockChunk) { chunk->chunkMutex.lock(); }
                return chunk;
            }

            static bool isChunkRegistered(glm::ivec2 position) { return registry.find(position) != registry.end(); }
            static inline auto exists = isChunkRegistered;
    };

    struct ChunkWorkerGuard {
        Chunk* chunk;
        bool claimed;

        explicit ChunkWorkerGuard(Chunk* c) : chunk(c), claimed(false) {
            if (!chunk) { return; }
            chunk->activeWorkers.fetch_add(1, std::memory_order_relaxed);
            if (chunk->isBeingDeleted.load(std::memory_order_acquire)) {
                chunk->activeWorkers.fetch_sub(1, std::memory_order_release);
            } else {
                claimed = true;
            }
        }

        ~ChunkWorkerGuard() {
            if (claimed) {
                chunk->activeWorkers.fetch_sub(1, std::memory_order_release);
            }
        }

        bool valid() const { return claimed; }

        ChunkWorkerGuard(const ChunkWorkerGuard&) = delete;
        ChunkWorkerGuard& operator=(const ChunkWorkerGuard&) = delete;
    };

    inline glm::ivec2 getblockChunkVector (const short x, const short y) {
        glm::ivec2 vector(0);
        if (x >= CHUNK_WIDTH) { vector.x = 1; }
        else if (x < 0) { vector.x = -1; }
        if (y >= CHUNK_WIDTH) { vector.y = 1; }
        else if (y < 0) { vector.y = -1; }
        return vector;
    }

    inline bool Chunk::isBlock(const short x, const short y, unsigned const short z, const bool checkSurroundingChunks = true) {
        if (z >= CHUNK_HEIGHT) { return false; }
        glm::ivec2 chunkVector = getblockChunkVector(x, y);

        if (checkSurroundingChunks && chunkVector != glm::ivec2(0)) {
            if (!chunkRegistry::exists(this->position + chunkVector)) { return true; }
            Chunk* chunk = chunkRegistry::getChunk(this->position + chunkVector);

            glm::ivec3 localPos = { x - chunkVector.x * CHUNK_WIDTH, y - chunkVector.y * CHUNK_WIDTH, z };
            if (localPos.x < 0 || localPos.x >= CHUNK_WIDTH || localPos.y < 0 || localPos.y >= CHUNK_WIDTH) { return false; }

            if (!chunk) { return true; }
            return chunk->getBlock(localPos).type != BlockType::air;
        }

        return getBlock(x, y, z).type != BlockType::air;
    }

    inline void Chunk::updateFlag(Block* block, const unsigned char flag, std::array<short, 3> localPosition, const bool removeFlag = false) {
        if (block->acessPointer) {
            if (removeFlag) { ((visibleBlock*)block->acessPointer)->flags &= ~flag; }
            else { ((visibleBlock*)block->acessPointer)->flags |= flag; }
        }
        else if (!removeFlag) {
            visibleBlockData.push_back(visibleBlock(block, localPosition));
            auto entry = visibleBlockData.rbegin();
            entry->flags |= flag;
            block->acessPointer = &(*entry);
        }
    }

    inline void Chunk::scaleAndApplyVertice(const GLfloat vertX, const GLfloat vertY, const GLfloat vertZ, const std::array<short, 3>& localPosition) {
        this->mesh->vertices.push_back(vertX + (float)localPosition[0] + (CHUNK_WIDTH * (float)this->position.x));
        this->mesh->vertices.push_back(vertY + (float)localPosition[1] + (CHUNK_WIDTH * (float)this->position.y));
        this->mesh->vertices.push_back(vertZ + (float)localPosition[2]);
    }

    inline void Chunk::scaleAndApplyUVs(const GLfloat UVx, const GLfloat UVy, const glm::fvec4 UVdata) {
        this->mesh->uvs.push_back(UVdata.x + (UVx * UVdata.z));
        this->mesh->uvs.push_back(UVdata.y + (UVy * UVdata.w));
    }

    inline void Chunk::addSideToMesh(const unsigned char& sideFlag, mesh::meshData* side, const std::array<short, 3>& localPosition) {
        size_t firstVertexIndex = this->mesh->vertices.size() / 3u;
        for (const auto& index : side->indices) {
            this->mesh->indices.push_back(index + firstVertexIndex);
        }

        for (auto i = side->vertices.begin(); i != side->vertices.end(); i += 3) {
            scaleAndApplyVertice(*i, *(i + 1), *(i + 2), localPosition);
        }

        BlockRef* blockData = &blockPalette[getBlock(localPosition[0], localPosition[1], localPosition[2]).ID];
        unsigned short textureID = 0u;

        if (sideFlag & blockRenderFlag::RENDER_FRONT) { textureID = blockData->palette.frontId; }
        else if (sideFlag & blockRenderFlag::RENDER_BACK) { textureID = blockData->palette.backId; }
        else if (sideFlag & blockRenderFlag::RENDER_TOP) { textureID = blockData->palette.topId; }
        else if (sideFlag & blockRenderFlag::RENDER_BOTTOM) { textureID = blockData->palette.bottomId; }
        else if (sideFlag & blockRenderFlag::RENDER_LEFT) { textureID = blockData->palette.leftId; }
        else if (sideFlag & blockRenderFlag::RENDER_RIGHT) { textureID = blockData->palette.rightId; }

        glm::fvec4 uvData = getTextureCoordinates(textureID);

        for (auto i = side->uvs.begin(); i != side->uvs.end(); i += 2) {
            scaleAndApplyUVs(*i, *(i+1), uvData);
        }
    }

    inline void Chunk::updateBlockMeshFlagsWithNeighbors(const short x, const short y, const unsigned short z, 
                                                         Chunk* north, Chunk* south, Chunk* west, Chunk* east,
                                                         const unsigned char ignoreFlags = 0u, const unsigned char forceFlags = 0u) {
        Block* block = &getBlock(x, y, z);
        if (block->type == BlockType::air) { return; }

        std::array<short, 3> localPos = { x, y, (short)z };

        // above
        if (!(ignoreFlags & blockRenderFlag::RENDER_TOP) || (forceFlags & blockRenderFlag::RENDER_TOP)) {
            if (z + 1 >= CHUNK_HEIGHT || getBlock(x, y, z + 1).type == BlockType::air || (forceFlags & blockRenderFlag::RENDER_TOP)) { updateFlag(block, blockRenderFlag::RENDER_TOP, localPos); }
            else { updateFlag(block, blockRenderFlag::RENDER_TOP, localPos, true); }
        }

        // below
        if (!(ignoreFlags & blockRenderFlag::RENDER_BOTTOM) || (forceFlags & blockRenderFlag::RENDER_BOTTOM)) {
            if (z == 0 || getBlock(x, y, z - 1).type == BlockType::air || (forceFlags & blockRenderFlag::RENDER_BOTTOM)) { updateFlag(block, blockRenderFlag::RENDER_BOTTOM, localPos); }
            else { updateFlag(block, blockRenderFlag::RENDER_BOTTOM, localPos, true); }
        }

        // front (y + 1)
        if (!(ignoreFlags & blockRenderFlag::RENDER_FRONT) || (forceFlags & blockRenderFlag::RENDER_FRONT)) {
            bool isSolid = (y + 1 >= CHUNK_WIDTH) ? (north ? north->getBlock(x, 0, z).type != BlockType::air : true) : (getBlock(x, y + 1, z).type != BlockType::air);
            if (!isSolid || (forceFlags & blockRenderFlag::RENDER_FRONT)) { updateFlag(block, blockRenderFlag::RENDER_FRONT, localPos); }
            else { updateFlag(block, blockRenderFlag::RENDER_FRONT, localPos, true); }
        }

        // back (y - 1)
        if (!(ignoreFlags & blockRenderFlag::RENDER_BACK) || (forceFlags & blockRenderFlag::RENDER_BACK)) {
            bool isSolid = (y - 1 < 0) ? (south ? south->getBlock(x, CHUNK_WIDTH - 1, z).type != BlockType::air : true) : (getBlock(x, y - 1, z).type != BlockType::air);
            if (!isSolid || (forceFlags & blockRenderFlag::RENDER_BACK)) { updateFlag(block, blockRenderFlag::RENDER_BACK, localPos); }
            else { updateFlag(block, blockRenderFlag::RENDER_BACK, localPos, true); }
        }

        // left (x - 1)
        if (!(ignoreFlags & blockRenderFlag::RENDER_LEFT) || (forceFlags & blockRenderFlag::RENDER_LEFT)) {
            bool isSolid = (x - 1 < 0) ? (west ? west->getBlock(CHUNK_WIDTH - 1, y, z).type != BlockType::air : true) : (getBlock(x - 1, y, z).type != BlockType::air);
            if (!isSolid || (forceFlags & blockRenderFlag::RENDER_LEFT)) { updateFlag(block, blockRenderFlag::RENDER_LEFT, localPos); }
            else { updateFlag(block, blockRenderFlag::RENDER_LEFT, localPos, true); }
        }

        // right (x + 1)
        if (!(ignoreFlags & blockRenderFlag::RENDER_RIGHT) || (forceFlags & blockRenderFlag::RENDER_RIGHT)) {
            bool isSolid = (x + 1 >= CHUNK_WIDTH) ? (east ? east->getBlock(0, y, z).type != BlockType::air : true) : (getBlock(x + 1, y, z).type != BlockType::air);
            if (!isSolid || (forceFlags & blockRenderFlag::RENDER_RIGHT)) { updateFlag(block, blockRenderFlag::RENDER_RIGHT, localPos); }
            else { updateFlag(block, blockRenderFlag::RENDER_RIGHT, localPos, true); }
        }

        if (block->acessPointer && !((visibleBlock*)block->acessPointer)->flags) {
            size_t idx = (visibleBlock*)block->acessPointer - visibleBlockData.data();
            if (idx != visibleBlockData.size() - 1) {
                visibleBlockData.back().origin->acessPointer = &visibleBlockData[idx];
                visibleBlockData[idx] = std::move(visibleBlockData.back());
            }
            visibleBlockData.pop_back();
            block->acessPointer = nullptr;
        }
    }

    inline void Chunk::updateBlockMeshFlags(const short x, const short y, const unsigned short z, const unsigned char ignoreFlags = 0u, const unsigned char forceFlags = 0u) {
        if (z >= CHUNK_HEIGHT) { return; }

        glm::ivec2 chunkVector = getblockChunkVector(x, y);
        Chunk* chunk;
        if (chunkVector == glm::ivec2(0)) {
            chunk = this;
            chunk->chunkMutex.lock();
        } else {
            chunk = chunkRegistry::getChunk(this->position + chunkVector, true);
        }

        if (!chunk) { return; }
        if (chunk->isBeingDeleted.load(std::memory_order_acquire)) { chunk->chunkMutex.unlock(); return; }

        short localX = x - chunkVector.x * CHUNK_WIDTH;
        short localY = y - chunkVector.y * CHUNK_WIDTH;
        std::array<short, 3> localPos = { localX, localY, (short)z };

        Block* block = &chunk->getBlock(localX, localY, z);
        if (block->type == BlockType::air) { chunk->chunkMutex.unlock(); return; }

        if (!(ignoreFlags & blockRenderFlag::RENDER_TOP) || (forceFlags & blockRenderFlag::RENDER_TOP)) {
            if (!chunk->isBlock(localX, localY, z + 1) || (forceFlags & blockRenderFlag::RENDER_TOP)) { chunk->updateFlag(block, blockRenderFlag::RENDER_TOP, localPos); }
            else { chunk->updateFlag(block, blockRenderFlag::RENDER_TOP, localPos, true); }
        }

        if (!(ignoreFlags & blockRenderFlag::RENDER_BOTTOM) || (forceFlags & blockRenderFlag::RENDER_BOTTOM)) {
            if (!chunk->isBlock(localX, localY, z - 1) || (forceFlags & blockRenderFlag::RENDER_BOTTOM)) { chunk->updateFlag(block, blockRenderFlag::RENDER_BOTTOM, localPos); }
            else { chunk->updateFlag(block, blockRenderFlag::RENDER_BOTTOM, localPos, true); }
        }

        if (!(ignoreFlags & blockRenderFlag::RENDER_FRONT) || (forceFlags & blockRenderFlag::RENDER_FRONT)) {
            if (!chunk->isBlock(localX, localY + 1, z) || (forceFlags & blockRenderFlag::RENDER_FRONT)) { chunk->updateFlag(block, blockRenderFlag::RENDER_FRONT, localPos); }
            else { chunk->updateFlag(block, blockRenderFlag::RENDER_FRONT, localPos, true); }
        }

        if (!(ignoreFlags & blockRenderFlag::RENDER_BACK) || (forceFlags & blockRenderFlag::RENDER_BACK)) {
            if (!chunk->isBlock(localX, localY - 1, z) || (forceFlags & blockRenderFlag::RENDER_BACK)) { chunk->updateFlag(block, blockRenderFlag::RENDER_BACK, localPos); }
            else { chunk->updateFlag(block, blockRenderFlag::RENDER_BACK, localPos, true); }
        }

        if (!(ignoreFlags & blockRenderFlag::RENDER_LEFT) || (forceFlags & blockRenderFlag::RENDER_LEFT)) {
            if (!chunk->isBlock(localX - 1, localY, z) || (forceFlags & blockRenderFlag::RENDER_LEFT)) { chunk->updateFlag(block, blockRenderFlag::RENDER_LEFT, localPos); }
            else { chunk->updateFlag(block, blockRenderFlag::RENDER_LEFT, localPos, true); }
        }

        if (!(ignoreFlags & blockRenderFlag::RENDER_RIGHT) || (forceFlags & blockRenderFlag::RENDER_RIGHT)) {
            if (!chunk->isBlock(localX + 1, localY, z) || (forceFlags & blockRenderFlag::RENDER_RIGHT)) { chunk->updateFlag(block, blockRenderFlag::RENDER_RIGHT, localPos); }
            else { chunk->updateFlag(block, blockRenderFlag::RENDER_RIGHT, localPos, true); }
        }

        if (block->acessPointer && !((visibleBlock*)block->acessPointer)->flags) {
            size_t idx = (visibleBlock*)block->acessPointer - chunk->visibleBlockData.data();
            if (idx != chunk->visibleBlockData.size() - 1) {
                chunk->visibleBlockData.back().origin->acessPointer = &chunk->visibleBlockData[idx];
                chunk->visibleBlockData[idx] = std::move(chunk->visibleBlockData.back());
            }
            chunk->visibleBlockData.pop_back();
            block->acessPointer = nullptr;
        }

        chunk->chunkMutex.unlock();
    }
            
    inline void Chunk::generateIntermediateData() {
        std::lock_guard<std::recursive_mutex> lock(chunkMutex); 
        if (!visibleBlockData.empty()) { visibleBlockData.clear(); }

        // Core lock optimization: Fetch neighboring references safely ONCE outside the loop
        Chunk* north = chunkRegistry::getChunk(this->position + glm::ivec2(0, 1));
        Chunk* south = chunkRegistry::getChunk(this->position + glm::ivec2(0, -1));
        Chunk* west  = chunkRegistry::getChunk(this->position + glm::ivec2(-1, 0));
        Chunk* east  = chunkRegistry::getChunk(this->position + glm::ivec2(1, 0));

        for (unsigned short int z = 0; z < CHUNK_HEIGHT; ++z) {
            for (unsigned char x = 0; x < CHUNK_WIDTH; ++x) {
                for (unsigned char y = 0; y < CHUNK_WIDTH; ++y) {
                    updateBlockMeshFlagsWithNeighbors(x, y, z, north, south, west, east);
                }
            }
        }
    }

    inline void Chunk::updateBlockIntermediateData(const short x, const short y, const short& z, const bool ignoreBadCoord = true) {
        if (x >= CHUNK_WIDTH || y >= CHUNK_WIDTH || z >= CHUNK_HEIGHT) { return; }

        updateBlockMeshFlags(x, y, z);
        updateBlockMeshFlags(x, y + 1, z);
        updateBlockMeshFlags(x, y - 1, z);
        updateBlockMeshFlags(x, y, z + 1);
        updateBlockMeshFlags(x, y, z - 1);
        updateBlockMeshFlags(x - 1, y, z);
        updateBlockMeshFlags(x + 1, y, z);
    }

    inline mesh::meshData* Chunk::stitchMesh() {
        std::lock_guard<std::recursive_mutex> lock(chunkMutex);

        if (mesh) { mesh->clear(); }
        else { mesh = new mesh::meshData; }

        if (visibleBlockData.empty()) { generateIntermediateData(); }

        if (visibleBlockData.empty()) { return mesh; }

        for (auto& entry : visibleBlockData) {
            if (entry.flags & blockRenderFlag::RENDER_TOP) { addSideToMesh(blockRenderFlag::RENDER_TOP, &mesh::defaults::Cube::top, entry.localPosition); }
            if (entry.flags & blockRenderFlag::RENDER_BOTTOM) { addSideToMesh(blockRenderFlag::RENDER_BOTTOM, &mesh::defaults::Cube::bottom, entry.localPosition); }
            if (entry.flags & blockRenderFlag::RENDER_FRONT) { addSideToMesh(blockRenderFlag::RENDER_FRONT, &mesh::defaults::Cube::front, entry.localPosition); }
            if (entry.flags & blockRenderFlag::RENDER_BACK) { addSideToMesh(blockRenderFlag::RENDER_BACK,&mesh::defaults::Cube::back, entry.localPosition); }
            if (entry.flags & blockRenderFlag::RENDER_LEFT) { addSideToMesh(blockRenderFlag::RENDER_LEFT, &mesh::defaults::Cube::left, entry.localPosition); }
            if (entry.flags & blockRenderFlag::RENDER_RIGHT) { addSideToMesh(blockRenderFlag::RENDER_RIGHT, &mesh::defaults::Cube::right, entry.localPosition); }
        }
        
        return mesh;
    }

    inline bool Chunk::registerChunk(glm::ivec2 position) { return chunkRegistry::registerChunk(this, position); }
    inline void Chunk::deregisterChunk() { chunkRegistry::deregisterChunk(this, false); }

    inline uint32_t Chunk::uploadMesh() {
        // ToDo: check if this doesn't slow things down
        std::lock_guard<std::recursive_mutex> lock(chunkMutex);

        if (!mesh || mesh->empty()) return 0;
        uint32_t oldID = mesh->meshID;

        if (isMeshUploaded) {
            chunkRenderer->free(oldID);
        }

        mesh->meshID = chunkRenderer->upload(*mesh);
        isMeshUploaded = true;

        std::lock_guard<std::mutex> rgLock(chunkRegistry::registryMutex);
        if (isMeshUploaded) {
            chunkRegistry::visibleChunks.erase(
                std::remove(chunkRegistry::visibleChunks.begin(), chunkRegistry::visibleChunks.end(), oldID),
                chunkRegistry::visibleChunks.end());
        }
        chunkRegistry::visibleChunks.push_back(mesh->meshID);

        return mesh->meshID;
    }

    inline void Chunk::regenerateSideIntermideateData(const ChunkSides& side) {
        std::lock_guard<std::recursive_mutex> lock(chunkMutex);

        Chunk* north = chunkRegistry::getChunk(this->position + glm::ivec2(0, 1));
        Chunk* south = chunkRegistry::getChunk(this->position + glm::ivec2(0, -1));
        Chunk* west  = chunkRegistry::getChunk(this->position + glm::ivec2(-1, 0));
        Chunk* east  = chunkRegistry::getChunk(this->position + glm::ivec2(1, 0));

        switch (side) {
            case ChunkSides::front:
                if (!north) { break; }
                for (unsigned short int z = 0; z < CHUNK_HEIGHT; ++z) {
                    for (unsigned char x = 0; x < CHUNK_WIDTH; ++x) {
                        updateBlockMeshFlagsWithNeighbors(x, CHUNK_WIDTH - 1, z, north, south, west, east, (unsigned char)-1 & ~blockRenderFlag::RENDER_FRONT);
                    }
                }
                break;

            case ChunkSides::back:
                if (!south) { break; }
                for (unsigned short int z = 0; z < CHUNK_HEIGHT; ++z) {
                    for (unsigned char x = 0; x < CHUNK_WIDTH; ++x) {
                        updateBlockMeshFlagsWithNeighbors(x, 0, z, north, south, west, east, (unsigned char)-1 & ~blockRenderFlag::RENDER_BACK);
                    }
                }
                break;

            case ChunkSides::left:
                if (!west) { break; }
                for (unsigned short int z = 0; z < CHUNK_HEIGHT; ++z) {
                    for (unsigned char y = 0; y < CHUNK_WIDTH; ++y) {
                        updateBlockMeshFlagsWithNeighbors(0, y, z, north, south, west, east, (unsigned char)-1 & ~blockRenderFlag::RENDER_LEFT);
                    }
                }
                break;

            case ChunkSides::right:
                if (!east) { break; }
                for (unsigned short int z = 0; z < CHUNK_HEIGHT; ++z) {
                    for (unsigned char y = 0; y < CHUNK_WIDTH; ++y) {
                        updateBlockMeshFlagsWithNeighbors(CHUNK_WIDTH - 1, y, z, north, south, west, east, (unsigned char)-1 & ~blockRenderFlag::RENDER_RIGHT);
                    }
                }
                break;
        }
    }
}

#endif // CHUNK_MANAGER_HEADER