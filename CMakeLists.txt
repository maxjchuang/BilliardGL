cmake_minimum_required(VERSION 3.13)

project(BilliardGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BILLIARDGL_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/Billiards")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)

add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/vec.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/platform_audio.cpp
)

target_include_directories(Billiards PRIVATE
    src/Billiards
    src/Billiards/dependencies/include
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
    ${GLUT_INCLUDE_DIR}
)

target_compile_definitions(Billiards PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
)

target_link_libraries(Billiards PRIVATE
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    ${GLUT_LIBRARIES}
)

if(APPLE)
    target_compile_definitions(Billiards PRIVATE GL_SILENCE_DEPRECATION)
endif()

if(WIN32)
    target_link_libraries(Billiards PRIVATE winmm)
endif()
