#ifndef MESH_HEADER
#define MESH_HEADER

#include <cstdint>
#include <vector>
#include <unordered_map>
#include <deque>
#include <stdexcept>
#include <mutex>

#include <glad/glad.h>
#include <glm/glm.hpp>

#include <shader.hpp>

namespace mesh {

    struct meshAllocation {
        GLint    baseVertex  = 0;
        GLuint   firstIndex  = 0;
        GLuint   indexCount  = 0;
        GLuint   vertexCount = 0;
        bool     valid       = false;
    };

    struct DrawCommand {
        GLuint count;          // index count
        GLuint instanceCount;  // always 1
        GLuint firstIndex;
        GLint  baseVertex;
        GLuint baseInstance;   // always 0
    };

    struct meshData {
        uint32_t meshID = 0u;

        std::vector<GLfloat> vertices;
        std::vector<GLuint>  indices;
        std::vector<GLfloat> uvs;

        meshData() {}
        meshData(std::vector<GLfloat> vertices, std::vector<GLuint> indices, std::vector<GLfloat> uvs): vertices(vertices), indices(indices), uvs(uvs) {};

        bool empty() const { return indices.empty() || vertices.empty(); }

        void merge(const meshData* const other) {
            size_t baseVertex = vertices.size() / 3;
            size_t baseIndex  = indices.size();

            vertices.insert(vertices.end(), other->vertices.begin(), other->vertices.end());
            uvs.insert(uvs.end(), other->uvs.begin(), other->uvs.end());
            indices.insert(indices.end(), other->indices.begin(), other->indices.end());

            for (size_t i = baseIndex; i < indices.size(); ++i)
                indices[i] += baseVertex;
        }

        void clear() { vertices.clear(); uvs.clear(); indices.clear(); }
    };

    class MassRenderer {
        public:
            static constexpr size_t MAX_VERTICES = 16'000'000;
            static constexpr size_t MAX_INDICES  = 32'000'000;
            static constexpr size_t MAX_CHUNKS   = 8192;
            
            static inline std::mutex rendererMutex;

            MassRenderer() { init(); }
            ~MassRenderer() { destroy(); }

            // Upload a mesh into the shared buffer.
            // Returns an ID used to refer to this mesh for rendering/freeing.
            // Call from main thread only.
            uint32_t upload(const meshData& data);

            // Release a previously uploaded mesh's buffer space.
            void free(uint32_t id);

            // Issue the single indirect draw call for all visible meshes.
            void render(Shader* shader, const std::vector<uint32_t>& visibleIDs);

        private:
            void init();
            void destroy();

            GLuint vao_         = 0;
            GLuint vbo_         = 0;  // shared vertex positions buffer
            GLuint ubo_         = 0;  // shared UV buffer
            GLuint ebo_         = 0;  // shared index buffer
            GLuint indirectBuf_ = 0;  // GL_DRAW_INDIRECT_BUFFER

            struct FreeBlock { size_t offset, size; };
            std::deque<FreeBlock>  freeVertexBlocks_;
            std::deque<FreeBlock>  freeIndexBlocks_;

            std::unordered_map<uint32_t, meshAllocation> allocations_;

            uint32_t nextID_ = 1;

            size_t allocVertices(size_t count);
            size_t allocIndices(size_t count);
            void   freeVertexBlock(size_t offset, size_t size);
            void   freeIndexBlock(size_t offset, size_t size);
    };


    // -------------------------------------------------------------------------
    // defaults
    // -------------------------------------------------------------------------

    class defaults {
        public:
        class Cube {
            public:                
                inline static meshData front = {
                    {
                        1.0f, 1.0f, 0.0f,  // 0 - front bottom right
                        0.0f, 1.0f, 0.0f,  // 1 - front bottom left
                        0.0f, 1.0f, 1.0f,  // 2 - front top left
                        1.0f, 1.0f, 1.0f   // 3 - front top right
                    },
                    { 0, 1, 2, 2, 3, 0 },
                    {
                        0.0f, 0.0f,
                        1.0f, 0.0f,
                        1.0f, 1.0f,
                        0.0f, 1.0f
                    }
                };

                inline static meshData back = {
                    {
                        0.0f, 0.0f, 0.0f,  // 0 - back bottom left
                        1.0f, 0.0f, 0.0f,  // 1 - back bottom right
                        1.0f, 0.0f, 1.0f,  // 2 - back top right
                        0.0f, 0.0f, 1.0f   // 3 - back top left
                    },
                    { 0, 1, 2, 2, 3, 0 },
                    {
                        0.0f, 0.0f,
                        1.0f, 0.0f,
                        1.0f, 1.0f,
                        0.0f, 1.0f
                    }
                };

                inline static meshData left = {
                    {
                        0.0f, 1.0f, 0.0f,  // 0 - left bottom back
                        0.0f, 0.0f, 0.0f,  // 1 - left bottom front
                        0.0f, 0.0f, 1.0f,  // 2 - left top front
                        0.0f, 1.0f, 1.0f   // 3 - left top back
                    },
                    { 0, 1, 2, 2, 3, 0 },
                    {
                        0.0f, 0.0f,
                        1.0f, 0.0f,
                        1.0f, 1.0f,
                        0.0f, 1.0f
                    }
                };

                inline static meshData right = {
                    {
                        1.0f, 0.0f, 0.0f,  // 0 - right bottom front
                        1.0f, 1.0f, 0.0f,  // 1 - right bottom back
                        1.0f, 1.0f, 1.0f,  // 2 - right top back
                        1.0f, 0.0f, 1.0f   // 3 - right top front
                    },
                    { 0, 1, 2, 2, 3, 0 },
                    {
                        0.0f, 0.0f,
                        1.0f, 0.0f,
                        1.0f, 1.0f,
                        0.0f, 1.0f
                    }
                };

                inline static meshData top = {
                    {
                        0.0f, 0.0f, 1.0f,  // 0 - top front left
                        1.0f, 0.0f, 1.0f,  // 1 - top front right
                        1.0f, 1.0f, 1.0f,  // 2 - top back right
                        0.0f, 1.0f, 1.0f   // 3 - top back left
                    },
                    { 0, 1, 2, 2, 3, 0 },
                    {
                        0.0f, 0.0f,
                        1.0f, 0.0f,
                        1.0f, 1.0f,
                        0.0f, 1.0f
                    }
                };

                inline static meshData bottom = {
                    {
                        0.0f, 1.0f, 0.0f,  // 0 - bottom back left
                        1.0f, 1.0f, 0.0f,  // 1 - bottom back right
                        1.0f, 0.0f, 0.0f,  // 2 - bottom front right
                        0.0f, 0.0f, 0.0f   // 3 - bottom front left
                    },
                    { 0, 1, 2, 2, 3, 0 },
                    {
                        0.0f, 0.0f,
                        1.0f, 0.0f,
                        1.0f, 1.0f,
                        0.0f, 1.0f
                    }
                };
        };
    };


    // -------------------------------------------------------------------------
    // MassRenderer implementation
    // -------------------------------------------------------------------------

    inline void MassRenderer::init() {
        // VAO
        glGenVertexArrays(1, &vao_);
        glBindVertexArray(vao_);

        // Vertex positions (attribute 0)
        glGenBuffers(1, &vbo_);
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * 3 * sizeof(GLfloat), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(0);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (void*)0);

        // UVs (attribute 1) — separate buffer, same VAO
        glGenBuffers(1, &ubo_);
        glBindBuffer(GL_ARRAY_BUFFER, ubo_);
        glBufferData(GL_ARRAY_BUFFER, MAX_VERTICES * 2 * sizeof(GLfloat), nullptr, GL_DYNAMIC_DRAW);
        glEnableVertexAttribArray(1);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 2 * sizeof(GLfloat), (void*)0);

        // Index buffer
        glGenBuffers(1, &ebo_);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, MAX_INDICES * sizeof(GLuint), nullptr, GL_DYNAMIC_DRAW);

        glBindVertexArray(0);

        // Indirect draw buffer (not part of VAO state)
        glGenBuffers(1, &indirectBuf_);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuf_);
        glBufferData(GL_DRAW_INDIRECT_BUFFER, MAX_CHUNKS * sizeof(DrawCommand), nullptr, GL_DYNAMIC_DRAW);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);

        // Seed free lists
        freeVertexBlocks_.push_back({0, MAX_VERTICES});
        freeIndexBlocks_.push_back({0, MAX_INDICES});
    }

    inline void MassRenderer::destroy() {
        glDeleteVertexArrays(1, &vao_);
        glDeleteBuffers(1, &vbo_);
        glDeleteBuffers(1, &ubo_);
        glDeleteBuffers(1, &ebo_);
        glDeleteBuffers(1, &indirectBuf_);
    }

    inline size_t MassRenderer::allocVertices(size_t count) {
        for (auto it = freeVertexBlocks_.begin(); it != freeVertexBlocks_.end(); ++it) {
            if (it->size >= count) {
                size_t offset = it->offset;
                it->offset += count;
                it->size   -= count;
                if (it->size == 0) freeVertexBlocks_.erase(it);
                return offset;
            }
        }
        throw std::runtime_error("MassRenderer: vertex buffer full");
    }

    inline size_t MassRenderer::allocIndices(size_t count) {
        for (auto it = freeIndexBlocks_.begin(); it != freeIndexBlocks_.end(); ++it) {
            if (it->size >= count) {
                size_t offset = it->offset;
                it->offset += count;
                it->size   -= count;
                if (it->size == 0) freeIndexBlocks_.erase(it);
                return offset;
            }
        }
        throw std::runtime_error("MassRenderer: index buffer full");
    }

    inline void MassRenderer::freeVertexBlock(size_t offset, size_t size) {
        freeVertexBlocks_.push_back({offset, size});  // append first

        // Sort — all iterators obtained AFTER this point are fresh
        std::sort(freeVertexBlocks_.begin(), freeVertexBlocks_.end(),
            [](const FreeBlock& a, const FreeBlock& b){ return a.offset < b.offset; });

        // Merge — obtain iterators only after the sort
        for (auto it = freeVertexBlocks_.begin(); it != freeVertexBlocks_.end(); ) {
            auto next = std::next(it);
            if (next == freeVertexBlocks_.end()) break;
            if (it->offset + it->size >= next->offset) {
                it->size = std::max(it->offset + it->size, next->offset + next->size) - it->offset;
                it = freeVertexBlocks_.erase(next);  // ← reassign 'it' from erase's return value
                // DON'T advance 'it' — check again from same position to catch chains of merges
            } else {
                ++it;
            }
        }
    }

    inline void MassRenderer::freeIndexBlock(size_t offset, size_t size) {
        freeIndexBlocks_.push_back({offset, size});

        std::sort(freeIndexBlocks_.begin(), freeIndexBlocks_.end(),
            [](const FreeBlock& a, const FreeBlock& b){ return a.offset < b.offset; });

        for (auto it = freeIndexBlocks_.begin(); it != freeIndexBlocks_.end(); ) {
            auto next = std::next(it);
            if (next == freeIndexBlocks_.end()) break;
            if (it->offset + it->size >= next->offset) {
                it->size = std::max(it->offset + it->size, next->offset + next->size) - it->offset;
                it = freeIndexBlocks_.erase(next);  // ← fix: reassign 'it', same as freeVertexBlock
            } else {
                ++it;
            }
        }
    }

    inline uint32_t MassRenderer::upload(const meshData& data) {
        std::lock_guard<std::mutex> lock(rendererMutex);

        size_t vertCount = data.vertices.size() / 3;
        size_t idxCount  = data.indices.size();

        size_t vertOffset = allocVertices(vertCount);
        size_t idxOffset  = allocIndices(idxCount);

        // Upload vertex positions into shared VBO
        glBindBuffer(GL_ARRAY_BUFFER, vbo_);
        glBufferSubData(GL_ARRAY_BUFFER,
            vertOffset * 3 * sizeof(GLfloat),
            data.vertices.size() * sizeof(GLfloat),
            data.vertices.data());

        // Upload UVs into shared UV buffer
        glBindBuffer(GL_ARRAY_BUFFER, ubo_);
        glBufferSubData(GL_ARRAY_BUFFER,
            vertOffset * 2 * sizeof(GLfloat),
            data.uvs.size() * sizeof(GLfloat),
            data.uvs.data());

        // Upload indices into shared EBO
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo_);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
            idxOffset * sizeof(GLuint),
            data.indices.size() * sizeof(GLuint),
            data.indices.data());

        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, 0);

        uint32_t id = nextID_++;
        allocations_[id] = {
            (GLint)vertOffset,
            (GLuint)idxOffset,
            (GLuint)idxCount,
            (GLuint)vertCount,
            true
        };

        return id;
    }

    inline void MassRenderer::free(uint32_t id) {
        std::lock_guard<std::mutex> lock(rendererMutex);
        auto it = allocations_.find(id);
        if (it == allocations_.end()) return;

        const auto& alloc = it->second;
        freeVertexBlock(alloc.baseVertex, alloc.vertexCount);
        freeIndexBlock(alloc.firstIndex, alloc.indexCount);
        allocations_.erase(it);
    }

    inline void MassRenderer::render(Shader* shader, const std::vector<uint32_t>& visibleIDs) {
        if (visibleIDs.empty()) return;

        shader->activate();

        // Build indirect command list from visible IDs
        std::vector<DrawCommand> commands;
        commands.reserve(visibleIDs.size());

        for (uint32_t id : visibleIDs) {
            auto it = allocations_.find(id);
            if (it == allocations_.end()) continue;

            const auto& a = it->second;
            commands.push_back({
                a.indexCount,
                1,
                a.firstIndex,
                a.baseVertex,
                0   // baseInstance — unused, always 0
            });
        }

        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, indirectBuf_);
        glBufferSubData(GL_DRAW_INDIRECT_BUFFER, 0,
            commands.size() * sizeof(DrawCommand),
            commands.data());

        glBindVertexArray(vao_);
        glMultiDrawElementsIndirect(GL_TRIANGLES, GL_UNSIGNED_INT,
                                    nullptr, (GLsizei)commands.size(), 0);
        glBindVertexArray(0);
        glBindBuffer(GL_DRAW_INDIRECT_BUFFER, 0);
    }

}

#endif // MESH_HEADER