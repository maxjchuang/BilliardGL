cmake_minimum_required(VERSION 3.13)

project(BilliardGL LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BILLIARDGL_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/Billiards")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_AUDIO REQUIRED IMPORTED_TARGET sdl2 SDL2_mixer)

set(BILLIARDGL_CORE_SOURCES
    src/Billiards/game_state.cpp
    src/Billiards/physics.cpp
    src/Billiards/rules.cpp
    src/Billiards/input.cpp
    src/Billiards/vec.cpp
)

add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/hud.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/platform_audio.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsPhysicsTests
    tests/physics_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsRulesTests
    tests/rules_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

target_include_directories(Billiards PRIVATE
    src/Billiards
    src/Billiards/dependencies/include
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
    ${GLUT_INCLUDE_DIR}
)

target_include_directories(BilliardsPhysicsTests PRIVATE
    src/Billiards
)

target_include_directories(BilliardsRulesTests PRIVATE
    src/Billiards
)

target_compile_definitions(Billiards PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
)

target_link_libraries(Billiards PRIVATE
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    Threads::Threads
    PkgConfig::SDL2_AUDIO
    ${GLUT_LIBRARIES}
)

if(APPLE)
    target_compile_definitions(Billiards PRIVATE GL_SILENCE_DEPRECATION)
endif()
