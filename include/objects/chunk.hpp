#ifndef CHUNK_MANAGER_HEADER
#define CHUNK_MANAGER_HEADER

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
    }
}; 



using BlockData = /*X*/std::array</*Y*/std::array<std::array</*Z*/Block, CHUNK_HEIGHT>, CHUNK_WIDTH>, CHUNK_WIDTH>;



struct Chunk {
    private:
        // check whether the block is solid and acessable
        bool isBlock(const short x, const short y, const unsigned short z, const bool checkSurroundingChunks);
        void updateFlag(Block* block, const unsigned char flag, std::array<unsigned short, 3> localPosition);
        void scaleAndApplyVertice(const GLfloat& vertX, const GLfloat& vertY, const GLfloat& vertZ, const std::array<unsigned short int, 3>& localPosition);
        void addSideToMesh(mesh::Mesh* side, std::array<unsigned short int, 3> localPosition);

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
        Chunk(const Chunk& master): position(master.position), blockData(master.blockData), mesh(master.mesh) {}

        void bakeChunk();
        
        // generates visible block data - necesery for stitching the mesh
        void generateIntermediateData();

        void updateIntermediateData(const unsigned char x, const unsigned char y, const unsigned short& z, const bool ignoreBadCoord);

        bool registerChunk(glm::ivec2);
        bool deregisterChunk();

        // stitches the blocks in the current chunk into a single large mesh
        mesh::Mesh* stitchMesh();
};




class chunkRegistry {
    public:
        static inline std::unordered_map<glm::ivec2, Chunk*, ivec2_hash> registry = std::unordered_map<glm::ivec2, Chunk*, ivec2_hash>();
        static inline mesh::Mesh worldMesh = mesh::Mesh();

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

#endif // CHUNK_MANAGER_HEADER