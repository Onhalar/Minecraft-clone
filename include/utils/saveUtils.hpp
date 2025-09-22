#ifndef SAVE_UTILS_HEADER
#define SAVE_UTILS_HEADER

#include <types.hpp>
#include <vector>
#include <chunk.hpp>

class flatten {
    private:

    public:
        std::vector<Block> dense(BlockData data) {
            std::vector<Block> flat;
            flat.reserve(CHUNK_WIDTH * CHUNK_WIDTH * CHUNK_HEIGHT);

            for (unsigned char x = 0; x < CHUNK_WIDTH; ++x) {
                for (unsigned char y = 0; y < CHUNK_WIDTH; ++y) {
                    for (unsigned char z = 0; z < CHUNK_HEIGHT; ++z) {
                        flat.push_back(data[x][y][z]);
                    }
                }
            }

            return flat;
        }

        //std::vector<std::pair<std::array<unsigned char, 3>, Block>> sparse(BlockData blockData) {}
};

class unflatten {
    public:

};

#endif // SAVE_UTILS_HEADER