#ifndef CHUNK_MANAGER_HEADER
#define CHUNK_MANAGER_HEADER

#include <core.hpp>
#include "glm/ext/vector_int2.hpp"
#include <glm/glm.hpp>
#include <types.hpp>
#include <glm/glm.hpp>
#include <mesh.hpp>
#include <unordered_map>
#include <vector>
#include <array>

#include <block.hpp>

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

// active faces
class blockRenderFlag {
    public:
        inline static const unsigned char RENDER_TOP =     0b00000001;
        inline static const unsigned char RENDER_BOTTOM =  0b00000010;
        inline static const unsigned char RENDER_FRONT =   0b00000100;
        inline static const unsigned char RENDER_BACK =    0b00001000;
        inline static const unsigned char RENDER_LEFT =    0b00010000;
        inline static const unsigned char RENDER_RIGHT =   0b00100000;
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



using BlockData = /*X*/std::array</*Y*/std::array<std::array</*Z*/Block, CHUNK_HEIGHT>, CHUNK_WIDTH>, CHUNK_WIDTH>;



struct Chunk {
    private:
        // check whether the block is solid and acessable
        bool isBlock(const short x, const short y, const unsigned short z, const bool checkSurroundingChunks);
        void updateFlag(Block* block, const unsigned char flag, std::array<unsigned short, 3> localPosition);
        void scaleAndApplyVertice(const GLfloat vertX, const GLfloat vertY, const GLfloat vertZ, const std::array<unsigned short int, 3>& localPosition);
        void scaleAndApplyUVs(const GLfloat UVx, const GLfloat UVy, const glm::fvec4 UVdata);
        void addSideToMesh(const unsigned char& sideFlag, mesh::Mesh* side, const std::array<unsigned short int, 3>& localPosition);

        // updates the intermediate data of a single block; Use ignore flags to avoid unecesery checks or force flags which overpower ignore flags
        void updateBlockMeshFlags(const unsigned char x, const unsigned char y, const unsigned short z, const unsigned char ignoreFlags, const unsigned char forceFlags);

    public:
        glm::ivec2 position = {0u, 0u}; 
        BlockData blockData;
        mesh::Mesh* mesh = nullptr;
        std::vector<visibleBlock> visibleBlockData;
        bool layerBlockPresence[CHUNK_HEIGHT]; // if a block is present in a layer
        bool baked = false;

        Chunk(glm::vec2 position): position(position) {}
        Chunk(): position(glm::vec2(0.0f)) {}
        Chunk(const Chunk& master): position(master.position), blockData(master.blockData), mesh(new mesh::Mesh(*master.mesh)) {}

        ~Chunk() {
            visibleBlockData.clear();
            
            if (mesh) {
                delete mesh;
                mesh = nullptr;
            }
        }

        void bakeChunk();
        
        // generates visible block data - necesery for stitching the mesh
        void generateIntermediateData();

        void updateIntermediateData(const unsigned char x, const unsigned char y, const unsigned short& z, const bool ignoreBadCoord);

        bool registerChunk(glm::ivec2);

        // stitches the blocks in the current chunk into a single large mesh
        mesh::Mesh* stitchMesh();
};




class chunkRegistry {
    public:
        static inline std::unordered_map<glm::ivec2, Chunk*, ivec2_hash> registry = std::unordered_map<glm::ivec2, Chunk*, ivec2_hash>();
        static inline mesh::Mesh worldMesh = mesh::Mesh();

        static void deregisterAll() {
            for (auto& [key, chunk] : registry) { delete chunk; }
        }

        static bool registerChunk(Chunk* chunk, glm::ivec2 position) {
            if (registry.find(position) != registry.end()) { return false; }

            registry[position] = chunk;
            chunk->position = position;
            return true;
        }

        static bool deregisterChunk(glm::ivec2 position, const bool destroy = true) {
            auto chunkIter = registry.find(position);
            if (chunkIter == registry.end()) { return false; }

            if (destroy) { delete chunkIter->second; }
            registry.erase(chunkIter);
            return true;
        }

        static bool deregisterChunk(Chunk* chunk, const bool destroy = true) {
            auto chunkIter = registry.find(chunk->position);
            if (chunkIter == registry.end()) { return false; }
            
            if (destroy) { delete chunkIter->second; }
            registry.erase(chunkIter);
            return true;
        }

        static bool isChunkRegistered(glm::ivec2 position) { return registry.find(position) != registry.end(); }
        // the same as isChunkRegistered
        static inline auto exists = isChunkRegistered;

        static mesh::Mesh stitchRegistryMesh(const bool restitchMeshes = false) {
            if (!worldMesh.empty()) { worldMesh = mesh::Mesh(); }

            for (const auto& [key, chunk] : registry) {
                worldMesh.merge(restitchMeshes ? chunk->stitchMesh() : chunk->mesh);
            }

            return worldMesh;
        }
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

// retrieves the begining of a texture
inline glm::fvec4 getTextureCoordinates(unsigned short int textureSheeetID) {
    static glm::ivec2 textureDimensions(0);
    static GLfloat UVSizeX = 0.0f;
    static GLfloat UVSizeY = 0.0f;

    if (textureDimensions == glm::ivec2(0) && mainTextureAtlas) {
        textureDimensions = {mainTextureAtlas->widthImg / blockTextureWidth, mainTextureAtlas->heightImg / blockTextureWidth};
        UVSizeX = (float)blockTextureWidth / (float)mainTextureAtlas->widthImg;
        UVSizeY =  (float)blockTextureWidth / (float)mainTextureAtlas->heightImg;
    }
    
    glm::ivec2 coordinates = {textureSheeetID % textureDimensions.x, textureSheeetID / textureDimensions.x};

    glm::fvec4 UV = {
        ((float)coordinates.x * UVSizeX),  // x - already normalized
        ((float)coordinates.y * UVSizeY),  // y - already normalized  
        UVSizeX,                          // UVx width
        UVSizeY                           // UVy height
    };

    return UV;
}


// DEFINITIONS

// ---==[PRIVATE DEFINITIONS]==---

inline bool Chunk::isBlock(const short x, const short y, const unsigned short z, const bool checkSurroundingChunks = true ) {
    if (z >= CHUNK_HEIGHT) { return false; }
    glm::ivec2 blockChunkVector = getblockChunkVector(x, y);
    
    if (blockChunkVector != glm::ivec2(0)) {
        if (!checkSurroundingChunks) { return false; }

        glm::ivec2 neighbourChunkPos = position + blockChunkVector;

        if (!chunkRegistry::exists(neighbourChunkPos)) { return false; }

        unsigned char altX = x - blockChunkVector.x * CHUNK_WIDTH;
        unsigned char altY = y - blockChunkVector.y * CHUNK_WIDTH;

        if (chunkRegistry::registry[neighbourChunkPos]->blockData[altX][altY][z].type != BlockType::air) { return true; }
    }
    else if (blockData[x][y][z].type != BlockType::air) { return true; }

    return false;
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
    this->mesh->UVs.push_back(UVdata.x + (UVx * UVdata.z));
    this->mesh->UVs.push_back(UVdata.y + (UVy * UVdata.w)); // Flip Y
}

inline void Chunk::addSideToMesh(const unsigned char& sideFlag, mesh::Mesh* side, const std::array<unsigned short int, 3>& localPosition) {
    size_t firstVertexIndex = this->mesh->vertices.size() / 3u; // last element + 1
    for (const auto& index : side->indices) {
        this->mesh->indices.push_back(index + firstVertexIndex);
    }

    for (auto i = side->vertices.begin(); i != side->vertices.end(); i += 3) {
        scaleAndApplyVertice(*i, *(i + 1), *(i + 2), localPosition);
    }

    unsigned short blockPaletteID = this->blockData[localPosition[0]][localPosition[1]][localPosition[2]].textureSheeetID;
    unsigned short textureID = 0u;


    if (sideFlag & blockRenderFlag::RENDER_FRONT) { textureID = blockPalette[blockPaletteID].frontId; }
    else if (sideFlag & blockRenderFlag::RENDER_BACK) { textureID = blockPalette[blockPaletteID].backId; }
    else if (sideFlag & blockRenderFlag::RENDER_TOP) { textureID = blockPalette[blockPaletteID].topId; }
    else if (sideFlag & blockRenderFlag::RENDER_BOTTOM) { textureID = blockPalette[blockPaletteID].bottomId; }
    else if (sideFlag & blockRenderFlag::RENDER_LEFT) { textureID = blockPalette[blockPaletteID].leftId; }
    else if (sideFlag & blockRenderFlag::RENDER_RIGHT) { textureID = blockPalette[blockPaletteID].rightId; }

    glm::fvec4 uvData = getTextureCoordinates(textureID);

    std::cout << "UV data: " << uvData.x << ", " << uvData.y << ", " << uvData.z << std::endl;

    for (auto i = side->UVs.begin(); i != side->UVs.end(); i += 2) {
        scaleAndApplyUVs(*i, *(i+1), uvData);
    }
}


inline void Chunk::updateBlockMeshFlags(const unsigned char x, const unsigned char y, const unsigned short z, const unsigned char ignoreFlags = 0u, const unsigned char forceFlags = 0u) {
    if (blockData[x][y][z].type == BlockType::air) { return; }
    if (blockData[x][y][z].acessPointer) {
        if (((visibleBlock*)blockData[x][y][z].acessPointer)->flags) {
            ((visibleBlock*)blockData[x][y][z].acessPointer)->flags = 0u;
        }
    }

    // above
    if (!(ignoreFlags & blockRenderFlag::RENDER_TOP) | (forceFlags & blockRenderFlag::RENDER_TOP)) {
        if (!isBlock(x, y, z + 1)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_TOP, {x, y, z}); }
    }

    // below
    if (!(ignoreFlags & blockRenderFlag::RENDER_BOTTOM) | (forceFlags & blockRenderFlag::RENDER_BOTTOM)) {
        if (!isBlock(x, y, z - 1) || z == 0 /* fixes underflow */) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_BOTTOM, {x, y, z}); }
    }

    // front
    if (!(ignoreFlags & blockRenderFlag::RENDER_FRONT) | (forceFlags & blockRenderFlag::RENDER_FRONT)) {
        if (!isBlock(x, (short)y - 1, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_FRONT, {x, y, z}); }
    }

    // back
    if (!(ignoreFlags & blockRenderFlag::RENDER_BACK) | (forceFlags & blockRenderFlag::RENDER_BACK)) {
        if (!isBlock(x, y + 1, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_BACK, {x, y, z}); }
    }

    // left
    if (!(ignoreFlags & blockRenderFlag::RENDER_LEFT) | (forceFlags & blockRenderFlag::RENDER_LEFT)) {
        if (!isBlock((short)x - 1, y, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_LEFT, {x, y, z}); }
    }

    // right
    if (!(ignoreFlags & blockRenderFlag::RENDER_RIGHT) | (forceFlags & blockRenderFlag::RENDER_RIGHT)) {
        if (!isBlock(x + 1, y, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_RIGHT, {x, y, z}); }
    }
}

// ---==[PUBLIC DEFINTIONS]==---

inline void Chunk::bakeChunk() {
    for (unsigned int z = 0; z < CHUNK_HEIGHT; ++z) {
        for (unsigned int x = 0; x < CHUNK_WIDTH; ++x) {
            if (layerBlockPresence[z]) { break; }
            for (unsigned int y = 0; y < CHUNK_WIDTH; ++y) {
                if (blockData[x][y][z].type == BlockType::solid) { layerBlockPresence[z] = true; break; }
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
inline mesh::Mesh* Chunk::stitchMesh() {
    if (mesh) {
        mesh->vertices.clear();
        mesh->indices.clear();
    }
    else {
        mesh = new mesh::Mesh;
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
        if (entry.flags & blockRenderFlag::RENDER_TOP) { addSideToMesh(blockRenderFlag::RENDER_TOP, &mesh::cubeDefaults::sides::top, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_BOTTOM) { addSideToMesh(blockRenderFlag::RENDER_BOTTOM, &mesh::cubeDefaults::sides::bottom, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_FRONT) { addSideToMesh(blockRenderFlag::RENDER_FRONT, &mesh::cubeDefaults::sides::front, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_BACK) { addSideToMesh(blockRenderFlag::RENDER_BACK,&mesh::cubeDefaults::sides::back, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_LEFT) { addSideToMesh(blockRenderFlag::RENDER_LEFT, &mesh::cubeDefaults::sides::left, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_RIGHT) { addSideToMesh(blockRenderFlag::RENDER_RIGHT, &mesh::cubeDefaults::sides::right, entry.localPosition); }
    }

    return mesh;
}

inline bool Chunk::registerChunk(glm::ivec2 position) { return chunkRegistry::registerChunk(this, position); }

#endif // CHUNK_MANAGER_HEADER