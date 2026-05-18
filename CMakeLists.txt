cmake_minimum_required(VERSION 3.26.0)
project(MinecraftClone VERSION 0.1.0)

# Try to find OpenGL package
find_package(OpenGL QUIET)

add_executable(
    MinecraftClone

    # main entry point of the application
    src/main.cpp

    # dependencies
    dependencies/glad.c
)

target_compile_features(MinecraftClone PRIVATE cxx_std_20)

target_include_directories(MinecraftClone PRIVATE
    include/core
    include/OpenGL
    include/utils
    include/objects

    dependencies
)

# Set the output directory for the executable to "bin" inside the build folder
set_target_properties(MinecraftClone PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
)

# Check if OpenGL was found
if (OPENGL_FOUND)
    target_link_libraries(MinecraftClone OpenGL::GL)
else()
    message(STATUS "OpenGL not found, using opengl32.")
    target_link_libraries(MinecraftClone opengl32)
endif()

if(WIN32)
    target_link_libraries(MinecraftClone "${CMAKE_SOURCE_DIR}/dependencies/GLFW/libglfw3.a")
elseif(UNIX)
    # Check if /etc/os-release mentions Debian or Ubuntu
    execute_process(
        COMMAND cat /etc/os-release
        OUTPUT_VARIABLE OS_RELEASE_CONTENT
        OUTPUT_STRIP_TRAILING_WHITESPACE
    )

    if(OS_RELEASE_CONTENT MATCHES "ID=(debian|ubuntu)")
        message(STATUS "Detected Debian-based Linux")
        target_link_libraries(MinecraftClone "${CMAKE_SOURCE_DIR}/dependencies/GLFW/libglfw3-compiled-mint.a")
    else()
        message(WARNING "Precompiled packages not avaialbe for your system! Please make sure you have all necsery packages installed.")
        target_link_libraries(MinecraftClone glfw)
    endif()
endif()

if(WIN32)
    set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -static -static-libgcc -static-libstdc++")
endif()

#Set debug build
target_compile_definitions(MinecraftClone PRIVATE
    $<$<CONFIG:Debug>:DEBUG_ENABLED>
)

# Add to your CMakeLists.txt temporarily
# target_compile_options(MinecraftClone PRIVATE -fsanitize=address -g)
# target_link_options(MinecraftClone PRIVATE -fsanitize=address)

# Define paths to the directories
set(SOURCE_RES_DIR "${CMAKE_SOURCE_DIR}/res")
set(SOURCE_SHADERS_DIR "${CMAKE_SOURCE_DIR}/shaders")
set(BUILD_RES_DIR "${CMAKE_BINARY_DIR}/res")
set(BUILD_SHADERS_DIR "${CMAKE_BINARY_DIR}/shaders")

# Create a custom command to copy the res directory
add_custom_command(
    TARGET MinecraftClone PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SOURCE_RES_DIR} ${BUILD_RES_DIR}
)

# Create a custom command to copy the shaders directory
add_custom_command(
    TARGET MinecraftClone PRE_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
    ${SOURCE_SHADERS_DIR} ${BUILD_SHADERS_DIR}
)

# Install the main executable
install(TARGETS MinecraftClone DESTINATION bin)