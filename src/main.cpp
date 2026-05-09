#include <core.hpp>
#include <config.hpp>
#include <cstdlib>
#include <globals.hpp>
#include <random>
#include <thread>
#include <types.hpp>
#include <render.hpp>
#include <world.hpp>

#include <debug.hpp>
#include <FormatConsole.hpp>
#include <chunkGeneration.hpp>
#include <chunkWorker.hpp>

#include <chrono>
#include <thread>
#include <filesystem>

#include "camera.hpp"
#include "chunk.hpp"
#include "player.hpp"
#include "setup/setupRender.cpp"

#include "render.cpp"

void cleanup();
void mainLoop();
void createWindow();
void createWindow();

void setupOpenGL();
void setupRenderer();
void setupTextureSheet();
void setupShaderMetrices(Shader* shader);
void setupShaders();

int main(int argc, char **argv) {
    // attemps to extract current file location from call args
    if (std::filesystem::exists(argv[0])) {
        projectDir = ((std::filesystem::path)argv[0]).parent_path().parent_path();
    }

    // if fails, tries to get current working directory and hope it's correct
    else {
        projectDir = std::filesystem::current_path().parent_path();
    }

    createWindow();
    glfwSetFramebufferSizeCallback(mainWindow, resize);

    setupOpenGL();

    std::srand(static_cast<unsigned int>(std::time(nullptr)));
    WorldSettings::seed = rand();
    
    world::worldGenerator = new world::chunkGenerator();

    if (!player) { player = (currentCamera ? new world::player(currentCamera) : new world::player()); }

    mainLoop();

    cleanup();

    glfwDestroyWindow(mainWindow);

    glfwTerminate();

    return 0;
}

void createWindow() {
    GLFWwindow* window;
    GLFWimage icon;
    
    // tries to initialize GLFW, if fails => error & exit
    if (!glfwInit()) {
        std::cerr << formatError("ERROR") << ": Could not initialize GLFW" << std::endl;
        exit(-1);
    }

    // specifying which OpenGL version to use
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    
    if (debugMode) {
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);
    }

    // creating GLFW window
    window = glfwCreateWindow(defaultWindowWidth, defaultWindowHeight, windowName.c_str(), NULL, NULL);

    // sets the minimum and maximum window size
    glfwSetWindowSizeLimits(window, minWindowWidth, minWindowHeight, GLFW_DONT_CARE, GLFW_DONT_CARE);
    
    icon.pixels = stbi_load(projectPath(iconPath).c_str(), &icon.width, &icon.height, 0, 4);

    // checks whther the icon is loaded successfully
    // if yes => sets icon and clears it from memory
    // if no  => prints an error
    if (icon.pixels) {
        glfwSetWindowIcon(window, 1, &icon);
        stbi_image_free(icon.pixels);
    }
    else {
        std::cerr << formatError("ERROR") << ": Could not open icon '" << formatPath(iconPath) << "'" << std::endl;
    }

    // tell GLFW that the created window is the one to be used
    glfwMakeContextCurrent(window);

    // set global variable to the window for future manupulation
    mainWindow = window;
}

void setupOpenGL() {

    // loads in OpenGL functions from GLAD
    gladLoadGL();

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS); // default but good to specify

    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    if (debugMode) {
        glEnable(GL_DEBUG_OUTPUT);
        glEnable(GL_DEBUG_OUTPUT_SYNCHRONOUS);
        glDebugMessageCallback(MessageCallback, 0);

        std::cout << "\n" << formatRole("Info") << " " << glGetString(GL_VERSION) << "\n" << std::endl;
    }

    int width, height;

    // retrieves the size of GLFW window
    glfwGetFramebufferSize(mainWindow, &width, &height);

    // sets OpenGL viewport (plane onto which will be deawn)
    glViewport(0, 0, width, height);

    // sets background color defined in header
    glClearColor(backgroundColor.decR , backgroundColor.decG, backgroundColor.decB, backgroundColor.a);

    setupShaders();

    setupRenderer();

    setupTextureSheet();

    glfwSwapInterval(VSync);
}

void mainLoop() {
    TinyUInt frameCount = 0;
    std::chrono::nanoseconds frameDuration(1'000'000'000 / targetFrameRate); // 1,000,000 μs / 60 = 16666 μs = 16.666 m

    world::chunkWorker::workerThread = std::thread(world::chunkWorker::workerThreadFunc);

    while (!glfwWindowShouldClose(mainWindow)) {
        auto frameStart = std::chrono::steady_clock::now(); // Use std::chrono

        // handles events such as resizing and creating window
        glfwPollEvents();

        if (!isMinimized) { // Custom Actions

            // CHUNKS
            if (frameCount % 5 == 0) { // check for new chunks every 10 frames (adjust as needed)
                world::chunkWorker::checkForFarChunks(player->position);
            }
            else if (frameCount % 5 == 2) { // check for far chunks every 10 frames, offset from new chunk check (adjust as needed)
                world::chunkWorker::checkForNewChunks(player->position);
            }

            world::chunkWorker::uploadQueuedMeshes();

            player->main();
            
            render();

        }

        static std::chrono::steady_clock::time_point lastTime;

        static bool isFirstFrame = true;
        if (isFirstFrame) {
            lastTime = frameStart;
            isFirstFrame = false;
        }

        auto frameEnd = std::chrono::steady_clock::now();
        auto elapsed = duration_cast<std::chrono::nanoseconds>(frameEnd - frameStart);

        if (elapsed < frameDuration && !VSync) {
            std::this_thread::sleep_for((frameDuration - elapsed) * staticDelayFraction);

            // spin delay for frames
            if (staticDelayFraction < 1.0f) {
                while (true)
                {
                    std::this_thread::sleep_for(spinDelay);
                    if (std::chrono::steady_clock::now() >= frameStart + frameDuration) { break; }
                }
            }
        }

        ++frameCount; // does not need to be checked unsigned types wrap around.

        frameEnd = std::chrono::steady_clock::now();

        deltaTime = duration_cast<std::chrono::nanoseconds>(frameEnd - lastTime).count() / 1'000'000'000.0;
            
        lastTime = frameEnd;
    }
}

void cleanup() {

    if (player) { delete player; }

    world::chunkWorker::shouldTerminate = true;
    {
        //std::lock_guard<std::mutex> lock(world::chunkWorker::workerMutex);
        world::chunkWorker::clearAssignedWork();
    }

    if (world::chunkWorker::workerThread.joinable()) { 
        world::chunkWorker::workerThread.join(); 
    }
    
    if (mainTextureAtlas) { delete mainTextureAtlas; mainTextureAtlas = nullptr; }

    for (auto& [key, shader] : Shaders) { if (shader) { delete shader; shader = nullptr; } }
    
    if (world::worldGenerator) { delete world::worldGenerator; world::worldGenerator = nullptr; }

    world::chunkRegistry::deregisterAll();

    if (chunkRenderer) { delete chunkRenderer; chunkRenderer = nullptr; }

    for (auto camera : Cameras) { if (camera) { delete camera; } }
}