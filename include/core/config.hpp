#ifndef MAIN_CONFIG_HEADER
#define MAIN_CONFIG_HEADER

#include <chrono>
#include <filesystem>
#include <string>

#include <color.hpp>

// SETUP
inline int minWindowWidth = 300;
inline int minWindowHeight = 250;

inline int defaultWindowWidth = 1024;
inline int defaultWindowHeight = 720;

inline unsigned short blockTextureWidth = 16; // px - pixel widht of a block in texture atlas

inline std::string windowName = "Minecraft Clone";

inline std::filesystem::path iconPath("res/img/icon.png");

inline Color backgroundColor("#1a2d3f");

inline std::filesystem::path textureSheetPath = "res/img/atlas.png";

// RENDER
inline int VSync = 1;
inline int targetFrameRate = 60;
inline float staticDelayFraction = 0.65f;
inline std::chrono::nanoseconds spinDelay(375);

inline int renderDistance = 8; // in chunks

struct WorldSettings {
    inline static unsigned int seed = 0u;

    // Controls biome territory size.
    // Smaller = larger biomes, larger = smaller biomes.
    // Good range: 0.001f (continent-sized) to 0.010f (small patches)
    inline static float biomeSize = 0.0075f;

    // Controls how sharp or gradual the transition between biomes is.
    // 1.0f = hard edge, lower = wider/smoother blend.
    // Good range: 0.05f (very gradual) to 1.0f (instant cutoff)
    inline static float transitionSharpness = 0.25f;

    // How many of the nearest biomes to blend per column.
    // 1 = no blending, 2-3 = smooth transitions (3 is a good default).
    inline static int blendCandidates = 3;
};

#endif // MAIN_CONFIG_HEADER