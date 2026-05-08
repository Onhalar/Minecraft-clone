#ifndef CHUNK_GENERATION_HEADER
#define CHUNK_GENERATION_HEADER

#include "glm/fwd.hpp"
#include <config.hpp>

#include <chunk.hpp>
#include <block.hpp>
#include <biome.hpp>

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
                int h = hash & 3;
                float u = h < 2 ? x : y;
                float v = h < 2 ? y : x;
                return ((h & 1) ? -u : u) + ((h & 2) ? -v : v);
            }
    };

    // ---------------------------------------------------------------------------
    // Biome blending helpers
    // ---------------------------------------------------------------------------

    // A biome paired with its interpolation weight for a given world column
    struct BiomeWeight {
        const biome* b;
        float weight;
    };

    // ---------------------------------------------------------------------------
    // Chunk generator
    // ---------------------------------------------------------------------------
    class chunkGenerator {
        WorldSettings   settings;
        PerlinNoise     noise;            // terrain shape noise
        PerlinNoise     temperatureNoise; // drives biome climate X axis
        PerlinNoise     humidityNoise;    // drives biome climate Y axis

        public:
            chunkGenerator(WorldSettings settings = {})
                : settings(settings)
                , noise(settings.seed)
                , temperatureNoise(settings.seed + 1)
                , humidityNoise   (settings.seed + 2)
            {}

            // Generate and register a chunk at the given chunk-space position.
            // Biome selection and blending are resolved internally per column.
            // Returns nullptr if the chunk is already registered.
            Chunk* generate(glm::ivec2 chunkPosition, const bool registerChunk = true) {
                if (chunkRegistry::isChunkRegistered(chunkPosition)) { return nullptr; }

                Chunk* chunk = new Chunk(glm::vec2(chunkPosition), registerChunk);

                for (unsigned char x = 0; x < CHUNK_WIDTH; ++x) {
                    for (unsigned char y = 0; y < CHUNK_WIDTH; ++y) {

                        // World-space XY for this column
                        float wx = chunkPosition.x * CHUNK_WIDTH + x;
                        float wy = chunkPosition.y * CHUNK_WIDTH + y;

                        auto weights = getBiomeWeights(wx, wy);
                        int surfaceZ = std::clamp(getBlendedSurfaceHeight(wx, wy, weights), 1, CHUNK_HEIGHT - 1);
                        fillBlendedColumn(*chunk, x, y, surfaceZ, weights);
                    }
                }

                chunkRegistry::registerChunk(chunk, chunkPosition);
                return chunk;
            }

        private:

            // -----------------------------------------------------------------------
            // Climate sampling
            // -----------------------------------------------------------------------

            // Sample the climate (temperature, humidity) at a world-space column.
            // Both values are in [-1, 1].
            void sampleClimate(float wx, float wy, float& outTemp, float& outHum) const {
                outTemp = temperatureNoise.noise(wx * settings.biomeSize, wy * settings.biomeSize);
                outHum  = humidityNoise   .noise(wx * settings.biomeSize, wy * settings.biomeSize);
            }

            // -----------------------------------------------------------------------
            // Biome weighting
            // -----------------------------------------------------------------------

            // Returns the blendCandidates nearest biomes in climate space, with
            // sharpness-adjusted inverse-distance weights that sum to 1.0.
            // The vector is sorted descending by weight (dominant biome first).
            std::vector<BiomeWeight> getBiomeWeights(float wx, float wy) const {
                float temp, hum;
                sampleClimate(wx, wy, temp, hum);

                // Compute squared climate-space distance to every registered biome
                std::vector<std::pair<float, const biome*>> dists;
                dists.reserve(biomeRegistry.size());
                for (const auto& [name, b] : biomeRegistry) {
                    float dt = b.temperature - temp;
                    float dh = b.humidity    - hum;
                    dists.push_back({ dt * dt + dh * dh, &b });
                }

                // Keep only the N closest candidates
                std::sort(dists.begin(), dists.end());
                if ((int)dists.size() > settings.blendCandidates) {
                    dists.resize(settings.blendCandidates);
                }

                // Sharpness-adjusted inverse-distance weighting.
                // Raising to 1/sharpness pushes weight toward the closest biome
                // as sharpness approaches 1.0, and spreads it evenly as it
                // approaches 0.0, widening the transition zone.
                const float exponent = 1.0f / std::max(settings.transitionSharpness, 1e-4f);
                std::vector<BiomeWeight> result;
                result.reserve(dists.size());
                float totalW = 0.0f;
                for (auto& [d, b] : dists) {
                    float w = std::pow(1.0f / (d + 1e-6f), exponent);
                    result.push_back({ b, w });
                    totalW += w;
                }
                // Normalize so weights sum to 1
                for (auto& bw : result) { bw.weight /= totalW; }

                // Sort descending so result[0] is always the dominant biome
                std::sort(result.begin(), result.end(),
                    [](const BiomeWeight& a, const BiomeWeight& b){ return a.weight > b.weight; });

                return result;
            }

            // -----------------------------------------------------------------------
            // Height generation
            // -----------------------------------------------------------------------

            // Blend the surface height across the candidate biomes using their weights.
            // Each biome contributes its own fbm sample scaled by its own parameters,
            // so noise character (roughness, scale) also transitions smoothly.
            int getBlendedSurfaceHeight(float wx, float wy,
                                        const std::vector<BiomeWeight>& weights) const {
                float blended = 0.0f;
                for (const auto& [b, w] : weights) {
                    float n = noise.fbm(
                        wx * b->scale,
                        wy * b->scale,
                        b->octaves,
                        b->persistence,
                        b->lacunarity
                    ); // n in [-1, 1]
                    blended += w * (b->baseHeight + n * b->heightRange);
                }
                return static_cast<int>(blended);
            }

            // -----------------------------------------------------------------------
            // Column fill
            // -----------------------------------------------------------------------

            // Fill a single XY column from z=0 up to surfaceZ.
            // The dominant biome (highest weight) determines which blocks to place.
            // Height is already blended above; blending block types themselves
            // would produce ugly checkerboard patterns at boundaries.
            void fillBlendedColumn(Chunk& chunk,
                                   unsigned char x, unsigned char y,
                                   int surfaceZ,
                                   const std::vector<BiomeWeight>& weights) const {
                const biome* dominant = weights[0].b; // highest weight, sorted above

                for (int z = 0; z <= surfaceZ && z < CHUNK_HEIGHT; ++z) {
                    blockID ID;

                    if (z == surfaceZ) {
                        ID = dominant->surfaceBlock;               // top: grass / sand / snow …
                    } else if (z >= surfaceZ - dominant->dirtDepth) {
                        ID = dominant->topLayerBlock;              // subsurface: dirt / sand …
                    } else {
                        ID = dominant->deepLayerBlock;             // deep: stone …
                    }

                    chunk.getBlock(x, y, z) = Block(ID, BlockType::solid);
                }
            }
    };

    inline world::chunkGenerator* worldGenerator = nullptr;

} // namespace world


#endif // CHUNK_GENERATION_HEADER