#ifndef CHUNK_MANAGER_HEADER
#define CHUNK_MANAGER_HEADER

#include <algorithm>
#include <core.hpp>
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
            // Use std::hash for the individual components
            std::size_t h1 = std::hash<int>()(v.x);
            std::size_t h2 = std::hash<int>()(v.y);
            
            // Combine hashes using a better mixing function
            // This approach works well with negative values
            return h1 ^ (h2 + 0x9e3779b9 + (h1 << 6) + (h1 >> 2));
        }
    };

    #define CHUNK_HEIGHT 255
    #define CHUNK_WIDTH 16



    // entry in intermediate data
    struct visibleBlock {
        std::array<unsigned short int, 3> localPosition = {0u, 0u, 0u};
        unsigned char flags = 0; // eg. top and bottom visible
        Block* origin;

        visibleBlock(Block* origin, const std::array<unsigned short int, 3>& localPosition): origin(origin), localPosition(localPosition) {}
        ~visibleBlock() {
            origin->acessPointer = nullptr;
            origin = nullptr;
        }
    }; 



    using BlockData = std::array<Block, CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT>;

    struct Chunk {
        private:
            bool isMeshUploaded = false;

            // check whether the block is solid and acessable
            bool isBlock(const short x, const short y, const unsigned short z, const bool checkSurroundingChunks);
            void updateFlag(Block* block, const unsigned char flag, std::array<unsigned short, 3> localPosition);
            void scaleAndApplyVertice(const GLfloat vertX, const GLfloat vertY, const GLfloat vertZ, const std::array<unsigned short int, 3>& localPosition);
            void scaleAndApplyUVs(const GLfloat UVx, const GLfloat UVy, const glm::fvec4 UVdata);
            void addSideToMesh(const unsigned char& sideFlag, mesh::meshData* side, const std::array<unsigned short int, 3>& localPosition);

            // updates the intermediate data of a single block; Use ignore flags to avoid unecesery checks or force flags which overpower ignore flags
            void updateBlockMeshFlags(const unsigned char x, const unsigned char y, const unsigned short z, const unsigned char ignoreFlags, const unsigned char forceFlags);

        public:
            glm::ivec2 position = {0u, 0u}; 
            BlockData blockData;
            mesh::meshData* mesh = nullptr;
            std::vector<visibleBlock> visibleBlockData;
            bool layerBlockPresence[CHUNK_HEIGHT]; // if a block is present in a layer
            bool baked = false;

            Chunk(glm::vec2 position, const bool registerChunk = true): position(position) { if (registerChunk) this->registerChunk(position); }
            Chunk(const bool registerChunk = true): position(glm::vec2(0.0f)) { if (registerChunk) this->registerChunk(position); }
            Chunk(const Chunk& master, const bool registerChunk = true): position(master.position), blockData(master.blockData), mesh(new mesh::meshData(*master.mesh)) { if (registerChunk) this->registerChunk(position); }

            ~Chunk() {
                visibleBlockData.clear();
                
                if (mesh) {
                    if (isMeshUploaded) { chunkRenderer->free(this->mesh->meshID); }
                    
                    delete mesh;
                    mesh = nullptr;
                }
            }

            void bakeChunk();
            
            // generates visible block data - necesery for stitching the mesh
            void generateIntermediateData();

            void updateIntermediateData(const unsigned char x, const unsigned char y, const unsigned short& z, const bool ignoreBadCoord);

            // give a chunk its x, y cooridnates
            bool registerChunk(glm::ivec2);
            void deregisterChunk();

            inline Block& getBlock(unsigned char x, unsigned char y, unsigned short z) { return this->blockData[ x + (y << 4) + (z << 8)]; }

            // stitches the blocks in the current chunk into a single large mesh
            mesh::meshData* stitchMesh();

            uint32_t uploadMesh(); // uploads mesh            
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
                std::lock_guard<std::mutex> lock(registryMutex);
                auto chunkIter = registry.find(position);
                if (chunkIter == registry.end()) { return false; }

                Chunk* chunk = chunkIter->second;
                if (chunk && chunk->mesh) {  // ← guard added
                    visibleChunks.erase(
                        std::remove(visibleChunks.begin(), visibleChunks.end(), chunk->mesh->meshID),
                        visibleChunks.end());
                }

                if (destroy) { delete chunk; chunkIter->second = nullptr; }
                registry.erase(chunkIter);
                return true;
            }

            static bool deregisterChunk(Chunk* chunk, const bool destroy = true) {
                std::lock_guard<std::mutex> lock(registryMutex);
                auto chunkIter = registry.find(chunk->position);
                if (chunkIter == registry.end()) { return false; }
                
                if (std::find(visibleChunks.begin(), visibleChunks.end(), chunkIter->second->mesh->meshID) != visibleChunks.end()) {
                    visibleChunks.erase(std::remove(visibleChunks.begin(), visibleChunks.end(), chunkIter->second->mesh->meshID), visibleChunks.end());
                }

                if (destroy) { delete chunkIter->second; chunkIter->second = nullptr; }
                registry.erase(chunkIter);
                return true;
            }

            static void uploadMeshes(const bool stitchMeshes = false) {
                std::lock_guard<std::mutex> lock(registryMutex);
                if (stitchMeshes) { for ( const auto [position, chunk] : registry ) { chunk->stitchMesh(); } }

                // TODO: make a propper is Chunk visible culler
                for ( const auto [position, chunk] : registry ) {
                    visibleChunks.push_back(chunk->uploadMesh());
                }
            }

            static bool isChunkRegistered(glm::ivec2 position) { return registry.find(position) != registry.end(); }
            // the same as isChunkRegistered
            static inline auto exists = isChunkRegistered;
    };











    // Helper functions:

    // 0,0 for current chunk, anything other is surrounding
    inline glm::ivec2 getblockChunkVector (const short x, const short y) {
        glm::ivec2 vector(0);

        if (x >= CHUNK_WIDTH) { vector.x = 1; }
        else if (x < 0) { vector.x = -1; }

        if (y >= CHUNK_WIDTH) { vector.y = 1; }
        else if (y < 0) { vector.y = -1; }

        return vector;
    }

    // DEFINITIONS

    // ---==[PRIVATE DEFINITIONS]==---

    inline bool Chunk::isBlock(const short x, const short y, const unsigned short z, const bool checkSurroundingChunks = true) {
        if (z >= CHUNK_HEIGHT) { return false; }
        glm::ivec2 blockChunkVector = getblockChunkVector(x, y);

        if (blockChunkVector != glm::ivec2(0)) {
            if (!checkSurroundingChunks) { return false; }

            glm::ivec2 neighbourChunkPos = position + blockChunkVector;

            std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
            auto it = chunkRegistry::registry.find(neighbourChunkPos);
            if (it == chunkRegistry::registry.end() || !it->second) { return false; }

            unsigned char altX = x - blockChunkVector.x * CHUNK_WIDTH;
            unsigned char altY = y - blockChunkVector.y * CHUNK_WIDTH;
            return it->second->getBlock(altX, altY, z).type != BlockType::air;
        }

        return getBlock(x, y, z).type != BlockType::air;
    }

    inline void Chunk::updateFlag(Block* block, const unsigned char flag, std::array<unsigned short, 3> localPosition) {
        if (block->acessPointer) {
            ((visibleBlock*)block->acessPointer)->flags |= flag;
        }
        else {
            visibleBlockData.push_back(visibleBlock(block, localPosition));
            auto entry = visibleBlockData.rbegin();
            entry->flags |= flag;
            block->acessPointer = &(*entry);

        }
    }

    inline void Chunk::scaleAndApplyVertice(const GLfloat vertX, const GLfloat vertY, const GLfloat vertZ, const std::array<unsigned short, 3>& localPosition) {
        this->mesh->vertices.push_back(vertX + (float)localPosition[0] + (CHUNK_WIDTH * (float)this->position.x));    // X
        this->mesh->vertices.push_back(vertY + (float)localPosition[1] + (CHUNK_WIDTH * (float)this->position.y));    // Y
        this->mesh->vertices.push_back(vertZ + (float)localPosition[2]);                                      // Z
    }

    inline void Chunk::scaleAndApplyUVs(const GLfloat UVx, const GLfloat UVy, const glm::fvec4 UVdata) {
        this->mesh->uvs.push_back(UVdata.x + (UVx * UVdata.z));
        this->mesh->uvs.push_back(UVdata.y + (UVy * UVdata.w)); // Flip Y
    }

    inline void Chunk::addSideToMesh(const unsigned char& sideFlag, mesh::meshData* side, const std::array<unsigned short int, 3>& localPosition) {
        size_t firstVertexIndex = this->mesh->vertices.size() / 3u; // last element + 1
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

        blockData = nullptr;
    }


    inline void Chunk::updateBlockMeshFlags(const unsigned char x, const unsigned char y, const unsigned short z, const unsigned char ignoreFlags = 0u, const unsigned char forceFlags = 0u) {
        if (getBlock(x, y, z).type == BlockType::air) { return; }
        if (getBlock(x, y, z).acessPointer) {
            if (((visibleBlock*)getBlock(x, y, z).acessPointer)->flags) {
                ((visibleBlock*)getBlock(x, y, z).acessPointer)->flags = 0u;
            }
        }

        // above
        if (!(ignoreFlags & blockRenderFlag::RENDER_TOP) | (forceFlags & blockRenderFlag::RENDER_TOP)) {
            if (!isBlock(x, y, z + 1)) { updateFlag(&getBlock(x, y, z), blockRenderFlag::RENDER_TOP, {x, y, z}); }
        }

        // below
        if (!(ignoreFlags & blockRenderFlag::RENDER_BOTTOM) | (forceFlags & blockRenderFlag::RENDER_BOTTOM)) {
            if (!isBlock(x, y, z - 1) || z == 0 /* fixes underflow */) { updateFlag(&getBlock(x, y, z), blockRenderFlag::RENDER_BOTTOM, {x, y, z}); }
        }

        // front
        if (!(ignoreFlags & blockRenderFlag::RENDER_FRONT) | (forceFlags & blockRenderFlag::RENDER_FRONT)) {
            if (!isBlock(x, (short)y - 1, z)) { updateFlag(&getBlock(x, y, z), blockRenderFlag::RENDER_FRONT, {x, y, z}); }
        }

        // back
        if (!(ignoreFlags & blockRenderFlag::RENDER_BACK) | (forceFlags & blockRenderFlag::RENDER_BACK)) {
            if (!isBlock(x, y + 1, z)) { updateFlag(&getBlock(x, y, z), blockRenderFlag::RENDER_BACK, {x, y, z}); }
        }

        // left
        if (!(ignoreFlags & blockRenderFlag::RENDER_LEFT) | (forceFlags & blockRenderFlag::RENDER_LEFT)) {
            if (!isBlock((short)x - 1, y, z)) { updateFlag(&getBlock(x, y, z), blockRenderFlag::RENDER_LEFT, {x, y, z}); }
        }

        // right
        if (!(ignoreFlags & blockRenderFlag::RENDER_RIGHT) | (forceFlags & blockRenderFlag::RENDER_RIGHT)) {
            if (!isBlock(x + 1, y, z)) { updateFlag(&getBlock(x, y, z), blockRenderFlag::RENDER_RIGHT, {x, y, z}); }
        }
    }

    // ---==[PUBLIC DEFINTIONS]==---

    inline void Chunk::bakeChunk() {
        for (unsigned int z = 0; z < CHUNK_HEIGHT; ++z) {
            for (unsigned int x = 0; x < CHUNK_WIDTH; ++x) {
                if (layerBlockPresence[z]) { break; }
                for (unsigned int y = 0; y < CHUNK_WIDTH; ++y) {
                    if (getBlock(x, y, z).type == BlockType::solid) { layerBlockPresence[z] = true; break; }
                }
            }
        }

        baked = true;
    }
            
    // generates visible block data - necesery for stitching the mesh
    inline void Chunk::generateIntermediateData() {
        if (!visibleBlockData.empty()) { visibleBlockData.clear(); }
        if (!baked) { bakeChunk(); }

        for (unsigned short int z = 0; z < CHUNK_HEIGHT; ++z) {
            if (!(layerBlockPresence[z] | !baked)) { continue; }

            for (unsigned char x = 0; x < CHUNK_WIDTH; ++x) {
                for (unsigned char y = 0; y < CHUNK_WIDTH; ++y) {
                    updateBlockMeshFlags(x, y, z);
                }
            }
        }
    }

    
    inline void Chunk::updateIntermediateData(const unsigned char x, const unsigned char y, const unsigned short& z, const bool ignoreBadCoord = true) {
        if (!isBlock(x, y, z)) {
            if (ignoreBadCoord) { return; }
            else { throw std::invalid_argument("ChunkUpdate: Cannot update non-existant block."); }
                    
            // ToDo: finsish this function
        }
    }

    // stitches the blocks in the current chunk into a single large mesh
    inline mesh::meshData* Chunk::stitchMesh() {
        if (mesh) {
            mesh->vertices.clear();
            mesh->indices.clear();
        }
        else {
            mesh = new mesh::meshData;
        }

        if (visibleBlockData.empty()) {
            if (baked) {
                for (const bool blockPresent : layerBlockPresence) {
                    if (blockPresent) {
                        generateIntermediateData();
                        break;
                    }
                }
            }
            else {
                generateIntermediateData();
            }
        }

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
            if (!mesh || mesh->empty()) return 0;

            uint32_t oldID = mesh->meshID;  // save BEFORE overwriting

            if (isMeshUploaded) {
                chunkRenderer->free(oldID);
            }

            mesh->meshID = chunkRenderer->upload(*mesh);
            isMeshUploaded = true;

            std::lock_guard<std::mutex> lock(chunkRegistry::registryMutex);
            if (isMeshUploaded) {
                // remove the OLD id from visible list, not the new one
                chunkRegistry::visibleChunks.erase(
                    std::remove(chunkRegistry::visibleChunks.begin(), chunkRegistry::visibleChunks.end(), oldID),
                    chunkRegistry::visibleChunks.end());
            }
            chunkRegistry::visibleChunks.push_back(mesh->meshID);

            return mesh->meshID;
        }

}

#endif // CHUNK_MANAGER_HEADER