#ifndef BLOCK_DEFINITIONS_HEADER
#define BLOCK_DEFINITIONS_HEADER

#include "mesh.hpp"
#include <map>
#include <core.hpp>


namespace world {
    
    enum BlockType: bool {
        air,
        solid
    };

    struct blockTexture {
        unsigned short topId;
        unsigned short bottomId;

        unsigned short frontId;
        unsigned short backId;

        unsigned short leftId;
        unsigned short rightId;
    };

    inline glm::fvec4 getTextureCoordinates(unsigned short int textureSheeetID) {
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
        const mesh::Mesh* const front = &mesh::cubeDefaults::sides::front;
        const mesh::Mesh* const back = &mesh::cubeDefaults::sides::back;
        const mesh::Mesh* const top = &mesh::cubeDefaults::sides::top;
        const mesh::Mesh* const bottom = &mesh::cubeDefaults::sides::bottom;
        const mesh::Mesh* const left = &mesh::cubeDefaults::sides::left;
        const mesh::Mesh* const right = &mesh::cubeDefaults::sides::right;
    };

    // reference sheet for blocks
    struct BlockRef {
        const BlockSides sides;
        const blockTexture palette;
        const unsigned char forceFlags = 0;

        BlockRef(): palette({0,0,0,0,0,0}), sides(BlockSides()) {}
        BlockRef( const blockTexture& palette): palette(palette), sides(BlockSides()) {}
        BlockRef( const blockTexture& palette,
            const mesh::Mesh* front,
            const mesh::Mesh* back,
            const mesh::Mesh* top,
            const mesh::Mesh* bottom,
            const mesh::Mesh* left,
            const mesh::Mesh* right
        ): palette(palette), sides({front, back, top, bottom, left, right}) {}
    };

    struct Block {
        BlockRef* blockData = nullptr;
        BlockType type;
        void* acessPointer = nullptr; // here to access possible visibleBlock entries NULL BY DEFAULT

        Block(): type(BlockType::air) {}
        Block(BlockRef* blockData): blockData(blockData), type(BlockType::solid) {}
        Block(BlockRef* blockData, BlockType type): blockData(blockData), type(type) {}
    };


    inline std::map<std::string, BlockRef> blockPalette = {
        {"grass", BlockRef({0, 2, 1, 1, 1, 1}) },
        {"dirt", BlockRef({2, 2, 2, 2, 2, 2}) },
        {"stone", BlockRef({3, 3, 3, 3, 3, 3}) }
    };
}



#endif // BLOCK_DEFINITIONS_HEADER