cmake_minimum_required(VERSION 3.13)

project(BilliardGL LANGUAGES CXX)

enable_testing()

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
    src/Billiards/assets.cpp
    src/Billiards/game_state.cpp
    src/Billiards/image_loader.cpp
    src/Billiards/physics.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/rules.cpp
    src/Billiards/input.cpp
    src/Billiards/vec.cpp
)

add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/hud.cpp
    src/Billiards/renderer.cpp
    src/Billiards/render_resources.cpp
    src/Billiards/screenshot.cpp
    src/Billiards/screenshot_gl.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
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

add_executable(BilliardsScreenshotTests
    tests/screenshot_tests.cpp
    src/Billiards/screenshot.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsGameStateTests
    tests/game_state_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsAssetsTests
    tests/assets_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsInputTests
    tests/input_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsRenderResourcesTests
    tests/render_resources_tests.cpp
    src/Billiards/render_resources.cpp
    src/Billiards/ObjLoader.cpp
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

target_include_directories(BilliardsScreenshotTests PRIVATE
    src/Billiards
)

target_include_directories(BilliardsGameStateTests PRIVATE
    src/Billiards
)

target_include_directories(BilliardsAssetsTests PRIVATE
    src/Billiards
)

target_include_directories(BilliardsInputTests PRIVATE
    src/Billiards
)

target_include_directories(BilliardsRenderResourcesTests PRIVATE
    src/Billiards
    src/Billiards/dependencies/include
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)

target_compile_definitions(Billiards PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
)

foreach(test_target
    BilliardsPhysicsTests
    BilliardsRulesTests
    BilliardsScreenshotTests
    BilliardsGameStateTests
    BilliardsAssetsTests
    BilliardsInputTests
    BilliardsRenderResourcesTests
)
    target_compile_definitions(${test_target} PRIVATE
        BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
    )
endforeach()

add_test(NAME BilliardsPhysicsTests COMMAND BilliardsPhysicsTests)
add_test(NAME BilliardsRulesTests COMMAND BilliardsRulesTests)
add_test(NAME BilliardsScreenshotTests COMMAND BilliardsScreenshotTests)
add_test(NAME BilliardsGameStateTests COMMAND BilliardsGameStateTests)
add_test(NAME BilliardsAssetsTests COMMAND BilliardsAssetsTests)
add_test(NAME BilliardsInputTests COMMAND BilliardsInputTests)
add_test(NAME BilliardsRenderResourcesTests COMMAND BilliardsRenderResourcesTests)

add_test(
    NAME BilliardsScreenshotDefault
    COMMAND Billiards --screenshot /tmp/billiardgl-ctest-default.ppm --screenshot-scene default
)
add_test(
    NAME BilliardsScreenshotHelp
    COMMAND Billiards --screenshot /tmp/billiardgl-ctest-help.ppm --screenshot-scene help
)
add_test(
    NAME BilliardsScreenshotAfterShot
    COMMAND Billiards --screenshot /tmp/billiardgl-ctest-after-shot.ppm --screenshot-scene after-shot
)

target_link_libraries(Billiards PRIVATE
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    Threads::Threads
    PkgConfig::SDL2_AUDIO
    ${GLUT_LIBRARIES}
)

target_link_libraries(BilliardsRenderResourcesTests PRIVATE
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
)

if(APPLE)
    target_compile_definitions(Billiards PRIVATE GL_SILENCE_DEPRECATION)
endif()
