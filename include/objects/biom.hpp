#ifndef BIOM_HEADER
#define BIOM_HEADER

#include <block.hpp>
#include <map>
#include <string>

namespace world {

    struct biom {
        BlockRef* surfaceBlock = nullptr;
        BlockRef* topLayerBlock = nullptr;
        BlockRef* deepLayerBlock = nullptr;
    };

    inline std::map<std::string, biom> biomRegistry = {
        {"plains", {&blockPalette["grass"], &blockPalette["dirt"], &blockPalette["stone"]} }
    };
}

#endif // BIOM_HEADER