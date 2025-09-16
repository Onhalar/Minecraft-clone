#ifndef DEBUG_HEADER
#define DEBUG_HEADER

#ifdef DEBUG_ENABLED
    inline bool debugMode = true;
#else
    inline bool debugMode = false;
#endif

inline bool prettyOutput = true;

#include <glm/glm.hpp>
#include <iostream>

inline void printVec3(const char* name, glm::vec3 in) {
    std::cout << name << ": " << in.x << ", " << in.y << ", " << in.z << std::endl;
}
inline void printVec3(const char* name, glm::dvec3 in) {
    std::cout << name << ": " << in.x << ", " << in.y << ", " << in.z << std::endl;
}

#endif // DEBUG_HEADER