cmake_minimum_required(VERSION 3.13)

project(BilliardGL LANGUAGES CXX)
find_package(Python3 COMPONENTS Interpreter REQUIRED)

if(APPLE)
    enable_language(OBJCXX)
endif()

enable_testing()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

set(BILLIARDGL_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/Billiards")
set(BILLIARDGL_SOURCE_ROOT "${CMAKE_CURRENT_SOURCE_DIR}")

find_package(OpenGL REQUIRED)
find_package(GLEW REQUIRED)
find_package(GLUT REQUIRED)
find_package(Threads REQUIRED)
find_package(Freetype REQUIRED)
find_package(PkgConfig REQUIRED)
pkg_check_modules(SDL2_AUDIO REQUIRED IMPORTED_TARGET sdl2 SDL2_mixer)

set(BILLIARDGL_CORE_SOURCES
    src/Billiards/assets.cpp
    src/Billiards/ball_ball_contact.cpp
    src/Billiards/cushion_contact.cpp
    src/Billiards/continuous_collision.cpp
    src/Billiards/contact_island.cpp
    src/Billiards/contact_solver.cpp
    src/Billiards/coupled_cue_contact.cpp
    src/Billiards/cue_contact.cpp
    src/Billiards/camera/camera_state_machine.cpp
    src/Billiards/frame_timing.cpp
    src/Billiards/frozen_cue_topology.cpp
    src/Billiards/game_state.cpp
    src/Billiards/game_runtime.cpp
    src/Billiards/image_loader.cpp
    src/Billiards/physics.cpp
    src/Billiards/physics_profile.cpp
    src/Billiards/physics_telemetry.cpp
    src/Billiards/pocket_boundary.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/rules.cpp
    src/Billiards/rules/chinese_eight_ball_rules.cpp
    src/Billiards/rules/double_hit_classifier.cpp
    src/Billiards/rules/push_classifier.cpp
    src/Billiards/rules/frozen_cushion_classifier.cpp
    src/Billiards/rules/double_hit_close_case_declaration.cpp
    src/Billiards/rules/frozen_ball_declaration.cpp
    src/Billiards/rules/frozen_cushion_declaration.cpp
    src/Billiards/rules/group_confusion_authority.cpp
    src/Billiards/rules/push_review_declaration.cpp
    src/Billiards/rules/nonphysical_general_foul_authority.cpp
    src/Billiards/rules/rack_executor.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/stalemate_authority.cpp
    src/Billiards/rules/rules_decision_ui_model.cpp
    src/Billiards/rules/rules_presentation.cpp
    src/Billiards/rules/shot_fact_collector.cpp
    src/Billiards/rules/table_departure_observer.cpp
    src/Billiards/rules/table_departure_observer_adapter.cpp
    src/Billiards/rules/table_departure_telemetry_bridge.cpp
    src/Billiards/rules/unintentional_break_miscue_declaration.cpp
    src/Billiards/scenario_geometry.cpp
    src/Billiards/input.cpp
    src/Billiards/shot.cpp
    src/Billiards/shot_control_model.cpp
    src/Billiards/surface_motion.cpp
    src/Billiards/table_specs.cpp
    src/Billiards/table_departure_geometry.cpp
    src/Billiards/vec.cpp
)

set(BILLIARDGL_PHASE4_SYSTEM_SOURCES
    src/Billiards/deterministic_hash.cpp
    src/Billiards/replay/product_replay_content_registry.cpp
    src/Billiards/replay/replay_recorder.cpp
    src/Billiards/replay/replay_transport_model.cpp
    src/Billiards/replay/replay_v1.cpp
    src/Billiards/training/ball_edit_transaction.cpp
    src/Billiards/training/preview_runtime.cpp
    src/Billiards/training/training_product_controller.cpp
    src/Billiards/training/training_scenario.cpp
    src/Billiards/training/training_session.cpp
    src/Billiards/training/final_semantic_ownership.cpp
    src/Billiards/training/trajectory_compound_visibility.cpp
    src/Billiards/training/trajectory_overlay_model.cpp
)

set(BILLIARDGL_PRODUCT_AI_SOURCES
    src/Billiards/ai/ai_player_controller.cpp
    src/Billiards/ai/ai_turn_controller.cpp
    src/Billiards/ai/async_shot_planner.cpp
    src/Billiards/ai/game_preview_evaluator.cpp
    src/Billiards/ai/shot_planner.cpp
)

set(BILLIARDGL_PRODUCT_HUD_SOURCES
    src/Billiards/product_hud.cpp
)

set(BILLIARDGL_PRODUCT_UI_RENDER_SOURCES
    src/Billiards/render/ui/hud_draw_list.cpp
    src/Billiards/render/ui/hud_shipping_tokens.cpp
    src/Billiards/render/ui/training_control_hud.cpp
    src/Billiards/render/ui/training_goal_overlay.cpp
    src/Billiards/render/ui/rules_decision_product_draw.cpp
    src/Billiards/render/ui/rules_decision_ui_draw_list.cpp
    src/Billiards/render/ui/sdf_font_atlas.cpp
    src/Billiards/render/ui/sdf_generator.cpp
    src/Billiards/render/ui/system_font_resolver_mac.cpp
)

set(BILLIARDGL_PRODUCT_AUDIO_SOURCES
    src/Billiards/audio/audio_asset_policy.cpp
    src/Billiards/audio/audio_event_mapper.cpp
    src/Billiards/audio/audio_mixer_model.cpp
    src/Billiards/audio/product_audio_controller.cpp
)

set(BILLIARDGL_PRODUCT_INPUT_SOURCES
    src/Billiards/product_input_controller.cpp
    src/Billiards/product_settings.cpp
    src/Billiards/product_settings_runtime.cpp
    src/Billiards/settings_menu_model.cpp
)

set(BILLIARDGL_PRODUCT_SHELL_SOURCES
    src/Billiards/product_shell_controller.cpp
    src/Billiards/render/ui/product_shell_draw_list.cpp
)

set(BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES
    src/Billiards/render/modern/product_input_adapter.cpp
    src/Billiards/render/modern/table_plane_pointer_projection.cpp
    src/Billiards/render/modern/modern_product_session.cpp
    src/Billiards/render/ui/product_shell_layout.cpp
)

if(APPLE)
    set(BILLIARDGL_PLATFORM_SOURCES
        src/Billiards/platform_display_timing_mac.mm
        src/Billiards/platform_scroll_mac.mm
        src/Billiards/render/modern/modern_application_mac.mm
    )
else()
    set(BILLIARDGL_PLATFORM_SOURCES
        src/Billiards/platform_display_timing.cpp
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
    add_test(NAME ${target} COMMAND ${target} ${ARGN})
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

add_executable(BilliardsEnvironmentLightingTests tests/environment_lighting_tests.cpp)
billiardgl_add_test(BilliardsEnvironmentLightingTests)
set_tests_properties(BilliardsEnvironmentLightingTests PROPERTIES LABELS "graphics;lighting;cpu")

billiardgl_add_core_test(BilliardsGameRuntimeTableDepartureObservationTests
    tests/game_runtime_table_departure_observation_tests.cpp)
set_tests_properties(BilliardsGameRuntimeTableDepartureObservationTests PROPERTIES
    LABELS "physics;rules;runtime;cpu;phase5" TIMEOUT 120)

# The legacy world-space trajectory renderer has one compiled owner. Targets
# that exercise it link this library instead of compiling the implementation
# into each executable independently.
add_library(BilliardsPhase4TrajectoryOverlayImplementation STATIC
    src/Billiards/training/trajectory_overlay_renderer.cpp
)
target_include_directories(BilliardsPhase4TrajectoryOverlayImplementation
    PUBLIC src/Billiards)

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
    src/Billiards/render_state.cpp
    src/Billiards/platform_audio.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/automation_runner.cpp
    src/Billiards/stdio_transport.cpp
    src/Billiards/launch_options.cpp
    src/Billiards/render/legacy/legacy_renderer.cpp
    src/Billiards/render/legacy/legacy_renderer_gl.cpp
    src/Billiards/render/modern/formal_asset_attestation.cpp
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    ${BILLIARDGL_PRODUCT_HUD_SOURCES}
    ${BILLIARDGL_PRODUCT_UI_RENDER_SOURCES}
    src/Billiards/render/ui/product_ui_frame_coordinator.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    ${BILLIARDGL_PRODUCT_SHELL_SOURCES}
    ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_PLATFORM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)

billiardgl_add_core_test(BilliardsPhysicsTests tests/physics_tests.cpp)
billiardgl_add_core_test(BilliardsComposedEventBudgetRegressionTests
    tests/composed_event_budget_regression_tests.cpp)
billiardgl_add_core_test(BilliardsBallBallContactTests tests/ball_ball_contact_tests.cpp)
billiardgl_add_core_test(BilliardsCushionContactTests tests/cushion_contact_tests.cpp)
billiardgl_add_core_test(BilliardsPocketBoundaryTests tests/pocket_boundary_tests.cpp)
billiardgl_add_core_test(BilliardsPocketStateMachineTests tests/pocket_state_machine_tests.cpp)
billiardgl_add_core_test(BilliardsContinuousCollisionTests tests/continuous_collision_tests.cpp)
billiardgl_add_core_test(BilliardsContactIslandTests tests/contact_island_tests.cpp)
billiardgl_add_core_test(BilliardsFrozenCueTopologyTests tests/frozen_cue_topology_tests.cpp)
billiardgl_add_core_test(BilliardsCoupledCueContactTests tests/coupled_cue_contact_tests.cpp)
billiardgl_add_core_test(BilliardsContactSolverTests tests/contact_solver_tests.cpp)
billiardgl_add_core_test(BilliardsSimultaneousLandingReleaseTests
    tests/simultaneous_landing_release_tests.cpp)
target_sources(BilliardsSimultaneousLandingReleaseTests PRIVATE
    src/Billiards/deterministic_hash.cpp
    src/Billiards/automation_json.cpp)
set_tests_properties(BilliardsSimultaneousLandingReleaseTests PROPERTIES
    LABELS "physics;contact;cpu;headless"
    TIMEOUT 30)
billiardgl_add_core_test(BilliardsPhysicsProfileTests tests/physics_profile_tests.cpp)
billiardgl_add_core_test(BilliardsPhysicsTelemetryTests tests/physics_telemetry_tests.cpp)
billiardgl_add_core_test(BilliardsSurfaceMotionTests tests/surface_motion_tests.cpp)
billiardgl_add_core_test(BilliardsTableDepartureGeometryTests tests/table_departure_geometry_tests.cpp)
target_compile_definitions(BilliardsTableDepartureGeometryTests PRIVATE
    BILLIARDGL_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}")
billiardgl_add_core_test(BilliardsPhysicsInstrumentationTests tests/physics_instrumentation_tests.cpp)
billiardgl_add_core_test(BilliardsScenarioGeometryTests tests/scenario_geometry_tests.cpp)
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
billiardgl_add_core_test(BilliardsDoubleHitClassifierTests tests/double_hit_classifier_tests.cpp)
billiardgl_add_core_test(BilliardsPushClassifierTests tests/push_classifier_tests.cpp)
billiardgl_add_core_test(BilliardsFrozenCushionClassifierTests tests/frozen_cushion_classifier_tests.cpp)
billiardgl_add_core_test(BilliardsTableSpecsTests tests/table_specs_tests.cpp)
billiardgl_add_core_test(BilliardsGameRuntimeTests tests/game_runtime_tests.cpp)
billiardgl_add_core_test(BilliardsOrdinaryShotEquivalenceTests tests/ordinary_shot_equivalence_tests.cpp)

add_executable(BilliardsFullGameStress
    tests/full_game_stress.cpp
    tests/full_game_case_registry.cpp
    tests/full_game_component_cases.cpp
    tests/full_game_gameplay_cases.cpp
    tests/full_game_equivalence_cases.cpp
    tests/full_game_cli.cpp
    tests/full_game_artifacts.cpp
    tests/full_game_invariants.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
target_include_directories(BilliardsFullGameStress PRIVATE
    ${BILLIARDGL_COMMON_INCLUDE_DIRS}
)
target_compile_definitions(BilliardsFullGameStress PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
add_test(NAME BilliardsFullGameStress COMMAND BilliardsFullGameStress
    --case legacy_break_stress --seed 101)

add_executable(BilliardsFullGameArtifactTests
    tests/full_game_artifact_tests.cpp
    tests/full_game_artifacts.cpp
    tests/full_game_invariants.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsFullGameArtifactTests)

add_executable(BilliardsFullGameCliTests
    tests/full_game_cli_tests.cpp
    tests/full_game_case_registry.cpp
    tests/full_game_component_cases.cpp
    tests/full_game_gameplay_cases.cpp
    tests/full_game_equivalence_cases.cpp
    tests/full_game_cli.cpp
    tests/full_game_invariants.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsFullGameCliTests)

add_executable(BilliardsFullGamePerformance
    tests/full_game_performance.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsFullGamePerformance)

add_executable(BilliardsFullGameComponentCasesTests
    tests/full_game_component_cases_tests.cpp
    tests/full_game_case_registry.cpp
    tests/full_game_component_cases.cpp
    tests/full_game_gameplay_cases.cpp
    tests/full_game_equivalence_cases.cpp
    tests/full_game_invariants.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsFullGameComponentCasesTests)

add_executable(BilliardsFullGameGameplayCasesTests
    tests/full_game_gameplay_cases_tests.cpp
    tests/full_game_case_registry.cpp
    tests/full_game_component_cases.cpp
    tests/full_game_gameplay_cases.cpp
    tests/full_game_equivalence_cases.cpp
    tests/full_game_invariants.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsFullGameGameplayCasesTests)

add_executable(BilliardsFullGameEquivalenceTests
    tests/full_game_equivalence_tests.cpp
    tests/full_game_case_registry.cpp
    tests/full_game_component_cases.cpp
    tests/full_game_gameplay_cases.cpp
    tests/full_game_equivalence_cases.cpp
    tests/full_game_invariants.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsFullGameEquivalenceTests)

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

add_executable(BilliardsRenderStateTests
    tests/render_state_tests.cpp
    src/Billiards/camera/camera_state_machine.cpp
    src/Billiards/frame_timing.cpp
    src/Billiards/render_state.cpp
    src/Billiards/game_state.cpp
    src/Billiards/shot.cpp
    src/Billiards/shot_control_model.cpp
)
billiardgl_add_test(BilliardsRenderStateTests)

billiardgl_add_core_test(BilliardsInteractivePhysicsTests
    tests/interactive_physics_tests.cpp)

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

add_executable(BilliardsLegacyRendererTests
    tests/legacy_renderer_tests.cpp
    src/Billiards/render/legacy/legacy_renderer.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/resource_path.cpp
)
target_include_directories(BilliardsLegacyRendererTests PRIVATE
    src/Billiards
    src/Billiards/dependencies/include
)
target_compile_definitions(BilliardsLegacyRendererTests PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
)
billiardgl_add_test(BilliardsLegacyRendererTests)

add_executable(BilliardsLegacyRendererCompatibilityTests
    tests/legacy_renderer_compatibility_tests.cpp
    src/Billiards/render/legacy/legacy_renderer.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/resource_path.cpp
)
target_include_directories(BilliardsLegacyRendererCompatibilityTests PRIVATE
    src/Billiards
    src/Billiards/dependencies/include
)
target_compile_definitions(BilliardsLegacyRendererCompatibilityTests PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
)
billiardgl_add_test(BilliardsLegacyRendererCompatibilityTests)

add_executable(BilliardsAutomationJsonTests
    tests/automation_json_tests.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsAutomationJsonTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsAutomationJsonTests)

add_executable(BilliardsGltfImporterTests
    tests/gltf_importer_tests.cpp
    src/Billiards/render/gltf/gltf_importer.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsGltfImporterTests PRIVATE src/Billiards)
target_compile_definitions(BilliardsGltfImporterTests PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
billiardgl_add_test(BilliardsGltfImporterTests)
set_tests_properties(BilliardsGltfImporterTests PROPERTIES LABELS "render;asset-validation;cpu")

add_executable(BilliardsGltfDrawPackageTests
    tests/gltf_draw_package_tests.cpp
    src/Billiards/render/gltf/gltf_importer.cpp
    src/Billiards/render/gltf/gltf_draw_package.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsGltfDrawPackageTests PRIVATE src/Billiards)
target_compile_definitions(BilliardsGltfDrawPackageTests PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
billiardgl_add_test(BilliardsGltfDrawPackageTests)
set_tests_properties(BilliardsGltfDrawPackageTests PROPERTIES LABELS "render;asset-validation;cpu;lod")

add_executable(BilliardsCameraShotModelTests
    tests/camera_shot_model_tests.cpp
    src/Billiards/camera/camera_state_machine.cpp
    src/Billiards/shot_control_model.cpp
    src/Billiards/shot.cpp
)
target_include_directories(BilliardsCameraShotModelTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsCameraShotModelTests)
set_tests_properties(BilliardsCameraShotModelTests PROPERTIES
    LABELS "camera;shot-control;cpu;phase3"
)

add_executable(BilliardsPhase3IntegrationTests
    tests/phase3_integration_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsPhase3IntegrationTests)
set_tests_properties(BilliardsPhase3IntegrationTests PROPERTIES
    LABELS "camera;shot-control;automation;presentation;phase3"
)

add_executable(BilliardsPhase4SystemTests
    tests/phase4_system_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsPhase4SystemTests)
set_tests_properties(BilliardsPhase4SystemTests PROPERTIES
    LABELS "training;preview;replay;phase4"
)

add_executable(BilliardsReplayRecorderTests
    tests/replay_recorder_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsReplayRecorderTests)
set_tests_properties(BilliardsReplayRecorderTests PROPERTIES
    LABELS "replay;recording;phase4"
)

add_executable(BilliardsPhase4ProductControllerTests
    tests/phase4_product_controller_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsTrajectoryOverlayRendererTests
    tests/trajectory_overlay_renderer_tests.cpp
    src/Billiards/training/trajectory_overlay_model.cpp
)
billiardgl_configure_core_test(BilliardsTrajectoryOverlayRendererTests)
target_link_libraries(BilliardsTrajectoryOverlayRendererTests PRIVATE
    BilliardsPhase4TrajectoryOverlayImplementation
)
set_tests_properties(BilliardsTrajectoryOverlayRendererTests PROPERTIES
    LABELS "training;replay;trajectory;projection;phase4"
)

add_executable(BilliardsTrajectoryCompoundVisibilityTests
    tests/trajectory_compound_visibility_tests.cpp
    src/Billiards/training/trajectory_compound_visibility.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsTrajectoryCompoundVisibilityTests PRIVATE
    src/Billiards)
target_compile_definitions(BilliardsTrajectoryCompoundVisibilityTests PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
billiardgl_add_test(BilliardsTrajectoryCompoundVisibilityTests)
set_tests_properties(BilliardsTrajectoryCompoundVisibilityTests PROPERTIES
    LABELS "training;replay;trajectory;compound-visibility;headless;cpu;phase4"
)

add_executable(BilliardsPhase4OverlayV4TopologyAudit
    spikes/render_core_profile/phase4_overlay_v4_topology_audit.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
target_include_directories(BilliardsPhase4OverlayV4TopologyAudit PRIVATE
    ${BILLIARDGL_COMMON_INCLUDE_DIRS}
)
target_compile_definitions(BilliardsPhase4OverlayV4TopologyAudit PRIVATE
    BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
target_link_libraries(BilliardsPhase4OverlayV4TopologyAudit PRIVATE
    BilliardsPhase4TrajectoryOverlayImplementation
)

add_executable(BilliardsPhase4OverlayV4CompoundAuditRunner
    spikes/render_core_profile/phase4_overlay_v4_compound_audit_runner.cpp
    src/Billiards/training/trajectory_compound_visibility.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsPhase4OverlayV4CompoundAuditRunner PRIVATE
    src/Billiards)
target_compile_definitions(BilliardsPhase4OverlayV4CompoundAuditRunner PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)

add_executable(BilliardsProductHudTests
    tests/product_hud_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PRODUCT_HUD_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsProductHudTests)
set_tests_properties(BilliardsProductHudTests PROPERTIES
    LABELS "hud;product;rules;layout;phase3;phase5"
)

add_executable(BilliardsShippingHudRenderTests
    tests/shipping_hud_render_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/table_specs.cpp
    ${BILLIARDGL_PRODUCT_HUD_SOURCES}
    ${BILLIARDGL_PRODUCT_UI_RENDER_SOURCES}
)
billiardgl_configure_core_test(BilliardsShippingHudRenderTests)
target_link_libraries(BilliardsShippingHudRenderTests PRIVATE Freetype::Freetype)
set_tests_properties(BilliardsShippingHudRenderTests PROPERTIES
    LABELS "hud;text;sdf;freetype;draw-list;phase3"
)

add_executable(BilliardsHudShippingTokenTests
    tests/hud_shipping_token_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/render/ui/hud_shipping_tokens.cpp
)
target_include_directories(BilliardsHudShippingTokenTests PRIVATE src/Billiards)
target_compile_definitions(BilliardsHudShippingTokenTests PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
billiardgl_add_test(BilliardsHudShippingTokenTests)
set_tests_properties(BilliardsHudShippingTokenTests PROPERTIES
    LABELS "hud;shipping-token;geometry;palette;headless;cpu;phase3"
)

add_executable(BilliardsTrainingGoalOverlayTests
    tests/training_goal_overlay_tests.cpp
    src/Billiards/render/ui/training_goal_overlay.cpp
    src/Billiards/table_specs.cpp
)
target_include_directories(BilliardsTrainingGoalOverlayTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsTrainingGoalOverlayTests)
set_tests_properties(BilliardsTrainingGoalOverlayTests PROPERTIES
    LABELS "training;ui;geometry;headless;cpu"
)

add_executable(BilliardsProductUiFrameCoordinatorTests
    tests/product_ui_frame_coordinator_tests.cpp
    src/Billiards/table_specs.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/product_hud.cpp
    src/Billiards/product_settings.cpp
    src/Billiards/render/ui/hud_draw_list.cpp
    src/Billiards/render/ui/hud_shipping_tokens.cpp
    src/Billiards/render/ui/training_control_hud.cpp
    src/Billiards/render/ui/training_goal_overlay.cpp
    src/Billiards/render/ui/product_shell_draw_list.cpp
    src/Billiards/render/ui/product_shell_layout.cpp
    src/Billiards/render/ui/product_ui_frame_coordinator.cpp
    src/Billiards/render/ui/rules_decision_product_draw.cpp
    src/Billiards/render/ui/rules_decision_ui_draw_list.cpp
    src/Billiards/render/ui/sdf_font_atlas.cpp
    src/Billiards/render/ui/sdf_generator.cpp
    src/Billiards/render/ui/system_font_resolver_mac.cpp
)
billiardgl_configure_core_test(BilliardsProductUiFrameCoordinatorTests)
target_link_libraries(BilliardsProductUiFrameCoordinatorTests PRIVATE
    BilliardsPhase4V19TrajectoryInsetImplementation
    Freetype::Freetype
)
set_tests_properties(BilliardsProductUiFrameCoordinatorTests PROPERTIES
    LABELS "ui;shell;hud;coordinator;shipping-token;headless;cpu;phase3;phase5"
)

add_library(BilliardsProductUiGpuCore STATIC
    src/Billiards/render/ui/product_ui_gpu_renderer.cpp
)
target_include_directories(BilliardsProductUiGpuCore PUBLIC src/Billiards)

add_library(BilliardsProductUiOpenGL41 STATIC
    src/Billiards/render/modern/product_ui_gl_core41.cpp
)
target_include_directories(BilliardsProductUiOpenGL41 PUBLIC src/Billiards)
target_link_libraries(BilliardsProductUiOpenGL41 PUBLIC
    BilliardsProductUiGpuCore
    OpenGL::GL
)
if(NOT APPLE)
    target_link_libraries(BilliardsProductUiOpenGL41 PUBLIC GLEW::GLEW)
endif()
target_compile_definitions(BilliardsProductUiOpenGL41 PRIVATE
    $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>
)

add_library(BilliardsModernProductUiRuntime STATIC
    src/Billiards/table_specs.cpp
    src/Billiards/render/modern/modern_product_ui_runtime.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/product_hud.cpp
    src/Billiards/product_settings.cpp
    src/Billiards/render/ui/hud_draw_list.cpp
    src/Billiards/render/ui/hud_shipping_tokens.cpp
    src/Billiards/render/ui/training_control_hud.cpp
    src/Billiards/render/ui/training_goal_overlay.cpp
    src/Billiards/render/ui/product_shell_draw_list.cpp
    src/Billiards/render/ui/product_shell_layout.cpp
    src/Billiards/render/ui/product_ui_frame_coordinator.cpp
    src/Billiards/render/ui/rules_decision_product_draw.cpp
    src/Billiards/render/ui/rules_decision_ui_draw_list.cpp
    src/Billiards/render/ui/sdf_font_atlas.cpp
    src/Billiards/render/ui/sdf_generator.cpp
)
target_include_directories(BilliardsModernProductUiRuntime PUBLIC src/Billiards)
target_link_libraries(BilliardsModernProductUiRuntime PUBLIC
    BilliardsProductUiGpuCore
    Freetype::Freetype
)

# V19 trajectory inset implementations have one compiled owner. The visible
# integration uses Product UI layout/chrome code, while the Product UI frame
# coordinator constructs V19 packages, so these two static libraries form an
# intentional CMake-managed static SCC.
add_library(BilliardsPhase4V19TrajectoryInsetImplementation STATIC
    src/Billiards/training/trajectory_inset_model.cpp
    src/Billiards/training/trajectory_inset_renderer.cpp
    src/Billiards/render/modern/trajectory_inset_atomic_transaction.cpp
    src/Billiards/render/modern/trajectory_inset_visible_integration.cpp
)
target_include_directories(BilliardsPhase4V19TrajectoryInsetImplementation
    PUBLIC src/Billiards)
target_link_libraries(BilliardsPhase4V19TrajectoryInsetImplementation PUBLIC
    BilliardsModernProductUiRuntime
)
target_link_libraries(BilliardsModernProductUiRuntime PUBLIC
    BilliardsPhase4V19TrajectoryInsetImplementation
)

add_executable(BilliardsPhase4V19TrajectoryInsetVisibleIntegrationCpuTests
    tests/phase4_v19_trajectory_inset_visible_integration_cpu_tests.cpp
    src/Billiards/render/modern/modern_renderer_product_ui.cpp
)
billiardgl_configure_core_test(
    BilliardsPhase4V19TrajectoryInsetVisibleIntegrationCpuTests)
target_link_libraries(
    BilliardsPhase4V19TrajectoryInsetVisibleIntegrationCpuTests PRIVATE
    BilliardsPhase4V19TrajectoryInsetImplementation
    BilliardsModernProductUiRuntime
    BilliardsProductUiOpenGL41
)
set_tests_properties(
    BilliardsPhase4V19TrajectoryInsetVisibleIntegrationCpuTests PROPERTIES
    TIMEOUT 300
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    LABELS "trajectory;inset;visible-integration;core41;headless;cpu;phase4"
)

add_executable(BilliardsTrajectoryInsetEventVisitMarkerTests
    tests/trajectory_inset_event_visit_marker_tests.cpp
    src/Billiards/training/trajectory_overlay_renderer.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/automation_json.cpp
)
billiardgl_configure_core_test(BilliardsTrajectoryInsetEventVisitMarkerTests)
target_link_libraries(BilliardsTrajectoryInsetEventVisitMarkerTests PRIVATE
    BilliardsPhase4V19TrajectoryInsetImplementation
)
set_tests_properties(BilliardsTrajectoryInsetEventVisitMarkerTests PROPERTIES
    TIMEOUT 60
    WORKING_DIRECTORY "${CMAKE_SOURCE_DIR}"
    LABELS "trajectory;inset;phase4;headless;cpu"
)

add_executable(BilliardsProductUiGpuRendererTests
    tests/product_ui_gpu_renderer_tests.cpp
    src/Billiards/render/ui/product_shell_draw_list.cpp
    src/Billiards/render/ui/product_shell_layout.cpp
    src/Billiards/render/ui/hud_shipping_tokens.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/render/ui/sdf_font_atlas.cpp
    src/Billiards/render/ui/sdf_generator.cpp
)
billiardgl_configure_core_test(BilliardsProductUiGpuRendererTests)
target_link_libraries(BilliardsProductUiGpuRendererTests PRIVATE
    BilliardsProductUiGpuCore
    Freetype::Freetype
)
set_tests_properties(BilliardsProductUiGpuRendererTests PROPERTIES
    LABELS "ui;gpu-submit;recording-dispatch;headless;core41;phase3"
)

add_executable(BilliardsModernProductUiRuntimeTests
    tests/modern_product_ui_runtime_tests.cpp
)
billiardgl_configure_core_test(BilliardsModernProductUiRuntimeTests)
target_link_libraries(BilliardsModernProductUiRuntimeTests PRIVATE
    BilliardsModernProductUiRuntime
)
set_tests_properties(BilliardsModernProductUiRuntimeTests PROPERTIES
    LABELS "ui;runtime;facade;lifecycle;headless;gpu-submit;phase3"
)

add_executable(BilliardsRulesDecisionProductRuntimeTests
    tests/rules_decision_product_runtime_tests.cpp
)

add_executable(BilliardsPhase5RulesDecisionUiV3CpuFramePreflight
    tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp
)
billiardgl_configure_core_test(
    BilliardsPhase5RulesDecisionUiV3CpuFramePreflight
    "${CMAKE_SOURCE_DIR}/docs/evidence/phase5/rules-decision-ui-v3-targeted-ready-v1/rules-decision-ui-v3-targeted-plan.tsv"
    "${CMAKE_CURRENT_BINARY_DIR}/rules-ui-v3-cpu-preflight.json")
target_compile_definitions(
    BilliardsPhase5RulesDecisionUiV3CpuFramePreflight PRIVATE
    BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED=1)
target_link_libraries(BilliardsPhase5RulesDecisionUiV3CpuFramePreflight PRIVATE
    BilliardsModernProductUiRuntime)

add_executable(BilliardsPhase5RulesDecisionUiV4Exact16CpuFramePreflight
    tests/phase5_rules_decision_ui_v2_cpu_frame_preflight.cpp
)
billiardgl_configure_core_test(
    BilliardsPhase5RulesDecisionUiV4Exact16CpuFramePreflight
    "${CMAKE_SOURCE_DIR}/docs/evidence/phase5/rules-decision-ui-v4-exact16-ready-v2/rules-decision-ui-v4-exact16-plan.tsv"
    "${CMAKE_CURRENT_BINARY_DIR}/rules-ui-v4-cpu-preflight.json")
target_compile_definitions(
    BilliardsPhase5RulesDecisionUiV4Exact16CpuFramePreflight PRIVATE
    BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1)
target_link_libraries(
    BilliardsPhase5RulesDecisionUiV4Exact16CpuFramePreflight PRIVATE
    BilliardsModernProductUiRuntime)

add_executable(BilliardsPhase5RulesDecisionUiV5CpuRasterTests
    tests/phase5_rules_decision_ui_v5_cpu_raster_tests.cpp
)
billiardgl_configure_core_test(
    BilliardsPhase5RulesDecisionUiV5CpuRasterTests)
target_link_libraries(
    BilliardsPhase5RulesDecisionUiV5CpuRasterTests PRIVATE
    BilliardsModernProductUiRuntime)
set_tests_properties(BilliardsPhase5RulesDecisionUiV5CpuRasterTests PROPERTIES
    LABELS "ui;rules;cpu-raster;pixel-topology;phase5"
)
billiardgl_configure_core_test(BilliardsRulesDecisionProductRuntimeTests)
target_link_libraries(BilliardsRulesDecisionProductRuntimeTests PRIVATE
    BilliardsModernProductUiRuntime
)
set_tests_properties(BilliardsRulesDecisionProductRuntimeTests PROPERTIES
    LABELS "ui;runtime;rules;authority;hit-rect;headless;phase5"
)

add_executable(BilliardsModernRendererProductUiTests
    tests/modern_renderer_product_ui_tests.cpp
    src/Billiards/render/modern/modern_renderer_product_ui.cpp
    src/Billiards/product_settings.cpp
    src/Billiards/automation_json.cpp
)
billiardgl_configure_core_test(BilliardsModernRendererProductUiTests)
set_tests_properties(BilliardsModernRendererProductUiTests PROPERTIES
    LABELS "renderer;ui;product-branch;recording-backend;headless;phase3"
)

add_executable(BilliardsProductAudioFoundationTests
    tests/product_audio_foundation_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/product_settings.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsProductAudioFoundationTests)
set_tests_properties(BilliardsProductAudioFoundationTests PROPERTIES
    LABELS "audio;product;physics;headless;phase3"
)

add_executable(BilliardsProductAudioControllerTests
    tests/product_audio_controller_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/product_settings.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsProductAudioControllerTests)
set_tests_properties(BilliardsProductAudioControllerTests PROPERTIES
    LABELS "audio;product;consumer;headless;phase5"
)

add_executable(BilliardsProductAudioCandidateTests
    tests/product_audio_candidate_tests.cpp
    src/Billiards/platform_audio.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/product_settings.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsProductAudioCandidateTests)
target_link_libraries(BilliardsProductAudioCandidateTests PRIVATE PkgConfig::SDL2_AUDIO Threads::Threads)
set_tests_properties(BilliardsProductAudioCandidateTests PROPERTIES
    LABELS "audio;product;candidate;SDL;headless;phase5"
    TIMEOUT 30
    ENVIRONMENT "SDL_AUDIODRIVER=dummy"
)

add_executable(BilliardsProductInputControllerTests
    tests/product_input_controller_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsProductInputControllerTests)
set_tests_properties(BilliardsProductInputControllerTests PROPERTIES
    LABELS "input;product;settings;training;replay;headless;phase5"
)

add_executable(BilliardsProductInputAdapterTests
    tests/product_input_adapter_tests.cpp
    src/Billiards/render/modern/product_input_adapter.cpp
)
billiardgl_add_test(BilliardsProductInputAdapterTests)
set_tests_properties(BilliardsProductInputAdapterTests PROPERTIES
    LABELS "input;adapter;product;headless;phase5"
)

add_executable(BilliardsTablePlanePointerProjectionTests
    tests/table_plane_pointer_projection_tests.cpp
    src/Billiards/render/modern/table_plane_pointer_projection.cpp
)
billiardgl_add_test(BilliardsTablePlanePointerProjectionTests)
set_tests_properties(BilliardsTablePlanePointerProjectionTests PROPERTIES
    LABELS "input;projection;rules;product;headless;phase5"
)

add_executable(BilliardsModernProductAudioSessionE2E
    tests/modern_product_audio_session_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    src/Billiards/product_shell_controller.cpp
    ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsModernProductAudioSessionE2E)
set_tests_properties(BilliardsModernProductAudioSessionE2E PROPERTIES
    LABELS "audio;product;authority;session;headless;e2e;phase5"
)

add_executable(BilliardsModernProductPointerSessionE2E
    tests/modern_product_pointer_session_tests.cpp
    src/Billiards/render_state.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    src/Billiards/product_shell_controller.cpp
    ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)

add_executable(BilliardsProductReplayContentRegistryTests
    tests/product_replay_content_registry_tests.cpp
    src/Billiards/render_state.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    src/Billiards/product_shell_controller.cpp
    ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsProductReplayContentRegistryTests)
set_tests_properties(BilliardsProductReplayContentRegistryTests PROPERTIES
    LABELS "headless;product;replay;rules"
)
billiardgl_configure_core_test(BilliardsModernProductPointerSessionE2E)
target_compile_definitions(BilliardsModernProductPointerSessionE2E PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
set_tests_properties(BilliardsModernProductPointerSessionE2E PROPERTIES
    LABELS "input;pointer;shell;settings;training;replay;headless;e2e;phase5"
)

add_executable(BilliardsRulesDecisionSessionIntegrationTests
    tests/rules_decision_session_integration_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    src/Billiards/product_shell_controller.cpp
    ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsRulesDecisionSessionIntegrationTests)
target_link_libraries(BilliardsRulesDecisionSessionIntegrationTests PRIVATE
    Threads::Threads
    PkgConfig::SDL2_AUDIO
)
set_tests_properties(BilliardsRulesDecisionSessionIntegrationTests PROPERTIES
    LABELS "product;session;rules;authority;headless;phase5"
)

add_executable(BilliardsModernProductFirstShotEvidenceE2E
    tests/modern_product_first_shot_evidence_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    src/Billiards/product_shell_controller.cpp
    ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsModernProductFirstShotEvidenceE2E)
target_compile_definitions(BilliardsModernProductFirstShotEvidenceE2E PRIVATE
    BILLIARDGL_FIRST_SHOT_EVIDENCE_ROOT="${CMAKE_CURRENT_BINARY_DIR}/test-output/first-shot-proxy"
)
set_tests_properties(BilliardsModernProductFirstShotEvidenceE2E PROPERTIES
    TIMEOUT 32
    LABELS "input;pointer;retina;authority;headless;e2e;engineering-proxy;phase3"
)

add_executable(BilliardsProductShellTests
    tests/product_shell_tests.cpp
    src/Billiards/product_hud.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render/ui/sdf_font_atlas.cpp
    src/Billiards/render/ui/sdf_generator.cpp
    src/Billiards/render/ui/system_font_resolver_mac.cpp
    src/Billiards/render/ui/product_shell_layout.cpp
    src/Billiards/render/ui/hud_shipping_tokens.cpp
    ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
    ${BILLIARDGL_PRODUCT_SHELL_SOURCES}
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsProductShellTests)
target_compile_definitions(BilliardsProductShellTests PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
target_link_libraries(BilliardsProductShellTests PRIVATE Freetype::Freetype)
set_tests_properties(BilliardsProductShellTests PROPERTIES
    LABELS "shell;ui;draw-list;settings;training;replay;headless;phase5"
)

billiardgl_configure_core_test(BilliardsPhase4ProductControllerTests)
set_tests_properties(BilliardsPhase4ProductControllerTests PROPERTIES
    LABELS "training;replay;trajectory;phase4;product"
)

add_executable(BilliardsChineseEightBallRulesTests
    tests/chinese_eight_ball_rules_tests.cpp
    src/Billiards/rules/chinese_eight_ball_rules.cpp
)
target_include_directories(BilliardsChineseEightBallRulesTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsChineseEightBallRulesTests)
set_tests_properties(BilliardsChineseEightBallRulesTests PROPERTIES
    LABELS "rules;phase5;cpu"
)

add_executable(BilliardsAiShotPlannerTests
    tests/ai_shot_planner_tests.cpp
    src/Billiards/ai/shot_planner.cpp
)
target_include_directories(BilliardsAiShotPlannerTests PRIVATE src/Billiards)
target_link_libraries(BilliardsAiShotPlannerTests PRIVATE Threads::Threads)
billiardgl_add_test(BilliardsAiShotPlannerTests)
set_tests_properties(BilliardsAiShotPlannerTests PROPERTIES
    LABELS "ai;phase5;cpu"
)

add_executable(BilliardsAiAcceptanceTests
    tests/ai_acceptance_tests.cpp
    src/Billiards/ai/shot_planner.cpp
)
target_include_directories(BilliardsAiAcceptanceTests PRIVATE src/Billiards)
target_link_libraries(BilliardsAiAcceptanceTests PRIVATE Threads::Threads)
billiardgl_add_test(BilliardsAiAcceptanceTests)
set_tests_properties(BilliardsAiAcceptanceTests PROPERTIES
    LABELS "ai;acceptance;phase5;cpu"
)

add_executable(BilliardsAiPhysicalAcceptanceTests
    tests/ai_physical_acceptance_tests.cpp
    src/Billiards/ai/game_preview_evaluator.cpp
    src/Billiards/ai/shot_planner.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAiPhysicalAcceptanceTests)
set_tests_properties(BilliardsAiPhysicalAcceptanceTests PROPERTIES
    LABELS "ai;acceptance;physics;preview;cpu;phase5"
    TIMEOUT 120
)

add_executable(BilliardsAiRulesRuntimeAcceptanceTests
    tests/ai_rules_runtime_acceptance_tests.cpp
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAiRulesRuntimeAcceptanceTests)
target_link_libraries(BilliardsAiRulesRuntimeAcceptanceTests PRIVATE Threads::Threads)
set_tests_properties(BilliardsAiRulesRuntimeAcceptanceTests PROPERTIES
    LABELS "ai;acceptance;physics;rules;runtime;cpu;phase5"
    TIMEOUT 120
)

add_executable(BilliardsAiFullMatchAcceptanceTests
    tests/ai_full_match_acceptance_tests.cpp
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    src/Billiards/automation_json.cpp
    src/Billiards/physics_scenario.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAiFullMatchAcceptanceTests)
target_link_libraries(BilliardsAiFullMatchAcceptanceTests PRIVATE Threads::Threads)
set_tests_properties(BilliardsAiFullMatchAcceptanceTests PROPERTIES
    LABELS "ai;acceptance;physics;rules;runtime;cpu;phase5"
    TIMEOUT 300
)

add_executable(BilliardsAsyncShotPlannerTests
    tests/async_shot_planner_tests.cpp
    src/Billiards/ai/async_shot_planner.cpp
    src/Billiards/ai/shot_planner.cpp
)
target_include_directories(BilliardsAsyncShotPlannerTests PRIVATE src/Billiards)
target_link_libraries(BilliardsAsyncShotPlannerTests PRIVATE Threads::Threads)
billiardgl_add_test(BilliardsAsyncShotPlannerTests)
set_tests_properties(BilliardsAsyncShotPlannerTests PROPERTIES
    LABELS "ai;threading;phase5;cpu"
)

add_executable(BilliardsGamePreviewEvaluatorTests
    tests/game_preview_evaluator_tests.cpp
    src/Billiards/ai/game_preview_evaluator.cpp
    src/Billiards/ai/shot_planner.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsGamePreviewEvaluatorTests)
set_tests_properties(BilliardsGamePreviewEvaluatorTests PROPERTIES
    LABELS "ai;preview;isolation;phase5"
)

add_executable(BilliardsAiTurnControllerTests
    tests/ai_turn_controller_tests.cpp
    ${BILLIARDGL_PRODUCT_AI_SOURCES}
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAiTurnControllerTests)
target_link_libraries(BilliardsAiTurnControllerTests PRIVATE Threads::Threads)
set_tests_properties(BilliardsAiTurnControllerTests PROPERTIES
    LABELS "ai;threading;preview;rules;phase5"
)

add_executable(BilliardsShotFactCollectorTests
    tests/shot_fact_collector_tests.cpp
    src/Billiards/rules/chinese_eight_ball_rules.cpp
    src/Billiards/rules/shot_fact_collector.cpp
    src/Billiards/game_state.cpp
)
target_include_directories(BilliardsShotFactCollectorTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsShotFactCollectorTests)
set_tests_properties(BilliardsShotFactCollectorTests PROPERTIES
    LABELS "rules;phase5;cpu"
)

add_library(BilliardsD6BTableDepartureObserver STATIC
    src/Billiards/rules/table_departure_observer.cpp
)
target_include_directories(BilliardsD6BTableDepartureObserver PUBLIC
    src/Billiards
)

add_executable(BilliardsTableDepartureObserverTests
    tests/table_departure_observer_tests.cpp
)
target_link_libraries(BilliardsTableDepartureObserverTests PRIVATE
    BilliardsD6BTableDepartureObserver
)
billiardgl_configure_core_test(BilliardsTableDepartureObserverTests)
set_tests_properties(BilliardsTableDepartureObserverTests PROPERTIES
    TIMEOUT 60
    LABELS "rules;physics;telemetry;d6-b;headless;cpu"
)

add_library(BilliardsD6BTableDepartureObserverAdapter STATIC
    src/Billiards/rules/table_departure_observer_adapter.cpp
)
target_include_directories(BilliardsD6BTableDepartureObserverAdapter PUBLIC
    src/Billiards
)
target_link_libraries(BilliardsD6BTableDepartureObserverAdapter PUBLIC
    BilliardsD6BTableDepartureObserver
)

add_executable(BilliardsTableDepartureObserverAdapterTests
    tests/table_departure_observer_adapter_tests.cpp
    src/Billiards/table_departure_geometry.cpp
    src/Billiards/physics_profile.cpp
    src/Billiards/surface_motion.cpp
    src/Billiards/ball_ball_contact.cpp
    src/Billiards/pocket_boundary.cpp
)
target_link_libraries(BilliardsTableDepartureObserverAdapterTests PRIVATE
    BilliardsD6BTableDepartureObserverAdapter
)
billiardgl_configure_core_test(BilliardsTableDepartureObserverAdapterTests)
set_tests_properties(BilliardsTableDepartureObserverAdapterTests PROPERTIES
    TIMEOUT 60
    LABELS "rules;physics;telemetry;d6-a;d6-b;adapter;headless;cpu"
)

add_library(BilliardsD6BTableDepartureTelemetryBridge STATIC
    src/Billiards/rules/table_departure_telemetry_bridge.cpp
)
target_include_directories(BilliardsD6BTableDepartureTelemetryBridge PUBLIC
    src/Billiards
)
target_link_libraries(BilliardsD6BTableDepartureTelemetryBridge PUBLIC
    BilliardsD6BTableDepartureObserverAdapter
)

add_executable(BilliardsTableDepartureTelemetryBridgeTests
    tests/table_departure_telemetry_bridge_tests.cpp
    ${BILLIARDGL_CORE_SOURCES}
)
target_link_libraries(BilliardsTableDepartureTelemetryBridgeTests PRIVATE
    BilliardsD6BTableDepartureTelemetryBridge
)
billiardgl_configure_core_test(BilliardsTableDepartureTelemetryBridgeTests)
set_tests_properties(BilliardsTableDepartureTelemetryBridgeTests PROPERTIES
    TIMEOUT 120
    LABELS "rules;physics;telemetry;d6-a;d6-b;bridge;headless;cpu"
)

add_test(NAME BilliardsPrivateLineageConsumerRuntimeTests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_m4_d6_a_private_lineage_consumer_runtime.py
)
set_tests_properties(BilliardsPrivateLineageConsumerRuntimeTests PROPERTIES
    TIMEOUT 60
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    LABELS "physics;private-lineage;consumer-runtime;headless;cpu"
)

add_executable(BilliardsProductSettingsTests
    tests/product_settings_tests.cpp
    src/Billiards/product_settings.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsProductSettingsTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsProductSettingsTests)
set_tests_properties(BilliardsProductSettingsTests PROPERTIES
    LABELS "settings;input;accessibility;phase5"
)

add_executable(BilliardsSettingsMenuModelTests
    tests/settings_menu_model_tests.cpp
    src/Billiards/settings_menu_model.cpp
    src/Billiards/product_settings.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsSettingsMenuModelTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsSettingsMenuModelTests)
set_tests_properties(BilliardsSettingsMenuModelTests PROPERTIES
    LABELS "settings;input;accessibility;ui;phase5"
)

add_executable(BilliardsProductSettingsRuntimeTests
    tests/product_settings_runtime_tests.cpp
    src/Billiards/product_settings_runtime.cpp
    src/Billiards/settings_menu_model.cpp
    src/Billiards/product_settings.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsProductSettingsRuntimeTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsProductSettingsRuntimeTests)
set_tests_properties(BilliardsProductSettingsRuntimeTests PROPERTIES
    LABELS "settings;input;accessibility;ui;runtime;phase5"
)

add_executable(BilliardsRulesRuntimeIntegrationTests
    tests/rules_runtime_integration_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsRulesRuntimeIntegrationTests)
set_tests_properties(BilliardsRulesRuntimeIntegrationTests PROPERTIES
    LABELS "rules;automation;phase5"
)

add_executable(BilliardsRefereeAuthorityProtocolTests
    tests/referee_authority_protocol_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
)

add_executable(BilliardsStalemateAuthorityTests
    tests/stalemate_authority_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/stalemate_authority.cpp
)
add_executable(BilliardsGroupConfusionAuthorityTests
    tests/group_confusion_authority_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/group_confusion_authority.cpp
)
add_executable(BilliardsFrozenBallDeclarationTests
    tests/frozen_ball_declaration_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/frozen_ball_declaration.cpp
)
target_include_directories(BilliardsFrozenBallDeclarationTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsFrozenBallDeclarationTests)
set_tests_properties(BilliardsFrozenBallDeclarationTests PROPERTIES LABELS "rules;authority;referee;frozen-ball;headless;phase5")
add_executable(BilliardsFrozenCushionDeclarationTests
    tests/frozen_cushion_declaration_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/frozen_cushion_declaration.cpp
)
target_include_directories(BilliardsFrozenCushionDeclarationTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsFrozenCushionDeclarationTests)
set_tests_properties(BilliardsFrozenCushionDeclarationTests PROPERTIES LABELS "rules;authority;referee;frozen-cushion;headless;phase5")
add_executable(BilliardsDoubleHitCloseCaseDeclarationTests
    tests/double_hit_close_case_declaration_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/double_hit_close_case_declaration.cpp
)
target_include_directories(BilliardsDoubleHitCloseCaseDeclarationTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsDoubleHitCloseCaseDeclarationTests)
set_tests_properties(BilliardsDoubleHitCloseCaseDeclarationTests PROPERTIES LABELS "rules;authority;referee;double-hit-close-case;headless;phase5")
add_executable(BilliardsPushReviewDeclarationTests
    tests/push_review_declaration_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/push_review_declaration.cpp
)
target_include_directories(BilliardsPushReviewDeclarationTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsPushReviewDeclarationTests)
set_tests_properties(BilliardsPushReviewDeclarationTests PROPERTIES LABELS "rules;authority;referee;push-review;headless;phase5")
add_executable(BilliardsUnintentionalBreakMiscueDeclarationTests
    tests/unintentional_break_miscue_declaration_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/unintentional_break_miscue_declaration.cpp
)
target_include_directories(BilliardsUnintentionalBreakMiscueDeclarationTests
    PRIVATE src/Billiards)
billiardgl_add_test(BilliardsUnintentionalBreakMiscueDeclarationTests)
set_tests_properties(BilliardsUnintentionalBreakMiscueDeclarationTests
    PROPERTIES LABELS
    "rules;authority;referee;break;miscue;headless;phase5")
add_executable(BilliardsNonphysicalGeneralFoulAuthorityTests
    tests/nonphysical_general_foul_authority_tests.cpp
    src/Billiards/rules/referee_authority_protocol.cpp
    src/Billiards/rules/nonphysical_general_foul_authority.cpp)
target_include_directories(BilliardsNonphysicalGeneralFoulAuthorityTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsNonphysicalGeneralFoulAuthorityTests)
set_tests_properties(BilliardsNonphysicalGeneralFoulAuthorityTests PROPERTIES LABELS "rules;authority;referee;general-foul;headless;phase5")
target_include_directories(BilliardsGroupConfusionAuthorityTests PRIVATE
    src/Billiards)
billiardgl_add_test(BilliardsGroupConfusionAuthorityTests)
set_tests_properties(BilliardsGroupConfusionAuthorityTests PROPERTIES
    LABELS "rules;authority;referee;group-confusion;headless;phase5"
)
target_include_directories(BilliardsStalemateAuthorityTests PRIVATE
    src/Billiards)
billiardgl_add_test(BilliardsStalemateAuthorityTests)
set_tests_properties(BilliardsStalemateAuthorityTests PROPERTIES
    LABELS "rules;authority;referee;stalemate;headless;phase5"
)
target_include_directories(BilliardsRefereeAuthorityProtocolTests PRIVATE
    src/Billiards)
billiardgl_add_test(BilliardsRefereeAuthorityProtocolTests)
set_tests_properties(BilliardsRefereeAuthorityProtocolTests PROPERTIES
    LABELS "rules;authority;referee;headless;phase5"
)

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
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAutomationControllerTests)

add_executable(BilliardsAutomationPhysicsScenarioTests
    tests/automation_physics_scenarios_tests.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/automation_protocol.cpp
    src/Billiards/automation_controller.cpp
    src/Billiards/physics_scenario.cpp
    src/Billiards/render_state.cpp
    ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
    ${BILLIARDGL_CORE_SOURCES}
)
billiardgl_configure_core_test(BilliardsAutomationPhysicsScenarioTests)

add_executable(BilliardsLaunchOptionsTests
    tests/launch_options_tests.cpp
    src/Billiards/launch_options.cpp
)
target_include_directories(BilliardsLaunchOptionsTests PRIVATE src/Billiards)
billiardgl_add_test(BilliardsLaunchOptionsTests)

add_test(NAME BilliardsProductFramePerformanceReportTests
    COMMAND ${Python3_EXECUTABLE}
        ${CMAKE_CURRENT_SOURCE_DIR}/tests/test_product_frame_performance_report.py)
set_tests_properties(BilliardsProductFramePerformanceReportTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    TIMEOUT 30
    LABELS "cpu;headless;performance;product-frame"
)

add_test(NAME BilliardsNativePixelPpmReaderTests
    COMMAND ${Python3_EXECUTABLE}
        -m unittest discover -s tests -p "test_native_product_pixel_matrix_*.py")
set_tests_properties(BilliardsNativePixelPpmReaderTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    TIMEOUT 30
    LABELS "cpu;headless;pixel-matrix;parser;geometry"
)

add_test(NAME BilliardsHeadlessAutomationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_headless_automation.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsHeadlessAutomationE2E PROPERTIES TIMEOUT 30)
add_test(NAME BilliardsRendererSelectionE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_renderer_selection.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsRendererSelectionE2E PROPERTIES TIMEOUT 30 LABELS "e2e;renderer-selection")
add_test(NAME BilliardsVisualSceneRegistryTests
    COMMAND ${Python3_EXECUTABLE} -m unittest tests.visual.test_visual_scene_registry)
set_tests_properties(BilliardsVisualSceneRegistryTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    TIMEOUT 30
    LABELS "visual;registry"
)
add_test(NAME BilliardsLookdevManifestTests
    COMMAND ${Python3_EXECUTABLE} -m unittest tests.visual.test_lookdev_manifest)
set_tests_properties(BilliardsLookdevManifestTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    TIMEOUT 30
    LABELS "visual;asset-validation"
)
add_test(NAME BilliardsM1GrayboxGltfTests
    COMMAND ${Python3_EXECUTABLE} -m unittest tests.visual.test_m1_graybox_gltf)
set_tests_properties(BilliardsM1GrayboxGltfTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    TIMEOUT 60
    LABELS "visual;asset-validation"
)
add_test(NAME BilliardsPhase2ModularGltfTests
    COMMAND ${Python3_EXECUTABLE} -m unittest tests.visual.test_phase2_modular_gltf)
set_tests_properties(BilliardsPhase2ModularGltfTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    # Full candidate generation and byte-determinism checks now exceed one
    # minute even without contention. This is a harness limit, not a render
    # performance budget; keep the geometry and determinism assertions intact.
    TIMEOUT 180
    LABELS "visual;asset-validation"
)
add_test(NAME BilliardsProductAcceptanceCaptureTests
    COMMAND ${Python3_EXECUTABLE} -m unittest tests.visual.test_product_acceptance_capture)
set_tests_properties(BilliardsProductAcceptanceCaptureTests PROPERTIES
    WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}"
    TIMEOUT 30 LABELS "visual;capture;cpu")
add_test(NAME BilliardsPhysicsValidationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_physics_validation.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsPhysicsValidationE2E PROPERTIES TIMEOUT 60 LABELS "physics-validation;e2e")
add_test(NAME BilliardsReferenceValidationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_reference_validation.py
        $<TARGET_FILE:Billiards> ${CMAKE_CURRENT_BINARY_DIR}/physics-reference-validation)
set_tests_properties(BilliardsReferenceValidationE2E PROPERTIES TIMEOUT 60 LABELS "physics-validation;reference-validation;e2e")
add_test(NAME BilliardsPhase3PhysicsReleaseGate
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/scripts/check_phase3_physics_release.py
        --root ${CMAKE_CURRENT_SOURCE_DIR}
        --executable $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsPhase3PhysicsReleaseGate PROPERTIES
    TIMEOUT 30 LABELS "physics-validation;release")
add_test(NAME BilliardsRenderedAutomationE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_rendered_automation.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsRenderedAutomationE2E PROPERTIES TIMEOUT 30 LABELS "rendered;e2e")
add_test(NAME BilliardsBreakCameraE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_break_camera.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsBreakCameraE2E PROPERTIES TIMEOUT 30 LABELS "physics-validation;rendered;e2e")
add_test(NAME BilliardsVisualBaselinesE2E
    COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_visual_baselines.py $<TARGET_FILE:Billiards>)
set_tests_properties(BilliardsVisualBaselinesE2E PROPERTIES
    TIMEOUT 480
    LABELS "visual;rendered;e2e"
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
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
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
        TIMEOUT 30
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
    Freetype::Freetype
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

# Modern renderer foundation. The Apple production executable links this
# library; V19 inset implementation objects are supplied by the dedicated
# target-closure dependency.
add_library(BilliardsModernGpuResources STATIC
    src/Billiards/deterministic_hash.cpp
    src/Billiards/render/modern/lookdev_settings.cpp
    src/Billiards/render/modern/modern_renderer.cpp
    src/Billiards/render/modern/modern_renderer_product_ui.cpp
    src/Billiards/render/modern/gpu_resources.cpp
    src/Billiards/render/modern/pbr_materials.cpp
    src/Billiards/render/modern/pbr_texture_resources.cpp
    src/Billiards/render/modern/shader_diagnostics.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsModernGpuResources PUBLIC src/Billiards)
target_link_libraries(BilliardsModernGpuResources PUBLIC
    BilliardsModernProductUiRuntime
    BilliardsProductUiOpenGL41
    OpenGL::GL
)
target_link_libraries(BilliardsModernGpuResources PRIVATE
    BilliardsPhase4V19TrajectoryInsetImplementation
    BilliardsPhase4TrajectoryOverlayImplementation
)
target_compile_definitions(BilliardsModernGpuResources PRIVATE
    $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>
)

add_library(BilliardsGltfGpuBridge STATIC
    src/Billiards/render/gltf/gltf_importer.cpp
    src/Billiards/render/gltf/gltf_draw_package.cpp
    src/Billiards/render/gltf/gltf_gpu_upload.cpp
    src/Billiards/automation_json.cpp
)
target_include_directories(BilliardsGltfGpuBridge PUBLIC src/Billiards)
target_link_libraries(BilliardsGltfGpuBridge PUBLIC BilliardsModernGpuResources OpenGL::GL)
target_compile_definitions(BilliardsGltfGpuBridge PRIVATE
    $<$<PLATFORM_ID:Darwin>:GL_SILENCE_DEPRECATION>
)

add_executable(BilliardsPhase4V19ModernRendererCp4FinalLinkTests
    tests/phase4_v19_modern_renderer_cp4_final_link_tests.cpp
)
target_include_directories(BilliardsPhase4V19ModernRendererCp4FinalLinkTests
    PRIVATE src/Billiards)
target_link_libraries(BilliardsPhase4V19ModernRendererCp4FinalLinkTests PRIVATE
    BilliardsModernGpuResources
    BilliardsGltfGpuBridge
)
add_test(NAME BilliardsPhase4V19ModernRendererCp4FinalLinkTests
    COMMAND BilliardsPhase4V19ModernRendererCp4FinalLinkTests)
set_tests_properties(BilliardsPhase4V19ModernRendererCp4FinalLinkTests PROPERTIES
    TIMEOUT 30
    LABELS "trajectory;inset;modern-renderer;final-link;headless;cpu;phase4"
)

# Pure C++ contract/diagnostic tests: no OpenGL library or context is required.
add_executable(BilliardsModernRenderFoundationTests
    tests/modern_render_foundation_tests.cpp
    src/Billiards/render/modern/formal_asset_attestation.cpp
    src/Billiards/render/modern/lookdev_settings.cpp
    src/Billiards/render/modern/pbr_materials.cpp
    src/Billiards/render/modern/shader_diagnostics.cpp
    src/Billiards/automation_json.cpp
    src/Billiards/deterministic_hash.cpp
)
target_include_directories(BilliardsModernRenderFoundationTests PRIVATE src/Billiards)
target_compile_definitions(BilliardsModernRenderFoundationTests PRIVATE
    BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
)
billiardgl_add_test(BilliardsModernRenderFoundationTests)

if(APPLE)
    target_compile_definitions(Billiards PRIVATE GL_SILENCE_DEPRECATION)
    target_link_libraries(Billiards PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        "-framework Cocoa"
        "-framework OpenGL"
    )

    # Non-shipping migration rehearsal only. Explicitly build this candidate;
    # the Billiards entry, defaults and every existing test binding stay intact.
    add_executable(BilliardsModernProductionCandidate EXCLUDE_FROM_ALL
        src/Billiards/product_main.cpp
        src/Billiards/render_state.cpp
        src/Billiards/platform_audio.cpp
        src/Billiards/automation_json.cpp
        src/Billiards/automation_protocol.cpp
        src/Billiards/automation_controller.cpp
        src/Billiards/physics_scenario.cpp
        src/Billiards/automation_runner.cpp
        src/Billiards/stdio_transport.cpp
        src/Billiards/launch_options.cpp
        src/Billiards/render/modern/modern_application_mac.mm
        src/Billiards/render/modern/formal_asset_attestation.cpp
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PRODUCT_HUD_SOURCES}
        ${BILLIARDGL_PRODUCT_UI_RENDER_SOURCES}
        src/Billiards/render/ui/product_ui_frame_coordinator.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        ${BILLIARDGL_PRODUCT_SHELL_SOURCES}
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_include_directories(BilliardsModernProductionCandidate PRIVATE src/Billiards)
    target_compile_definitions(BilliardsModernProductionCandidate PRIVATE
        BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
        GL_SILENCE_DEPRECATION
    )
    target_link_libraries(BilliardsModernProductionCandidate PRIVATE
        BilliardsModernGpuResources BilliardsGltfGpuBridge OpenGL::GL
        Threads::Threads Freetype::Freetype PkgConfig::SDL2_AUDIO
        "-framework Cocoa" "-framework OpenGL"
    )

    add_test(NAME BilliardsModernRendererProductionE2E
        COMMAND ${Python3_EXECUTABLE} ${CMAKE_CURRENT_SOURCE_DIR}/tests/e2e/test_modern_renderer_production.py $<TARGET_FILE:Billiards>)
    set_tests_properties(BilliardsModernRendererProductionE2E PROPERTIES
        TIMEOUT 120
        LABELS "rendered;e2e;modern-application"
    )

    # Phase 0 only: an isolated proof that the deployment target can create a
    # macOS OpenGL 4.1 Core context. It intentionally has no dependency on the
    # game, simulation core, legacy renderer, or production screenshot path.
    add_executable(BilliardsRenderCoreProfileSpike
        spikes/render_core_profile/main.mm
    )
    target_compile_definitions(BilliardsRenderCoreProfileSpike PRIVATE
        GL_SILENCE_DEPRECATION
    )
    target_link_libraries(BilliardsRenderCoreProfileSpike PRIVATE
        "-framework Cocoa"
        "-framework OpenGL"
    )

    set(BILLIARDGL_RENDER_SPIKE_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/test-output/render-core-profile-spike.ppm")
    add_test(NAME BilliardsRenderCoreProfileSpikeScreenshot
        COMMAND BilliardsRenderCoreProfileSpike
            --screenshot "${BILLIARDGL_RENDER_SPIKE_OUTPUT}")
    set_tests_properties(BilliardsRenderCoreProfileSpikeScreenshot PROPERTIES
        TIMEOUT 20
        LABELS "rendered;spike"
    )

    add_executable(BilliardsModernGpuResourcesSmoke
        spikes/render_core_profile/modern_resources_smoke.mm
    )
    target_compile_definitions(BilliardsModernGpuResourcesSmoke PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${CMAKE_CURRENT_SOURCE_DIR}"
    )
    target_link_libraries(BilliardsModernGpuResourcesSmoke PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        "-framework Cocoa"
        "-framework OpenGL"
    )
    add_test(NAME BilliardsModernGpuResourcesSmoke
        COMMAND BilliardsModernGpuResourcesSmoke)
    set_tests_properties(BilliardsModernGpuResourcesSmoke PROPERTIES
        TIMEOUT 20
        LABELS "rendered;smoke"
    )

    add_executable(BilliardsProductUiGpuSmoke
        spikes/render_core_profile/product_ui_gpu_smoke.mm
        src/Billiards/render/ui/product_shell_draw_list.cpp
        src/Billiards/render/ui/product_shell_layout.cpp
        src/Billiards/render/ui/hud_shipping_tokens.cpp
        src/Billiards/automation_json.cpp
        src/Billiards/deterministic_hash.cpp
        src/Billiards/render/ui/sdf_font_atlas.cpp
        src/Billiards/render/ui/sdf_generator.cpp
    )
    target_compile_definitions(BilliardsProductUiGpuSmoke PRIVATE
        GL_SILENCE_DEPRECATION
    )
    target_include_directories(BilliardsProductUiGpuSmoke PRIVATE src/Billiards)
    target_link_libraries(BilliardsProductUiGpuSmoke PRIVATE
        BilliardsProductUiOpenGL41
        Freetype::Freetype
        "-framework Cocoa"
        "-framework OpenGL"
    )
    add_test(NAME BilliardsProductUiGpuSmoke COMMAND BilliardsProductUiGpuSmoke)
    set_tests_properties(BilliardsProductUiGpuSmoke PROPERTIES
        TIMEOUT 20
        LABELS "rendered;smoke;ui;gpu-submit;core41;phase3"
    )

    add_executable(BilliardsModernProductUiIntegrationSmoke
        spikes/render_core_profile/modern_product_ui_integration_smoke.mm
        src/Billiards/automation_json.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(BilliardsModernProductUiIntegrationSmoke PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    target_link_libraries(BilliardsModernProductUiIntegrationSmoke PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )
    set(BILLIARDGL_MODERN_PRODUCT_UI_SMOKE_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/test-output/product-ui/modern-product-main-default.ppm")
    add_test(NAME BilliardsModernProductUiIntegrationSmoke
        COMMAND BilliardsModernProductUiIntegrationSmoke
            "${BILLIARDGL_MODERN_PRODUCT_UI_SMOKE_OUTPUT}")
    set_tests_properties(BilliardsModernProductUiIntegrationSmoke PROPERTIES
        TIMEOUT 30
        LABELS "rendered;smoke;ui;modern-renderer;product-branch;core41;phase3"
    )

    add_executable(BilliardsHighReplayDofConsumerSmoke
        spikes/render_core_profile/high_replay_dof_consumer_smoke.mm
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(BilliardsHighReplayDofConsumerSmoke PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    target_link_libraries(BilliardsHighReplayDofConsumerSmoke PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )
    set(BILLIARDGL_HIGH_REPLAY_DOF_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/test-output/high-replay-dof")
    add_test(NAME BilliardsHighReplayDofConsumerSmoke
        COMMAND BilliardsHighReplayDofConsumerSmoke
            "${BILLIARDGL_HIGH_REPLAY_DOF_OUTPUT}")
    set_tests_properties(BilliardsHighReplayDofConsumerSmoke PROPERTIES
        TIMEOUT 120
        RUN_SERIAL TRUE
        LABELS "rendered;smoke;replay;dof;performance;core41;phase3"
    )

    add_executable(BilliardsPhase3GpuCaptureRunner
        spikes/render_core_profile/phase3_gpu_capture_runner.mm
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(BilliardsPhase3GpuCaptureRunner PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    target_link_libraries(BilliardsPhase3GpuCaptureRunner PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )

    add_executable(BilliardsPhase4OverlayGpuCaptureRunner
        spikes/render_core_profile/phase4_overlay_gpu_capture_runner.mm
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(BilliardsPhase4OverlayGpuCaptureRunner PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    target_link_libraries(BilliardsPhase4OverlayGpuCaptureRunner PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )

    set(BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE
        "${CMAKE_CURRENT_SOURCE_DIR}/spikes/render_core_profile/phase5_rules_decision_ui_gpu_capture_runner.mm")
    file(SHA256 "${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE}"
        BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA)
    add_executable(BilliardsPhase5RulesDecisionUiGpuCaptureRunner
        "${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE}"
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(BilliardsPhase5RulesDecisionUiGpuCaptureRunner PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
        BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA="${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA}"
    )
    target_link_libraries(BilliardsPhase5RulesDecisionUiGpuCaptureRunner PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )

    add_executable(BilliardsPhase5RulesDecisionUiV3TargetedGpuCaptureRunner
        "${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE}"
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(
        BilliardsPhase5RulesDecisionUiV3TargetedGpuCaptureRunner PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
        BILLIARDGL_PHASE5_RULES_UI_V3_TARGETED=1
        BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA="${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA}"
    )
    target_link_libraries(
        BilliardsPhase5RulesDecisionUiV3TargetedGpuCaptureRunner PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )

    add_executable(BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner
        "${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE}"
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(
        BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
        BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1
        BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA="${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA}"
    )
    target_link_libraries(
        BilliardsPhase5RulesDecisionUiV4Exact16GpuCaptureRunner PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )

    add_executable(BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner
        "${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE}"
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(
        BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
        BILLIARDGL_PHASE5_RULES_UI_V4_EXACT16=1
        BILLIARDGL_PHASE5_RULES_UI_V5_EXACT16=1
        BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA="${BILLIARDGL_PHASE5_RULES_UI_RUNNER_SOURCE_SHA}"
    )
    target_link_libraries(
        BilliardsPhase5RulesDecisionUiV5Exact16GpuCaptureRunner PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )

    option(BILLIARDGL_PHASE4_FINAL_OWNERSHIP_RUNNER
        "Generate the capture-only integrated final ownership runner" OFF)
    set(BILLIARDGL_PHASE4_V4_GPU_RUNNER_SOURCE
        "${CMAKE_CURRENT_BINARY_DIR}/generated/phase4_overlay_v4_gpu_capture_runner.mm")
    set(BILLIARDGL_PHASE4_V4_GPU_RUNNER_ARGS)
    if(BILLIARDGL_PHASE4_FINAL_OWNERSHIP_RUNNER)
        list(APPEND BILLIARDGL_PHASE4_V4_GPU_RUNNER_ARGS --final-ownership)
    endif()
    add_custom_command(
        OUTPUT "${BILLIARDGL_PHASE4_V4_GPU_RUNNER_SOURCE}"
        COMMAND ${Python3_EXECUTABLE}
            "${CMAKE_CURRENT_SOURCE_DIR}/scripts/generate_phase4_overlay_v4_gpu_runner.py"
            ${BILLIARDGL_PHASE4_V4_GPU_RUNNER_ARGS}
            --output "${BILLIARDGL_PHASE4_V4_GPU_RUNNER_SOURCE}"
        DEPENDS
            scripts/generate_phase4_overlay_v4_gpu_runner.py
            spikes/render_core_profile/phase4_overlay_gpu_capture_runner.mm
            spikes/render_core_profile/phase4_overlay_v4_gpu_capture_support.inc
            src/Billiards/training/final_semantic_ownership.cpp
            src/Billiards/training/final_semantic_ownership.h
            src/Billiards/training/phase4_final_ownership_instrumentation.cpp
            src/Billiards/training/phase4_final_ownership_instrumentation.h
        VERBATIM
    )
    add_executable(BilliardsPhase4OverlayV4GpuCaptureRunner
        "${BILLIARDGL_PHASE4_V4_GPU_RUNNER_SOURCE}"
        src/Billiards/automation_json.cpp
        src/Billiards/training/phase4_final_ownership_instrumentation.cpp
        src/Billiards/render_state.cpp
        src/Billiards/physics_scenario.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_compile_definitions(BilliardsPhase4OverlayV4GpuCaptureRunner PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    target_link_libraries(BilliardsPhase4OverlayV4GpuCaptureRunner PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )

    add_executable(BilliardsGltfGpuUploadSmoke
        spikes/render_core_profile/gltf_gpu_upload_smoke.mm
    )
    target_compile_definitions(BilliardsGltfGpuUploadSmoke PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    target_link_libraries(BilliardsGltfGpuUploadSmoke PRIVATE
        BilliardsGltfGpuBridge
        "-framework Cocoa"
        "-framework OpenGL"
    )
    add_test(NAME BilliardsGltfGpuUploadSmoke COMMAND BilliardsGltfGpuUploadSmoke)
    set_tests_properties(BilliardsGltfGpuUploadSmoke PROPERTIES
        TIMEOUT 30
        LABELS "rendered;smoke;gltf;gpu-upload"
    )

    add_executable(BilliardsModernRendererSliceSmoke
        spikes/render_core_profile/modern_renderer_slice_smoke.mm
    )
    target_compile_definitions(BilliardsModernRendererSliceSmoke PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
    )
    target_link_libraries(BilliardsModernRendererSliceSmoke PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        "-framework Cocoa"
        "-framework OpenGL"
    )
    set(BILLIARDGL_MODERN_SLICE_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/test-output/modern-slice")
    add_test(NAME BilliardsModernRendererSliceSmoke
        COMMAND BilliardsModernRendererSliceSmoke "${BILLIARDGL_MODERN_SLICE_OUTPUT}")
    set_tests_properties(BilliardsModernRendererSliceSmoke PROPERTIES
        TIMEOUT 30
        LABELS "rendered;smoke;modern-slice"
    )
    add_test(NAME BilliardsModernRendererSeamlessCubemapState
        COMMAND BilliardsModernRendererSliceSmoke
            "${CMAKE_CURRENT_BINARY_DIR}/test-output/modern-seamless-state")
    set_tests_properties(BilliardsModernRendererSeamlessCubemapState PROPERTIES
        TIMEOUT 30
        LABELS "rendered;smoke;modern-slice;gl-state"
        ENVIRONMENT "BILLIARDGL_SEAMLESS_STATE_ONLY=1"
    )
    add_test(NAME BilliardsModernRendererGltfShadow
        COMMAND BilliardsModernRendererSliceSmoke
            "${CMAKE_CURRENT_BINARY_DIR}/test-output/modern-gltf-shadow")
    set_tests_properties(BilliardsModernRendererGltfShadow PROPERTIES
        TIMEOUT 30
        LABELS "rendered;smoke;modern-slice;gltf;shadow"
        ENVIRONMENT "BILLIARDGL_GLTF_SHADOW_ONLY=1"
    )

    add_executable(BilliardsModernApplicationE2E
        tests/modern_application_e2e.mm
        src/Billiards/render/modern/modern_application_mac.mm
        src/Billiards/render/modern/formal_asset_attestation.cpp
        src/Billiards/automation_json.cpp
        src/Billiards/render_state.cpp
        src/Billiards/automation_protocol.cpp
        src/Billiards/automation_controller.cpp
        src/Billiards/automation_runner.cpp
        src/Billiards/stdio_transport.cpp
        src/Billiards/physics_scenario.cpp
        src/Billiards/platform_audio.cpp
        ${BILLIARDGL_PRODUCT_AUDIO_SOURCES}
        ${BILLIARDGL_PRODUCT_INPUT_SOURCES}
        src/Billiards/product_shell_controller.cpp
        ${BILLIARDGL_MODERN_PRODUCT_LIFECYCLE_SOURCES}
        ${BILLIARDGL_PRODUCT_AI_SOURCES}
        ${BILLIARDGL_PHASE4_SYSTEM_SOURCES}
        ${BILLIARDGL_CORE_SOURCES}
    )
    target_include_directories(BilliardsModernApplicationE2E PRIVATE src/Billiards)
    target_compile_definitions(BilliardsModernApplicationE2E PRIVATE
        GL_SILENCE_DEPRECATION
        BILLIARDGL_VISIBLE_DRAWABLE_SELF_TEST=1
        BILLIARDGL_SOURCE_ROOT="${BILLIARDGL_SOURCE_ROOT}"
        BILLIARDGL_ASSET_ROOT="${BILLIARDGL_ASSET_ROOT}"
    )
    target_link_libraries(BilliardsModernApplicationE2E PRIVATE
        BilliardsModernGpuResources
        BilliardsGltfGpuBridge
        Threads::Threads
        PkgConfig::SDL2_AUDIO
        "-framework Cocoa"
        "-framework OpenGL"
    )
    set(BILLIARDGL_MODERN_APP_OUTPUT
        "${CMAKE_CURRENT_BINARY_DIR}/test-output/modern-application")
    add_test(NAME BilliardsModernApplicationE2E
        COMMAND BilliardsModernApplicationE2E "${BILLIARDGL_MODERN_APP_OUTPUT}")
    set_tests_properties(BilliardsModernApplicationE2E PROPERTIES
        TIMEOUT 120
        LABELS "rendered;e2e;modern-application"
    )
    add_test(NAME BilliardsModernVisibleDrawableE2E
        COMMAND BilliardsModernApplicationE2E
            "${BILLIARDGL_MODERN_APP_OUTPUT}/visible-drawable"
            --visible-drawable)
    set_tests_properties(BilliardsModernVisibleDrawableE2E PROPERTIES
        TIMEOUT 30
        LABELS "rendered;e2e;modern-application;visible-drawable"
    )
    add_test(NAME BilliardsNativeProductPixelMatrix
        COMMAND ${Python3_EXECUTABLE}
            "${BILLIARDGL_SOURCE_ROOT}/scripts/run_native_product_pixel_matrix.py"
            --binary $<TARGET_FILE:BilliardsModernApplicationE2E>
            --output
                "${CMAKE_CURRENT_BINARY_DIR}/test-output/native-product-pixel-matrix")
    set_tests_properties(BilliardsNativeProductPixelMatrix PROPERTIES
        TIMEOUT 180
        RUN_SERIAL TRUE
        LABELS "rendered;e2e;modern-application;visible-drawable;pixel-matrix;hud;trajectory;cvd"
    )
    add_test(NAME BilliardsModernProductFramePerformanceE2E
        COMMAND ${Python3_EXECUTABLE}
            "${BILLIARDGL_SOURCE_ROOT}/scripts/run_product_frame_performance.py"
            --binary $<TARGET_FILE:Billiards>
            --output
                "${CMAKE_CURRENT_BINARY_DIR}/test-output/product-frame-performance")
    set_tests_properties(BilliardsModernProductFramePerformanceE2E PROPERTIES
        TIMEOUT 180
        RUN_SERIAL TRUE
        LABELS "rendered;e2e;performance;product-frame;visible-drawable"
    )
endif()

add_executable(BilliardsPhysicsTelemetryExactHelperTests
    tests/physics_telemetry_exact_test_helpers_tests.cpp)
target_include_directories(BilliardsPhysicsTelemetryExactHelperTests PRIVATE src/Billiards)
add_test(NAME BilliardsPhysicsTelemetryExactHelperTests
    COMMAND BilliardsPhysicsTelemetryExactHelperTests)
set_tests_properties(BilliardsPhysicsTelemetryExactHelperTests PROPERTIES
    LABELS "cpu;headless;physics" TIMEOUT 30)
