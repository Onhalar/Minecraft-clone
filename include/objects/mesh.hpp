#ifndef MESH_HEADER
#define MESH_HEADER

#include <vector>
#include <glad/glad.h>

#include <stdexcept>

#include <VAO.hpp>
#include <VBO.hpp>
#include <EBO.hpp>
#include <shader.hpp>

namespace mesh {

    class Mesh {
        private:
            VAO* vao = nullptr;
            VBO* vboVertices = nullptr;
            VBO* vboColors = nullptr;
            VBO* vboUVs = nullptr;
            EBO* ebo = nullptr;

            void clearBuffers() {
                if (vao) { delete vao; vao = nullptr; }
                if (vboVertices) { delete vboVertices; vboVertices = nullptr; }
                if (vboColors) { delete vboColors; vboColors = nullptr; }
                if (vboUVs) { delete vboUVs; vboUVs = nullptr; }
                if (ebo) { delete ebo; ebo = nullptr; }
            }
        public:
            std::vector<GLfloat> vertices;
            std::vector<GLuint> indices;
            std::vector<GLfloat> UVs;

            Mesh() {}
            Mesh(const Mesh& master): vertices(master.vertices), indices(master.indices) {}
            Mesh(const std::vector<GLfloat>& vertices, const std::vector<GLuint>& indices): vertices(vertices), indices(indices) {}
            Mesh(const std::vector<GLfloat>& vertices, const std::vector<GLuint>& indices, const std::vector<GLfloat> Uvs): vertices(vertices), indices(indices), UVs(Uvs) {}
            ~Mesh() {
                clearBuffers();

                vertices.clear();
                indices.clear();
                UVs.clear();

            }

            bool empty() { return indices.empty() || vertices.empty() || UVs.empty(); }

            // merges another mesh into the current one
            void merge(const Mesh& meshToMerge) {
                indices.reserve(indices.size() + meshToMerge.indices.size()); 
                vertices.reserve(vertices.size() + meshToMerge.vertices.size());
                UVs.reserve(UVs.size() + meshToMerge.UVs.size());

                size_t oldVertexSize = vertices.size() / 3;
                size_t oldIndicesSize = indices.size();

                indices.insert(indices.end(), meshToMerge.indices.begin(), meshToMerge.indices.end());
                vertices.insert(vertices.end(), meshToMerge.vertices.begin(), meshToMerge.vertices.end());
                UVs.insert(UVs.end(), meshToMerge.UVs.begin(), meshToMerge.UVs.end());

                for (size_t i = oldIndicesSize; i < indices.size(); ++i) { indices[i] += oldVertexSize; }
            }

            void merge(const Mesh* meshToMerge, const bool& destroy = false) {
                merge(*meshToMerge);
                if (destroy) { delete meshToMerge; }
            }

        void buffer() {
            if (vao || vboVertices || vboColors || vboUVs || ebo) { clearBuffers(); }
            if (vertices.empty() || indices.empty()) {
                throw std::invalid_argument("Mesh: All mesh data must be filled.");
            }
            
            vao = new VAO();
            vao->bind();
            
            // Vertex positions (attribute 0)
            vboVertices = new VBO(vertices.data(), vertices.size() * sizeof(GLfloat));
            vao->linkAttrib(*vboVertices, 0, 3, GL_FLOAT, 3 * sizeof(GLfloat), (void*)0);
            
            // UVs (attribute 1) - only if UV data exists
            if (UVs.empty()) {
                static std::vector<GLfloat> defaultUVs = {0.0, 0.0};

                for (int i = 0; i < vertices.size() / (3); ++i) { UVs.insert(UVs.end(), defaultUVs.begin(), defaultUVs.end()); }
            }
            vboUVs = new VBO(UVs.data(), UVs.size() * sizeof(GLfloat));
            vao->linkAttrib(*vboUVs, 1, 2, GL_FLOAT, 2 * sizeof(GLfloat), (void*)0);
            
            ebo = new EBO(indices.data(), indices.size() * sizeof(GLuint));
            
            vao->unbind();
            vboVertices->unbind();
            vboUVs->unbind();
            if (vboColors) { vboColors->unbind(); }
            ebo->unbind();
        }

            void render(Shader* shader) {
                
                if (shader) { shader->activate(); }
                else { throw std::invalid_argument("Mesh: Given shader object does not exists: nullptr");}

                if (!((bool)vao & (bool)vboVertices & (bool)vboColors & (bool)ebo)) { buffer(); }

                vao->bind();
                glDrawElements(GL_TRIANGLES, indices.size(), GL_UNSIGNED_INT, 0);
                vao->unbind();
            }

            void updateData(const Mesh& master) {
                indices = master.indices;
                vertices = master.vertices;

                clearBuffers();
            }
    };

    class cubeDefaults {
    public:
        inline const static std::vector<GLfloat> vertices = {
            // Front face
            0.0f,  0.0f,  0.0f,  // 0 - front top left
            1.0f,  0.0f,  0.0f,  // 1 - front top right
            1.0f, -1.0f,  0.0f,  // 2 - front bottom right
            0.0f, -1.0f,  0.0f,  // 3 - front bottom left

            // Back face
            0.0f,  0.0f, -1.0f,  // 4 - back top left
            1.0f,  0.0f, -1.0f,  // 5 - back top right
            1.0f, -1.0f, -1.0f,  // 6 - back bottom right
            0.0f, -1.0f, -1.0f   // 7 - back bottom left
        };

        inline const static std::vector<GLuint> indices = {
            // Front face
            0, 1, 2,
            2, 3, 0,

            // Right face
            1, 5, 6,
            6, 2, 1,

            // Back face
            5, 4, 7,
            7, 6, 5,

            // Left face
            4, 0, 3,
            3, 7, 4,

            // Top face
            4, 5, 1,
            1, 0, 4,

            // Bottom face
            3, 2, 6,
            6, 7, 3
        };

        static Mesh getCube() { return Mesh(vertices, indices); }

        // +X -> Right
        // -X -> Left
        // +Y -> Back
        // -Y -> Forward
        // +Z -> Up
        // -Z -> Down
        class sides {
            public:
                inline static Mesh front = {
                    {
                        -0.5f, -0.5f, -0.5f,  // 0 - front bottom left
                        0.5f, -0.5f, -0.5f,  // 1 - front bottom right
                        0.5f, -0.5f,  0.5f,  // 2 - front top right
                        -0.5f, -0.5f,  0.5f   // 3 - front top left
                    },
                    {
                        0, 1, 2,
                        2, 3, 0
                    },
                    {
                        0.0f, 0.0f,  // 0 - bottom left
                        1.0f, 0.0f,  // 1 - bottom right
                        1.0f, 1.0f,  // 2 - top right
                        0.0f, 1.0f   // 3 - top left
                    }
                };
                
                inline static Mesh back = {
                    {
                        0.5f,  0.5f, -0.5f,  // 0 - back bottom right
                        -0.5f,  0.5f, -0.5f,  // 1 - back bottom left
                        -0.5f,  0.5f,  0.5f,  // 2 - back top left
                        0.5f,  0.5f,  0.5f   // 3 - back top right
                    },
                    {
                        0, 1, 2,
                        2, 3, 0
                    },
                    {
                        0.0f, 0.0f,  // 0 - bottom right
                        1.0f, 0.0f,  // 1 - bottom left
                        1.0f, 1.0f,  // 2 - top left
                        0.0f, 1.0f   // 3 - top right
                    }
                };
                
                inline static Mesh left = {
                    {
                        -0.5f,  0.5f, -0.5f,  // 0 - left bottom back
                        -0.5f, -0.5f, -0.5f,  // 1 - left bottom front
                        -0.5f, -0.5f,  0.5f,  // 2 - left top front
                        -0.5f,  0.5f,  0.5f   // 3 - left top back
                    },
                    {
                        0, 1, 2,
                        2, 3, 0
                    },
                    {
                        0.0f, 0.0f,  // 0 - bottom back
                        1.0f, 0.0f,  // 1 - bottom front
                        1.0f, 1.0f,  // 2 - top front
                        0.0f, 1.0f   // 3 - top back
                    }
                };
                
                inline static Mesh right = {
                    {
                        0.5f, -0.5f, -0.5f,  // 0 - right bottom front
                        0.5f,  0.5f, -0.5f,  // 1 - right bottom back
                        0.5f,  0.5f,  0.5f,  // 2 - right top back
                        0.5f, -0.5f,  0.5f   // 3 - right top front
                    },
                    {
                        0, 1, 2,
                        2, 3, 0
                    },
                    {
                        0.0f, 0.0f,  // 0 - bottom front
                        1.0f, 0.0f,  // 1 - bottom back
                        1.0f, 1.0f,  // 2 - top back
                        0.0f, 1.0f   // 3 - top front
                    }
                };
                
                inline static Mesh top = {
                    {
                        -0.5f, -0.5f,  0.5f,  // 0 - top front left
                        0.5f, -0.5f,  0.5f,  // 1 - top front right
                        0.5f,  0.5f,  0.5f,  // 2 - top back right
                        -0.5f,  0.5f,  0.5f   // 3 - top back left
                    },
                    {
                        0, 1, 2,
                        2, 3, 0
                    },
                    {
                        0.0f, 0.0f,  // 0 - front left
                        1.0f, 0.0f,  // 1 - front right
                        1.0f, 1.0f,  // 2 - back right
                        0.0f, 1.0f   // 3 - back left
                    }
                };
                
                inline static Mesh bottom = {
                    {
                        -0.5f,  0.5f, -0.5f,  // 0 - bottom back left
                        0.5f,  0.5f, -0.5f,  // 1 - bottom back right
                        0.5f, -0.5f, -0.5f,  // 2 - bottom front right
                        -0.5f, -0.5f, -0.5f   // 3 - bottom front left
                    },
                    {
                        0, 1, 2,
                        2, 3, 0
                    },
                    {
                        0.0f, 0.0f,  // 0 - back left
                        1.0f, 0.0f,  // 1 - back right
                        1.0f, 1.0f,  // 2 - front right
                        0.0f, 1.0f   // 3 - front left
                    }
                };
        };
    };
}

#endif // MESH_HEADER