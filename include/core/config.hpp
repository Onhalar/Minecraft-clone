#ifndef MAIN_CONFIG_HEADER
#define MAIN_CONFIG_HEADER

#include <chrono>
#include <filesystem>
#include <string>

#include <color.hpp>

// SETUP
inline int minWindowWidth = 300;
inline int minWindowHeight = 250;

inline int defaultWindowWidth = 600;
inline int defaultWindowHeight = 500;

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

#endif // MAIN_CONFIG_HEADER