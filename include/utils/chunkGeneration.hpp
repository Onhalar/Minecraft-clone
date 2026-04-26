#ifndef CHUNK_GENERATION_HEADER
#define CHUNK_GENERATION_HEADER

#include "glm/fwd.hpp"
#include <config.hpp>

#include <chunk.hpp>
#include <block.hpp>
#include <biom.hpp>

#include <cmath>
#include <numeric>
#include <algorithm>
#include <random>

namespace world {

    // ---------------------------------------------------------------------------
    // Minimal self-contained Perlin noise (no external dep needed)
    // ---------------------------------------------------------------------------
    class PerlinNoise {
        std::vector<int> p;
        public:
            PerlinNoise(unsigned int seed = 0) {
                p.resize(256);
                std::iota(p.begin(), p.end(), 0);
                std::default_random_engine engine(seed);
                std::shuffle(p.begin(), p.end(), engine);
                p.insert(p.end(), p.begin(), p.end()); // duplicate
            }

            float noise(float x, float y) const {
                int X = (int)std::floor(x) & 255;
                int Y = (int)std::floor(y) & 255;
                x -= std::floor(x);
                y -= std::floor(y);
                float u = fade(x), v = fade(y);
                int a = p[X] + Y, b = p[X+1] + Y;
                return lerp(v,
                    lerp(u, grad(p[a],   x,   y),   grad(p[b],   x-1, y)),
                    lerp(u, grad(p[a+1], x,   y-1), grad(p[b+1], x-1, y-1))
                );
            }

            // Fractal Brownian Motion - layers multiple octaves for natural terrain
            float fbm(float x, float y, int octaves, float persistence, float lacunarity) const {
                float value = 0.0f, amplitude = 1.0f, frequency = 1.0f, maxValue = 0.0f;
                for (int i = 0; i < octaves; ++i) {
                    value    += noise(x * frequency, y * frequency) * amplitude;
                    maxValue += amplitude;
                    amplitude *= persistence;
                    frequency *= lacunarity;
                }
                return value / maxValue; // normalize to [-1, 1]
            }

        private:
            static float fade(float t) { return t * t * t * (t * (t * 6 - 15) + 10); }
            static float lerp(float t, float a, float b) { return a + t * (b - a); }
            static float grad(int hash, float x, float y) {
                // 2D gradient
                int h = hash & 3;
                float u = h < 2 ? x : y;
                float v = h < 2 ? y : x;
                return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
            }
    };

    // ---------------------------------------------------------------------------
    // Chunk generator
    // ---------------------------------------------------------------------------
    class chunkGenerator {
        PerlinNoise     noise;

        public:
            // Seed defaults to 0; pass a real seed for varied worlds
            chunkGenerator(unsigned int seed = worldSeed)
                : noise(seed) {}

            // Generate and register a chunk at the given chunk-space position.
            // Returns nullptr if the chunk is already registered.
            Chunk* generate(glm::ivec2 chunkPosition, biom* biom) {
                if (chunkRegistry::isChunkRegistered(chunkPosition)) { return nullptr; }

                Chunk* chunk = new Chunk(glm::vec2(chunkPosition));

                for (unsigned char x = 0; x < CHUNK_WIDTH; ++x) {
                    for (unsigned char y = 0; y < CHUNK_WIDTH; ++y) {

                        // World-space XY for this column
                        float wx = chunkPosition.x * CHUNK_WIDTH + x;
                        float wy = chunkPosition.y * CHUNK_WIDTH + y;

                        int surfaceZ = getSurfaceHeight(wx, wy, biom);
                        surfaceZ = std::clamp(surfaceZ, 1, CHUNK_HEIGHT - 1);

                        fillColumn(*chunk, x, y, surfaceZ, biom);
                    }
                }

                chunkRegistry::registerChunk(chunk, chunkPosition);
                return chunk;
            }

        private:
            // Returns the surface block Z for a given world XY column
            int getSurfaceHeight(float wx, float wy, biom* biom) const {
                float n = noise.fbm(
                    wx * biom->scale,
                    wy * biom->scale,
                    biom->octaves,
                    biom->persistence,
                    biom->lacunarity
                ); // n in [-1, 1]

                // Map to [baseHeight - heightRange, baseHeight + heightRange]
                return biom->baseHeight + static_cast<int>(n * biom->heightRange);
            }

            // Fill a single XY column from z=0 up to surfaceZ
            void fillColumn(Chunk& chunk, unsigned char x, unsigned char y, int surfaceZ, biom* biom) const {
                for (int z = 0; z <= surfaceZ && z < CHUNK_HEIGHT; ++z) {
                    BlockRef* blockData;

                    if (z == surfaceZ) {
                        blockData = biom->surfaceBlock;             // top layer: grass
                    } else if (z >= surfaceZ - biom->dirtDepth) {
                        blockData = biom->topLayerBlock;            // subsurface: dirt
                    } else {
                        blockData = biom->deepLayerBlock;            // deep: dirt (swap for stone when you add it)
                    }

                    chunk.getBlock(x, y, z) = Block(blockData, BlockType::solid);
                }
            }
    };

} // namespace world

#endif // CHUNK_GENERATION_HEADER