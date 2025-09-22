#ifndef CHUNK_MANAGER_HEADER
#define CHUNK_MANAGER_HEADER

#include <stdexcept>
#include <types.hpp>
#include <glm/glm.hpp>
#include <mesh.hpp>
#include <vector>
#include <array>

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

enum BlockType: unsigned char {
    air,
    solid
};

struct Block {
    unsigned short int textureSheeetID;
    BlockType type;
    void* acessPointer = nullptr; // here to access possible visibleBlock entries

    Block(): textureSheeetID(0u), type(BlockType::air) {}
    Block(unsigned short int textureSheeetID): textureSheeetID(textureSheeetID), type(BlockType::solid) {}
    Block(unsigned short int textureSheeetID, BlockType type): textureSheeetID(textureSheeetID), type(type) {}
};

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
        void updateFlag(Block* block, const unsigned char& flag, std::array<unsigned short int, 3> localPosition) {
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

        // ToDo: add surrounding chunk check.
        bool isBlock(const unsigned char& x, const unsigned char& y, const unsigned short& z, const bool& checkOnlyExists = false, const bool& checkSurroundingChunks = false ) {
            if (x >= CHUNK_WIDTH || y >= CHUNK_WIDTH || z >= CHUNK_HEIGHT) { return false; }
            if (checkOnlyExists) { return true; }

            if (blockData[x][y][z].type != BlockType::air) { return true; }
            return false;
        }

        void scaleAndApplyVertice(const GLfloat& vertX, const GLfloat& vertY, const GLfloat& vertZ, const std::array<unsigned short int, 3>& localPosition) {
            this->mesh->vertices.push_back(vertX + (float)localPosition[0] + (CHUNK_WIDTH * (float)this->position.x));    // X
            this->mesh->vertices.push_back(vertY + (float)localPosition[1] + (CHUNK_WIDTH * (float)this->position.y));    // Y
            this->mesh->vertices.push_back(vertZ + (float)localPosition[2]);                                      // Z
        }

        void addSideToMesh(mesh::Mesh* side, std::array<unsigned short int, 3> localPosition) {
            size_t firstVertexIndex = this->mesh->vertices.size() / 3u; // last element + 1
            for (const auto& index : side->indices) {
                this->mesh->indices.push_back(index + firstVertexIndex);
            }

            for (auto i = side->vertices.begin(); i != side->vertices.end(); i += 3) {
                scaleAndApplyVertice(*i, *(i + 1), *(i + 2), localPosition);
            }
        }

        // updates the intermediate data of a single block; Use ignore flags to avoid unecesery checks or force flags which overpower ignore flags
        void updateBlockMeshFlags(const unsigned char& x, const unsigned char& y, const unsigned short& z, const unsigned char& ignoreFlags = 0u, const unsigned char& forceFlags = 0u) {
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
                if (!isBlock(x, y - 1, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_FRONT, {x, y, z}); }
            }

            // back
            if (!(ignoreFlags & blockRenderFlag::RENDER_BACK) | (forceFlags & blockRenderFlag::RENDER_BACK)) {
                if (!isBlock(x, y + 1, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_BACK, {x, y, z}); }
            }

            // left
            if (!(ignoreFlags & blockRenderFlag::RENDER_LEFT) | (forceFlags & blockRenderFlag::RENDER_LEFT)) {
                if (!isBlock(x - 1, y, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_LEFT, {x, y, z}); }
            }

            // right
            if (!(ignoreFlags & blockRenderFlag::RENDER_RIGHT) | (forceFlags & blockRenderFlag::RENDER_RIGHT)) {
                if (!isBlock(x + 1, y, z)) { updateFlag(&blockData[x][y][z], blockRenderFlag::RENDER_RIGHT, {x, y, z}); }
            }
        }

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

        void bakeChunk() {

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
        void generateIntermediateData() {
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

        // 
        void updateIntermediateData(const unsigned char& x, const unsigned char& y, const unsigned int& z, const bool& ignoreBadCoord = true) {
            if (!isBlock(x, y, z)) {
                if (ignoreBadCoord) { return; }
                else { throw std::invalid_argument("ChunkUpdate: Cannot update non-existant block."); }
                
                // ToDo: finsish this function
            }
        }

        // stitches the blocks in the current chunk into a single large mesh
        mesh::Mesh* stitchMesh() {
            if (mesh) {
                mesh->vertices.clear();
                mesh->indices.clear();
            }
            else {
                mesh = new mesh::Mesh;
            }

            //if (visibleBlockData.empty()) { generateIntermediateData(); }

            for (auto& entry : visibleBlockData) {
                if (entry.flags & blockRenderFlag::RENDER_TOP) { addSideToMesh(&mesh::cubeDefaults::sides::top, entry.localPosition); }
                if (entry.flags & blockRenderFlag::RENDER_BOTTOM) { addSideToMesh(&mesh::cubeDefaults::sides::bottom, entry.localPosition); }
                if (entry.flags & blockRenderFlag::RENDER_FRONT) { addSideToMesh(&mesh::cubeDefaults::sides::front, entry.localPosition); }
                if (entry.flags & blockRenderFlag::RENDER_BACK) { addSideToMesh(&mesh::cubeDefaults::sides::back, entry.localPosition); }
                if (entry.flags & blockRenderFlag::RENDER_LEFT) { addSideToMesh(&mesh::cubeDefaults::sides::left, entry.localPosition); }
                if (entry.flags & blockRenderFlag::RENDER_RIGHT) { addSideToMesh(&mesh::cubeDefaults::sides::right, entry.localPosition); }
            }

            return mesh;
        }
};

inline std::vector<Chunk*> Archive;

#endif // CHUNK_MANAGER_HEADER