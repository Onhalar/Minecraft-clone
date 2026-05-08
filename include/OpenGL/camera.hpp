# ifndef CAMERA_CLASS_HEADER
# define CAMERA_CLASS_HEADER

// for delta time
#include <globals.hpp>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#define GLM_ENABLE_EXPERIMENTAL
#include <glm/gtx/rotate_vector.hpp>
#include <glm/gtx/vector_angle.hpp>


#include <shader.hpp>

class Camera {
    public:
        glm::mat4 projectionMatrix = glm::mat4(1.0f);
        glm::mat4 viewMatrix = glm::mat4(1.0f);

        glm::vec3 position;
        glm::vec3 orientation = glm::vec3(0.0f, 1.0f, 0.0f);
        const glm::vec3 UP = glm::vec3(0.0f, 0.0f, 1.0f);

        float yaw = 90.0f;
        float pitch = 0.0f;

        float cameraSpeed = 55.0f;
        float sensitivity = 175.0f;

        float nearClipPlane = 0.1f;
        float farClipPlane = 10'000.0f;

        int width, height;

        float FOVdeg = 85.0f;

        Camera(int width, int height, glm::vec3 position) {
            this->position = position;
            this->width = width;
            this->height = height;
            updateOrientation();
        }

    private:
        void updateOrientation() {
            orientation = glm::normalize(glm::vec3(
                cos(glm::radians(yaw)) * cos(glm::radians(pitch)),
                sin(glm::radians(yaw)) * cos(glm::radians(pitch)),
                sin(glm::radians(pitch))
            ));
        }

    public:
        void handleInputs(GLFWwindow* window) {
            static bool controlCamera = false;

            if (glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) && !controlCamera) {
                controlCamera = true;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
                glfwSetCursorPos(window, (width / 2), (height / 2));
            }
            else if (glfwGetKey(window, GLFW_KEY_ESCAPE) && controlCamera) {
                controlCamera = false;
                glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
            }

            if (controlCamera) {
                double mouseX, mouseY;
                glfwGetCursorPos(window, &mouseX, &mouseY);

                float rotX = sensitivity * (float)(mouseY - (height / 2)) / height;
                float rotY = sensitivity * (float)(mouseX - (width / 2)) / width;

                pitch -= rotX;
                yaw   -= rotY;

                pitch = glm::clamp(pitch, -89.0f, 89.0f);
                updateOrientation();

                glfwSetCursorPos(window, (width / 2), (height / 2));

                float FdeltaTime = (float)deltaTime;

                glm::vec3 forward = glm::normalize(glm::vec3(orientation.x, orientation.y, 0.0f));
                glm::vec3 right = glm::normalize(glm::cross(forward, UP));

                if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) {
                    position += forward * cameraSpeed * FdeltaTime;
                }
                if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
                    position -= forward * cameraSpeed * FdeltaTime;
                }
                if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
                    position -= right * cameraSpeed * FdeltaTime;
                }
                if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) {
                    position += right * cameraSpeed * FdeltaTime;
                }

                if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) {
                    position += UP * cameraSpeed * FdeltaTime;
                }
                if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) {
                    position -= UP * cameraSpeed * FdeltaTime;
                }
            }
        }

        void updateProjection(Shader* shader) {
            shader->activate();

            viewMatrix = glm::lookAt(position, position + orientation, UP);
            projectionMatrix = glm::perspective(glm::radians(FOVdeg), width/(float)height, nearClipPlane, farClipPlane);

            shader->viewMatrix = viewMatrix;
            shader->projectionMatrix = projectionMatrix;

            shader->applyViewMatrix();
            shader->applyProjectionMatrix();
        }
        
        void updateProjection(int projectionWidth, int projectionHeight, Shader* shader) {
            shader->activate();
            
            width = projectionWidth;
            height = projectionHeight;

            glViewport(0, 0, projectionWidth, projectionHeight);

            viewMatrix = glm::lookAt(position, position + orientation, UP);
            projectionMatrix = glm::perspective(glm::radians(FOVdeg), width/(float)height, nearClipPlane, farClipPlane);

            shader->viewMatrix = viewMatrix;
            shader->projectionMatrix = projectionMatrix;

            shader->applyViewMatrix();
            shader->applyProjectionMatrix();
        }

        void updateProjection(const int& projectionWidth, const int& projectionHeight, Shader* shader, const float& nearClipPlane, const float& farClipPlane) {
            shader->activate();
            
            width = projectionWidth;
            height = projectionHeight;

            glViewport(0, 0, projectionWidth, projectionHeight);

            viewMatrix = glm::lookAt(position, position + orientation, UP);
            projectionMatrix = glm::perspective(glm::radians(FOVdeg), width/(float)height, nearClipPlane, farClipPlane);

            shader->viewMatrix = viewMatrix;
            shader->projectionMatrix = projectionMatrix;

            shader->applyViewMatrix();
            shader->applyProjectionMatrix();

            this->nearClipPlane = nearClipPlane;
            this->farClipPlane = farClipPlane;
        }

        void updateCameraValues(const float& renderDistance, const float& sensitivity, const float& speed, const float& fovDeg) {
            this->farClipPlane = renderDistance; this->sensitivity = sensitivity, this->cameraSpeed = speed; this->FOVdeg = fovDeg;
        }
};

#endif
