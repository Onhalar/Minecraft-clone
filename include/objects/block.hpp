#ifndef BLOCK_DEFINITIONS_HEADER
#define BLOCK_DEFINITIONS_HEADER

#include <render.hpp>

#include "mesh.hpp"
#include <map>
#include <core.hpp>


namespace world {

    using blockID = unsigned short int;
    
    enum BlockType: bool {
        air,
        solid
    };

    struct blockTexture {
        blockID topId;
        blockID bottomId;

        blockID frontId;
        blockID backId;

        blockID leftId;
        blockID rightId;
    };

    inline glm::fvec4 getTextureCoordinates(blockID textureSheeetID) {
        static bool initialized = false;
        static glm::ivec2 textureDimensions(0);
        static GLfloat UVSizeX = 0.0f;
        static GLfloat UVSizeY = 0.0f;

        if (!mainTextureAtlas) { return glm::fvec4(0.0f); }

        if (!initialized) {
            textureDimensions = {
                mainTextureAtlas->widthImg / blockTextureWidth,
                mainTextureAtlas->heightImg / blockTextureWidth
            };
            UVSizeX = (float)blockTextureWidth / (float)mainTextureAtlas->widthImg;
            UVSizeY = (float)blockTextureWidth / (float)mainTextureAtlas->heightImg;
            initialized = true;
        }

        glm::ivec2 coordinates = {
            textureSheeetID % textureDimensions.x,
            textureSheeetID / textureDimensions.x
        };

        return glm::fvec4(
            (float)coordinates.x * UVSizeX,    // x - already normalized
            (float)(textureDimensions.y - 1 - coordinates.y) * UVSizeY,  // flipped
            UVSizeX,                            // UV width
            UVSizeY                             // UV height
        );
    }

    class blockRenderFlag {
        public:
            inline static const unsigned char RENDER_TOP =     0b00000001;
            inline static const unsigned char RENDER_BOTTOM =  0b00000010;
            inline static const unsigned char RENDER_FRONT =   0b00000100;
            inline static const unsigned char RENDER_BACK =    0b00001000;
            inline static const unsigned char RENDER_LEFT =    0b00010000;
            inline static const unsigned char RENDER_RIGHT =   0b00100000;
    };

    struct BlockSides {
        const mesh::meshData* const front = &mesh::defaults::Cube::front;
        const mesh::meshData* const back = &mesh::defaults::Cube::back;
        const mesh::meshData* const top = &mesh::defaults::Cube::top;
        const mesh::meshData* const bottom = &mesh::defaults::Cube::bottom;
        const mesh::meshData* const left = &mesh::defaults::Cube::left;
        const mesh::meshData* const right = &mesh::defaults::Cube::right;
    };

    // reference sheet for blocks
    struct BlockRef {
        const BlockSides sides;
        const blockTexture palette;
        const unsigned char forceFlags = 0;

        BlockRef(): palette({0,0,0,0,0,0}), sides(BlockSides()) {}
        BlockRef( const blockTexture& palette): palette(palette), sides(BlockSides()) {}
        BlockRef( const blockTexture& palette,
            const mesh::meshData* front,
            const mesh::meshData* back,
            const mesh::meshData* top,
            const mesh::meshData* bottom,
            const mesh::meshData* left,
            const mesh::meshData* right
        ): palette(palette), sides({front, back, top, bottom, left, right}) {}
    };

    struct Block {
        blockID ID = 0u;
        BlockType type;
        void* acessPointer = nullptr; // here to access possible visibleBlock entries NULL BY DEFAULT

        Block(): type(BlockType::air) {}
        Block(blockID ID): ID(ID), type(BlockType::solid) {}
        Block(blockID ID, BlockType type): ID(ID), type(type) {}
    };


    inline std::vector<BlockRef> blockPalette = {
        BlockRef({0, 2, 1, 1, 1, 1}),
        BlockRef({2, 2, 2, 2, 2, 2}),
        BlockRef({3, 3, 3, 3, 3, 3}),
        BlockRef({4, 4, 4, 4, 4, 4}),
        BlockRef({5, 5, 5, 5, 5, 5})
    };

    inline std::map<std::string, blockID> blockIDlookup = {
        {"grass", 0u},
        {"dirt", 1u},
        {"stone", 2u},
        {"snow", 3u},
        {"sand", 4u}
    };
}



#endif // BLOCK_DEFINITIONS_HEADER