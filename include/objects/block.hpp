#ifndef BLOCK_DEFINITIONS_HEADER
#define BLOCK_DEFINITIONS_HEADER

#include <vector>


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

struct Block {
    unsigned short int textureSheeetID;
    BlockType type;
    void* acessPointer = nullptr; // here to access possible visibleBlock entries

    Block(): textureSheeetID(0u), type(BlockType::air) {}
    Block(unsigned short int textureSheeetID): textureSheeetID(textureSheeetID), type(BlockType::solid) {}
    Block(unsigned short int textureSheeetID, BlockType type): textureSheeetID(textureSheeetID), type(type) {}
};

inline std::vector<blockTexture> blockPalette = {
    {0, 2, 1, 1, 1, 1}, // ID 0 - grass
    {2, 2, 2, 2, 2, 2}  // ID 1 - dirt
};



#endif // BLOCK_DEFINITIONS_HEADER