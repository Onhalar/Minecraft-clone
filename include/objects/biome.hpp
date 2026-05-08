#ifndef BIOM_HEADER
#define BIOM_HEADER

#include <block.hpp>
#include <map>
#include <string>

namespace world {

    struct biome {
        blockID surfaceBlock = 0u;
        blockID topLayerBlock = 0u;
        blockID deepLayerBlock = 0u;

        // Climate axes — both in [-1, 1] noise space.
        // Place biomes far apart so each occupies a distinct territory.
        //
        //   humidity ^
        //     1.0    |  tundra      mountains
        //            |
        //     0.0    |
        //            |
        //    -1.0    |  desert      plains
        //            +----------------------------> temperature
        //              -1.0        1.0
        float temperature = 0.0f;
        float humidity    = 0.0f;

        int   octaves      = 6;       // more = more detail
        float persistence  = 0.5f;    // how fast amplitude drops per octave
        float lacunarity   = 2.0f;    // how fast frequency rises per octave
        float scale        = 0.03f;   // overall zoom (smaller = smoother/larger features)

        int   baseHeight   = 64;      // sea level / average terrain height
        int   heightRange  = 40;      // max deviation above/below base

        int   dirtDepth    = 3;       // how many dirt layers under grass
    };

    inline std::map<std::string, biome> biomeRegistry = {
        // Warm & dry — flat, low-frequency dunes
        { "desert", {
            blockIDlookup["sand"],
            blockIDlookup["sand"],
            blockIDlookup["stone"],
            /*temperature*/  0.8f,
            /*humidity*/    -0.8f,   // pushed further negative to distance from tundra
            /*octaves*/      3,
            /*persistence*/  0.3f,
            /*lacunarity*/   2.0f,
            /*scale*/        0.012f,
            /*baseHeight*/   60,
            /*heightRange*/  8,
            /*dirtDepth*/    2
        }},
        // Warm & moist — gentle rolling hills
        { "plains", {
            blockIDlookup["grass"],
            blockIDlookup["dirt"],
            blockIDlookup["stone"],
            /*temperature*/  0.6f,
            /*humidity*/     0.3f,   // opposite quadrant from desert
            /*octaves*/      4,
            /*persistence*/  0.4f,
            /*lacunarity*/   2.0f,
            /*scale*/        0.02f,
            /*baseHeight*/   64,
            /*heightRange*/  12,
            /*dirtDepth*/    3
        }},
        // Cold & moist — dramatic peaks
        { "mountains", {
            blockIDlookup["stone"],
            blockIDlookup["stone"],
            blockIDlookup["stone"],
            /*temperature*/  0.0f,
            /*humidity*/     0.4f,   // high humidity, cold — clearly distinct corner
            /*octaves*/      5,
            /*persistence*/  0.55f,
            /*lacunarity*/   2.1f,
            /*scale*/        0.005f,
            /*baseHeight*/   80,
            /*heightRange*/  140,
            /*dirtDepth*/    1
        }},
        // Cold & dry — flat, slightly icy plains
        { "tundra", {
            blockIDlookup["snow"],
            blockIDlookup["dirt"],
            blockIDlookup["stone"],
            /*temperature*/ -0.8f,
            /*humidity*/     0.1f,   // shifted positive to widen gap from desert
            /*octaves*/      4,
            /*persistence*/  0.45f,
            /*lacunarity*/   2.0f,
            /*scale*/        0.014f,
            /*baseHeight*/   62,
            /*heightRange*/  10,
            /*dirtDepth*/    3
        }},
    };
}

#endif // BIOM_HEADER