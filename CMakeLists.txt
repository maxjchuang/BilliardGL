cmake_minimum_required(VERSION 3.13)

project(BilliardGL LANGUAGES CXX)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(APPLE)
    enable_language(OBJCXX)
endif()

enable_testing()

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BILLIARDGL_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/Billiards")
set(BILLIARDGL_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(Threads REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_AUDIO REQUIRED IMPORTED_TARGET sdl2 SDL2_mixer)

set(BILLIARDGL_CORE_SOURCES
    src/Billiards/assets.cpp
    src/Billiards/ball_ball_contact.cpp
    src/Billiards/cushion_contact.cpp
    src/Billiards/cue_contact.cpp
    src/Billiards/frame_timing.cpp
    src/Billiards/game_state.cpp
    src/Billiards/game_runtime.cpp
    src/Billiards/image_loader.cpp
    src/Billiards/physics.cpp
    src/Billiards/physics_profile.cpp
    src/Billiards/physics_telemetry.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/rules.cpp
    src/Billiards/input.cpp
    src/Billiards/shot.cpp
    src/Billiards/surface_motion.cpp
    src/Billiards/table_specs.cpp
    src/Billiards/vec.cpp
)

if(APPLE)
    set(BILLIARDGL_PLATFORM_SOURCES
        src/Billiards/platform_scroll_mac.mm
    )
else()
    set(BILLIARDGL_PLATFORM_SOURCES
        src/Billiards/platform_scroll.cpp
    )
endif()

set(BILLIARDGL_COMMON_INCLUDE_DIRS
    src/Billiards
)

set(BILLIARDGL_RENDER_INCLUDE_DIRS
    src/Billiards
    src/Billiards/dependencies/include
    ${OPENGL_INCLUDE_DIR}
    ${GLEW_INCLUDE_DIRS}
)

function(billiardgl_configure_core_test target)
    target_include_directories(${target} PRIVATE
        ${BILLIARDGL_COMMON_INCLUDE_DIRS}
    )
    target_compile_definitions(${target} PRIVATE
        BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

function(billiardgl_add_core_test target test_source)
    add_executable(${target}
        ${test_source}
        ${BILLIARDGL_CORE_SOURCES}
    )
    billiardgl_configure_core_test(${target})
endfunction()

function(billiardgl_add_test target)
    target_include_directories(${target} PRIVATE
        ${BILLIARDGL_COMMON_INCLUDE_DIRS}
    )
    add_test(NAME ${target} COMMAND ${target})
endfunction()

add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/hud.cpp
    src/Billiards/renderer.cpp
    src/Billiards/render_resources.cpp
    src/Billiards/particle_resources.cpp
    src/Billiards/screenshot.cpp
    src/Billiards/screenshot_gl.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/platform_audio.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/automation_runner.cpp
    src/Billiards/stdio_transport.cpp
    src/Billiards/launch_options.cpp
    ${BILLIARDGL_PLATFORM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)

billiardgl_add_core_test(BilliardsPhysicsTests tests/physics_tests.cpp)
billiardgl_add_core_test(BilliardsBallBallContactTests tests/ball_ball_contact_tests.cpp)
billiardgl_add_core_test(BilliardsCushionContactTests tests/cushion_contact_tests.cpp)
billiardgl_add_core_test(BilliardsPhysicsProfileTests tests/physics_profile_tests.cpp)
billiardgl_add_core_test(BilliardsPhysicsTelemetryTests tests/physics_telemetry_tests.cpp)
billiardgl_add_core_test(BilliardsSurfaceMotionTests tests/surface_motion_tests.cpp)
billiardgl_add_core_test(BilliardsPhysicsInstrumentationTests tests/physics_instrumentation_tests.cpp)
billiardgl_add_core_test(BilliardsRulesTests tests/rules_tests.cpp)

add_executable(BilliardsPhysicsScenarioTests
    tests/physics_scenario_tests.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/automation_json.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsPhysicsScenarioTests)

add_executable(BilliardsScreenshotTests
    tests/screenshot_tests.cpp
    src/Billiards/screenshot.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsScreenshotTests)

billiardgl_add_core_test(BilliardsGameStateTests tests/game_state_tests.cpp)
billiardgl_add_core_test(BilliardsAssetsTests tests/assets_tests.cpp)
billiardgl_add_core_test(BilliardsInputTests tests/input_tests.cpp)
billiardgl_add_core_test(BilliardsShotTests tests/shot_tests.cpp)
billiardgl_add_core_test(BilliardsCueContactTests tests/cue_contact_tests.cpp)
billiardgl_add_core_test(BilliardsTableSpecsTests tests/table_specs_tests.cpp)
billiardgl_add_core_test(BilliardsGameRuntimeTests tests/game_runtime_tests.cpp)

add_executable(BilliardsVecTests
    tests/vec_tests.cpp
    src/Billiards/vec.cpp
)
billiardgl_add_test(BilliardsVecTests)

add_executable(BilliardsFrameTimingTests
    tests/frame_timing_tests.cpp
    src/Billiards/frame_timing.cpp
)
billiardgl_add_test(BilliardsFrameTimingTests)

add_executable(BilliardsObjLoaderTests
    tests/obj_loader_tests.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/resource_path.cpp
)
target_include_directories(BilliardsObjLoaderTests PRIVATE
    src/Billiards/dependencies/include
    ${GLUT_INCLUDE_DIR}
)
billiardgl_add_test(BilliardsObjLoaderTests)

add_executable(BilliardsRenderResourcesTests
    tests/render_resources_tests.cpp
    src/Billiards/render_resources.cpp
    src/Billiards/ObjLoader.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
target_include_directories(BilliardsRenderResourcesTests PRIVATE
    ${BILLIARDGL_RENDER_INCLUDE_DIRS}
)
target_compile_definitions(BilliardsRenderResourcesTests PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
billiardgl_add_test(BilliardsRenderResourcesTests)

add_executable(BilliardsArchitectureTests
    tests/architecture_tests.cpp
)
target_compile_definitions(BilliardsArchitectureTests PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
billiardgl_add_test(BilliardsArchitectureTests)

add_executable(BilliardsAutomationJsonTests
    tests/automation_json_tests.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsAutomationJsonTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsAutomationJsonTests)

add_executable(BilliardsAutomationProtocolTests
    tests/automation_protocol_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAutomationProtocolTests)

add_executable(BilliardsAutomationControllerTests
    tests/automation_controller_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAutomationControllerTests)

add_executable(BilliardsAutomationPhysicsScenarioTests
    tests/automation_physics_scenarios_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAutomationPhysicsScenarioTests)

add_executable(BilliardsLaunchOptionsTests
    tests/launch_options_tests.cpp
    src/Billiards/launch_options.cpp
)
target_include_directories(BilliardsLaunchOptionsTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsLaunchOptionsTests)

add_test(NAME BilliardsHeadlessAutomationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_headless_automation.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsHeadlessAutomationE2E PROPERTIES TIMEOUT 30)
add_test(NAME BilliardsPhysicsValidationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_physics_validation.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsPhysicsValidationE2E PROPERTIES TIMEOUT 60 LABELS "physics-validation;e2e")
add_test(NAME BilliardsReferenceValidationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_reference_validation.py
        $<TARGET_FILE:Billiards> ${CMAKE_CURRENT_BINARY_DIR}/physics-reference-validation)
set_tests_properties(BilliardsReferenceValidationE2E PROPERTIES TIMEOUT 60 LABELS "physics-validation;reference-validation;e2e")
add_test(NAME BilliardsRenderedAutomationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_rendered_automation.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsRenderedAutomationE2E PROPERTIES TIMEOUT 30 LABELS "rendered;e2e")

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

set(BILLIARDGL_SCREENSHOT_TEST_ROOT "${CMAKE_CURRENT_BINARY_DIR}/test-output")

function(billiardgl_add_screenshot_test test_name scene_name)
    set(test_output_dir "${BILLIARDGL_SCREENSHOT_TEST_ROOT}/${test_name}")
    file(MAKE_DIRECTORY "${test_output_dir}")
    add_test(
        NAME ${test_name}
        COMMAND Billiards --screenshot screenshot.ppm --screenshot-scene ${scene_name}
    )
    set_tests_properties(${test_name} PROPERTIES
        WORKING_DIRECTORY "${test_output_dir}"
    )
endfunction()

billiardgl_add_screenshot_test(BilliardsScreenshotDefault default)
billiardgl_add_screenshot_test(BilliardsScreenshotHelp help)
billiardgl_add_screenshot_test(BilliardsScreenshotAim aim)
billiardgl_add_screenshot_test(BilliardsScreenshotAfterShot after-shot)

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
    GLEW::GLEW
)

target_link_libraries(BilliardsObjLoaderTests PRIVATE
    ${GLUT_LIBRARIES}
)

if(APPLE)
    target_compile_definitions(Billiards PRIVATE GL_SILENCE_DEPRECATION)
endif()
