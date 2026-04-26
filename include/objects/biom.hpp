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

        int   octaves      = 6;       // more = more detail
        float persistence  = 0.5f;    // how fast amplitude drops per octave
        float lacunarity   = 2.0f;    // how fast frequency rises per octave
        float scale        = 0.03f;   // overall zoom (smaller = smoother/larger features)

        int   baseHeight   = 64;      // sea level / average terrain height
        int   heightRange  = 40;      // max deviation above/below base

        int   dirtDepth    = 3;       // how many dirt layers under grass
    };

    inline std::map<std::string, biom> biomRegistry = {
        {"plains",
            {
                &blockPalette["grass"],
                &blockPalette["dirt"],
                &blockPalette["stone"],
                4,0.4f,2.0f,0.015f,64,12,3,
            }
        }
    };
}

#endif // BIOM_HEADER