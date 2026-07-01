# BilliardGL Cross-Platform Migration Design

Date: 2026-07-01

## Goal

Migrate BilliardGL from a Visual Studio/Win32-only project to a project that can compile and run on Linux and macOS.

The first phase prioritizes a low-risk port: keep the existing FreeGLUT/GLEW/OpenGL rendering path and game logic, add a cross-platform build, and isolate the Windows-only pieces. A later phase can modernize the windowing, input, audio, and rendering structure.

## Current State

The project is a C++ OpenGL billiards game with these main source files:

- `src/Billiards/billiards.cpp`
- `src/Billiards/ObjLoader.cpp`
- `src/Billiards/particle.cpp`
- `src/Billiards/vec.cpp`

The current build path is Visual Studio:

- `src/Billiards.sln`
- `src/Billiards/Billiards.vcxproj`

The repository vendors Windows-oriented dependencies and release artifacts, including `.lib`, `.dll`, and `.exe` files. Runtime assets live under:

- `src/Billiards/tex`
- `src/Billiards/obj`
- `src/Billiards/audio`

Important Windows-specific code exists in `billiards.cpp`:

- `#include <windows.h>`
- `#include <mmsystem.h>`
- MSVC `#pragma comment(...)`
- `PlaySound(...)`
- `mciSendString(...)`
- `Sleep(...)`
- `CreateFontA(...)`, `HFONT`, `wglGetCurrentDC(...)`
- `fopen_s(...)`
- `_itoa_s(...)`

There are also cross-platform include issues such as `<gl/glut.h>`, which is not portable across Linux and macOS.

## Recommended Approach

Use a two-stage migration.

Phase 1 is the current scope. It adds a CMake-based cross-platform build and applies small compatibility changes while preserving the existing rendering and gameplay code.

Phase 2 is out of scope for the first implementation. It can move the project toward a more modern cross-platform structure, such as GLFW or SDL2 for windowing/input, a dedicated audio library, and cleaner separation between rendering and game logic.

## Phase 1 Architecture

### Build System

Add a root `CMakeLists.txt` that builds the game from the existing source files:

- `billiards.cpp`
- `ObjLoader.cpp`
- `particle.cpp`
- `vec.cpp`

The CMake build should discover and link platform libraries:

- OpenGL
- GLU, where required by the existing code
- GLEW
- FreeGLUT

The existing Visual Studio files remain in the repository, but CMake becomes the migration path for Linux and macOS.

### Platform Compatibility Layer

Add small platform wrappers instead of spreading `#ifdef` blocks throughout the code.

`platform_time.h` provides sleep behavior using standard C++:

- Replace `Sleep(1000)` with a wrapper based on `std::this_thread::sleep_for`.

`platform_audio.h` and `platform_audio.cpp` provide game-level sound functions:

- `playBackgroundLoop()`
- `playHit()`
- `playBallIn()`
- `playGameOver()`

For Phase 1, non-Windows audio can be a no-op implementation. This keeps the game compiling and running while isolating audio as a later improvement. Windows can either keep the current WinMM behavior behind the wrapper or also use the no-op temporarily if preserving Visual Studio builds is not part of the immediate verification.

### Resource Paths

Add a small resource path helper:

- `resource_path.h`
- `resource_path.cpp`

The helper should build paths for:

- `tex/...`
- `obj/...`
- `audio/...`

This removes the assumption that the program is launched from `src/Billiards`. The executable should be able to run from the CMake build directory and still locate textures, OBJ/MTL files, and audio files.

The helper can use a configured asset root from CMake for Phase 1. That is enough for local development and avoids introducing installer/package complexity.

### Source Compatibility Changes

Keep changes in `billiards.cpp`, `ObjLoader.cpp`, and `particle.h` narrowly scoped.

Required changes:

- Remove or conditionally compile Windows-only headers.
- Remove MSVC `#pragma comment(...)` from the cross-platform build path.
- Replace `PlaySound(...)` and `mciSendString(...)` with platform audio wrapper calls.
- Replace `Sleep(...)` with the platform time wrapper.
- Replace `fopen_s(...)` with portable C or C++ file opening.
- Replace `_itoa_s(...)` with a portable conversion.
- Remove or conditionally compile the unused Win32 font selection function.
- Normalize OpenGL/GLUT include usage so it works on Linux and macOS.
- Use the resource path helper for OBJ, MTL, texture, and audio asset paths.

Rendering code, physics, input callbacks, particle behavior, and OBJ parsing behavior should remain functionally unchanged in Phase 1.

## Out of Scope

The first phase does not include:

- Replacing FreeGLUT with GLFW or SDL2.
- Replacing GLEW with glad.
- Rewriting rendering to modern OpenGL.
- Replacing fixed-function OpenGL or GLU usage.
- Refactoring `billiards.cpp` into a full engine-style architecture.
- Implementing cross-platform audio playback on Linux/macOS.
- Removing release binaries or vendored Windows libraries.
- Guaranteeing continued Visual Studio project compatibility.

## Verification

Phase 1 is complete when these checks pass:

- Linux can configure and build with:
  - `cmake -S . -B build`
  - `cmake --build build`
- macOS can configure and build with the same CMake commands after installing dependencies through Homebrew or the system package manager.
- The executable can be launched from the CMake build directory.
- The game can create an OpenGL window and enter the GLUT main loop.
- OBJ models and BMP textures load from the configured resource root.
- Audio calls on non-Windows platforms do not crash, even if they do not play sound.

## Risks

macOS deprecates OpenGL and GLUT. Phase 1 accepts deprecation warnings as long as the game builds and runs.

The existing code uses GLU and fixed-function OpenGL. Driver differences may still cause rendering differences after compilation succeeds.

Resource lookup is likely to be the most common runtime failure after the build is fixed. The resource path helper is required, not optional.

Audio no-op behavior reduces feature completeness, but it keeps the first migration focused on graphics and build portability.

## Implementation Sequence

1. Add CMake build configuration.
2. Add resource path helper and configure the asset root.
3. Add platform time and platform audio wrappers.
4. Update includes and remove MSVC-only build directives from the portable build path.
5. Replace Windows-only API calls with wrappers or portable equivalents.
6. Build on Linux and fix compile/link errors.
7. Document macOS dependency setup and expected warnings.

