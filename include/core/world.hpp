#ifndef CORE_WORLD_HEADER
#define CORE_WORLD_HEADER

namespace WorldSettings {
    inline unsigned int seed = 0u;

    // Controls biome territory size.
    // Smaller = larger biomes, larger = smaller biomes.
    // Good range: 0.001f (continent-sized) to 0.010f (small patches)
    inline float biomeSize = 0.0075f;

    // Controls how sharp or gradual the transition between biomes is.
    // 1.0f = hard edge, lower = wider/smoother blend.
    // Good range: 0.05f (very gradual) to 1.0f (instant cutoff)
    inline float transitionSharpness = 0.25f;

    // How many of the nearest biomes to blend per column.
    // 1 = no blending, 2-3 = smooth transitions (3 is a good default).
    inline int blendCandidates = 3;
};

#endif // CORE_WORLD_HEADER