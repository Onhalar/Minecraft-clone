#include "chunk.hpp"

// Helper functions:

// 0,0 for current chunk, anything other is surrounding
glm::ivec2 getblockChunkVector (const short x, const short y) {
    glm::ivec2 vector(0);

    if (x >= CHUNK_WIDTH) { vector.x = 1; }
    else if (x < 0) { vector.x = -1; }

    if (y >= CHUNK_WIDTH) { vector.y = 1; }
    else if (y < 0) { vector.y = -1; }

    return vector;
}

// DEFINITIONS

// ---==[PRIVATE DEFINITIONS]==---

bool Chunk::isBlock(const short x, const short y, const unsigned short z, const bool checkSurroundingChunks = true ) {
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

void Chunk::updateFlag(Block* block, const unsigned char flag, std::array<unsigned short, 3> localPosition) {
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

void Chunk::scaleAndApplyVertice(const GLfloat& vertX, const GLfloat& vertY, const GLfloat& vertZ, const std::array<unsigned short, 3>& localPosition) {
    this->mesh->vertices.push_back(vertX + (float)localPosition[0] + (CHUNK_WIDTH * (float)this->position.x));    // X
    this->mesh->vertices.push_back(vertY + (float)localPosition[1] + (CHUNK_WIDTH * (float)this->position.y));    // Y
    this->mesh->vertices.push_back(vertZ + (float)localPosition[2]);                                      // Z
}

void Chunk::addSideToMesh(mesh::Mesh* side, std::array<unsigned short int, 3> localPosition) {
    size_t firstVertexIndex = this->mesh->vertices.size() / 3u; // last element + 1
    for (const auto& index : side->indices) {
        this->mesh->indices.push_back(index + firstVertexIndex);
    }

    for (auto i = side->vertices.begin(); i != side->vertices.end(); i += 3) {
        scaleAndApplyVertice(*i, *(i + 1), *(i + 2), localPosition);
    }
}


void Chunk::updateBlockMeshFlags(const unsigned char x, const unsigned char y, const unsigned short z, const unsigned char ignoreFlags = 0u, const unsigned char forceFlags = 0u) {
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

void Chunk::bakeChunk() {
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
void Chunk::generateIntermediateData() {
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

 
void Chunk::updateIntermediateData(const unsigned char x, const unsigned char y, const unsigned short& z, const bool ignoreBadCoord = true) {
    if (!isBlock(x, y, z)) {
        if (ignoreBadCoord) { return; }
        else { throw std::invalid_argument("ChunkUpdate: Cannot update non-existant block."); }
                
        // ToDo: finsish this function
    }
}

// stitches the blocks in the current chunk into a single large mesh
mesh::Mesh* Chunk::stitchMesh() {
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
        if (entry.flags & blockRenderFlag::RENDER_TOP) { addSideToMesh(&mesh::cubeDefaults::sides::top, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_BOTTOM) { addSideToMesh(&mesh::cubeDefaults::sides::bottom, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_FRONT) { addSideToMesh(&mesh::cubeDefaults::sides::front, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_BACK) { addSideToMesh(&mesh::cubeDefaults::sides::back, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_LEFT) { addSideToMesh(&mesh::cubeDefaults::sides::left, entry.localPosition); }
        if (entry.flags & blockRenderFlag::RENDER_RIGHT) { addSideToMesh(&mesh::cubeDefaults::sides::right, entry.localPosition); }
    }

    return mesh;
}

bool Chunk::registerChunk(glm::ivec2 position) { return chunkRegistry::registerChunk(this, position); }

bool Chunk::deregisterChunk() { return chunkRegistry::deregisterChunk(this); }