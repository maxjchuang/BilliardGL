# Cross-Platform Migration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build BilliardGL on Linux and macOS through CMake while preserving the current FreeGLUT/GLEW/OpenGL rendering and gameplay behavior.

**Architecture:** Add a CMake build path and small compatibility helpers for resource lookup, sleep, and audio. Keep the existing GLUT event loop and rendering logic intact, and replace only the Windows-only calls that block cross-platform compilation.

**Tech Stack:** C++11, CMake, OpenGL, GLU, GLEW, FreeGLUT, standard C/C++ file and time APIs.

---

## File Structure

- Create `CMakeLists.txt`: root CMake build for the Billiards executable.
- Create `src/Billiards/platform_time.h`: standard C++ sleep wrapper.
- Create `src/Billiards/platform_audio.h`: sound effect interface used by game logic.
- Create `src/Billiards/platform_audio.cpp`: no-op non-Windows audio and optional WinMM Windows implementation.
- Create `src/Billiards/resource_path.h`: public asset path helpers.
- Create `src/Billiards/resource_path.cpp`: asset root and path construction.
- Modify `src/Billiards/billiards.cpp`: remove direct Win32 usage from portable path, route sound/sleep/assets through helpers, and replace MSVC-only functions.
- Modify `src/Billiards/ObjLoader.cpp`: use the resource helper for material files and portable FreeGLUT include.
- Modify `src/Billiards/particle.h`: normalize GLUT include path.
- Modify `README.md`: add Linux/macOS CMake build instructions.

## Task 1: Add CMake Build Skeleton

**Files:**
- Create: `CMakeLists.txt`

- [ ] **Step 1: Create the CMake file**

Create `CMakeLists.txt` at the repository root with this content:

```cmake
cmake_minimum_required(VERSION 3.16)

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
```

- [ ] **Step 2: Configure to expose current compile blockers**

Run:

```bash
cmake -S . -B build
```

Expected: CMake configures if system packages are installed. If a dependency is missing on Linux, install development packages equivalent to `cmake`, `g++`, `libgl1-mesa-dev`, `libglu1-mesa-dev`, `libglew-dev`, and `freeglut3-dev`. On macOS, install `cmake`, `glew`, and `freeglut` through Homebrew.

- [ ] **Step 3: Build to confirm Windows-specific failures are visible**

Run:

```bash
cmake --build build
```

Expected before later tasks: build fails on Linux/macOS because `windows.h`, `mmsystem.h`, `<gl/glut.h>`, `fopen_s`, `_itoa_s`, `Sleep`, `PlaySound`, and `mciSendString` are still present.

- [ ] **Step 4: Commit**

```bash
git add CMakeLists.txt
git commit -m "add cmake build skeleton"
```

## Task 2: Add Resource Path Helper

**Files:**
- Create: `src/Billiards/resource_path.h`
- Create: `src/Billiards/resource_path.cpp`

- [ ] **Step 1: Create the public resource path header**

Create `src/Billiards/resource_path.h`:

```cpp
#pragma once

#include <string>

namespace billiardgl {

std::string resourcePath(const std::string& relativePath);
std::string texturePath(const std::string& fileName);
std::string objectPath(const std::string& fileName);
std::string audioPath(const std::string& fileName);

}  // namespace billiardgl
```

- [ ] **Step 2: Create the implementation**

Create `src/Billiards/resource_path.cpp`:

```cpp
#include "resource_path.h"

#include <string>

#ifndef BILLIARDGL_ASSET_ROOT
#define BILLIARDGL_ASSET_ROOT "."
#endif

namespace billiardgl {

namespace {

std::string joinPath(const std::string& left, const std::string& right)
{
    if (left.empty()) {
        return right;
    }
    const char last = left[left.size() - 1];
    if (last == '/' || last == '\\') {
        return left + right;
    }
    return left + "/" + right;
}

}  // namespace

std::string resourcePath(const std::string& relativePath)
{
    return joinPath(BILLIARDGL_ASSET_ROOT, relativePath);
}

std::string texturePath(const std::string& fileName)
{
    return resourcePath(joinPath("tex", fileName));
}

std::string objectPath(const std::string& fileName)
{
    return resourcePath(joinPath("obj", fileName));
}

std::string audioPath(const std::string& fileName)
{
    return resourcePath(joinPath("audio", fileName));
}

}  // namespace billiardgl
```

- [ ] **Step 3: Add the helper to CMake**

Modify the `add_executable` source list in `CMakeLists.txt`:

```cmake
add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/vec.cpp
    src/Billiards/resource_path.cpp
)
```

- [ ] **Step 4: Configure**

Run:

```bash
cmake -S . -B build
```

Expected: CMake configuration succeeds if dependencies are present.

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/Billiards/resource_path.h src/Billiards/resource_path.cpp
git commit -m "add resource path helper"
```

## Task 3: Add Platform Time Wrapper

**Files:**
- Create: `src/Billiards/platform_time.h`

- [ ] **Step 1: Create the wrapper header**

Create `src/Billiards/platform_time.h`:

```cpp
#pragma once

#include <chrono>
#include <thread>

namespace billiardgl {

inline void sleepMilliseconds(int milliseconds)
{
    std::this_thread::sleep_for(std::chrono::milliseconds(milliseconds));
}

}  // namespace billiardgl
```

- [ ] **Step 2: Commit**

```bash
git add src/Billiards/platform_time.h
git commit -m "add portable sleep wrapper"
```

## Task 4: Add Platform Audio Wrapper

**Files:**
- Create: `src/Billiards/platform_audio.h`
- Create: `src/Billiards/platform_audio.cpp`
- Modify: `CMakeLists.txt`

- [ ] **Step 1: Create the audio header**

Create `src/Billiards/platform_audio.h`:

```cpp
#pragma once

namespace billiardgl {

void playBackgroundLoop();
void playHit();
void playBallIn();
void playGameOver();

}  // namespace billiardgl
```

- [ ] **Step 2: Create the audio implementation**

Create `src/Billiards/platform_audio.cpp`:

```cpp
#include "platform_audio.h"

#include "resource_path.h"

#include <string>

#if defined(_WIN32)
#include <windows.h>
#include <mmsystem.h>
#endif

namespace billiardgl {

#if defined(_WIN32)

namespace {

void playFileAsync(const char* fileName)
{
    const std::string path = audioPath(fileName);
    PlaySoundA(path.c_str(), NULL, SND_FILENAME | SND_ASYNC);
}

}  // namespace

void playBackgroundLoop()
{
    const std::string path = audioPath("background.wav");
    PlaySoundA(path.c_str(), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
}

void playHit()
{
    playFileAsync("hit.wav");
}

void playBallIn()
{
    playFileAsync("ballin.wav");
}

void playGameOver()
{
    playFileAsync("GameOver.wav");
}

#else

void playBackgroundLoop() {}
void playHit() {}
void playBallIn() {}
void playGameOver() {}

#endif

}  // namespace billiardgl
```

- [ ] **Step 3: Add the implementation to CMake**

Modify the `add_executable` source list in `CMakeLists.txt`:

```cmake
add_executable(Billiards
    src/Billiards/billiards.cpp
    src/Billiards/ObjLoader.cpp
    src/Billiards/particle.cpp
    src/Billiards/vec.cpp
    src/Billiards/resource_path.cpp
    src/Billiards/platform_audio.cpp
)
```

- [ ] **Step 4: Link WinMM only on Windows**

Add this block near the bottom of `CMakeLists.txt`:

```cmake
if(WIN32)
    target_link_libraries(Billiards PRIVATE winmm)
endif()
```

- [ ] **Step 5: Commit**

```bash
git add CMakeLists.txt src/Billiards/platform_audio.h src/Billiards/platform_audio.cpp
git commit -m "add platform audio wrapper"
```

## Task 5: Normalize OpenGL and GLUT Includes

**Files:**
- Modify: `src/Billiards/billiards.cpp`
- Modify: `src/Billiards/ObjLoader.cpp`
- Modify: `src/Billiards/particle.h`

- [ ] **Step 1: Replace includes in `billiards.cpp`**

Replace the include block at the top of `src/Billiards/billiards.cpp` with:

```cpp
#define GLEW_STATIC
#include <GL/glew.h>
#include <GL/freeglut.h>
#include <GL/gl.h>
#include <GL/glu.h>

#include "ObjLoader.h"
#include "particle.h"
#include "platform_audio.h"
#include "platform_time.h"
#include "resource_path.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <string>
#include <thread>
```

- [ ] **Step 2: Replace include in `ObjLoader.cpp`**

Replace:

```cpp
#define FREEGLUT_STATIC
#include "ObjLoader.h"
#include <fstream>
#include <iostream>
#include "GL/freeglut.h"
```

with:

```cpp
#include "ObjLoader.h"

#include "resource_path.h"

#include <GL/freeglut.h>

#include <fstream>
#include <iostream>
```

- [ ] **Step 3: Replace include in `particle.h`**

Replace:

```cpp
#define FREEGLUT_STATIC
#define GLUT_DISABLE_ATEXIT_HACK  
#include "vec.h"
#include <gl/glut.h>
#include <iostream>
```

with:

```cpp
#include "vec.h"

#include <GL/freeglut.h>

#include <iostream>
```

- [ ] **Step 4: Verify removed include blockers**

Run:

```bash
rg -n "windows.h|mmsystem.h|<gl/glut.h>|#pragma comment" src/Billiards/billiards.cpp src/Billiards/ObjLoader.cpp src/Billiards/particle.h
```

Expected: no matches in `billiards.cpp`, `ObjLoader.cpp`, or `particle.h`. Windows headers may still appear in `platform_audio.cpp` behind `_WIN32`.

- [ ] **Step 5: Commit**

```bash
git add src/Billiards/billiards.cpp src/Billiards/ObjLoader.cpp src/Billiards/particle.h
git commit -m "normalize opengl includes"
```

## Task 6: Route Assets Through Resource Paths

**Files:**
- Modify: `src/Billiards/billiards.cpp`
- Modify: `src/Billiards/ObjLoader.cpp`

- [ ] **Step 1: Update OBJ loader globals**

In `src/Billiards/billiards.cpp`, replace:

```cpp
ObjLoader tableObj(".//obj//table.obj");
ObjLoader cueObj(".//obj//cue.obj");
ObjLoader benchObj(".//obj//bench.obj");
ObjLoader wardObj(".//obj//wardrobe.obj");
```

with:

```cpp
ObjLoader tableObj(billiardgl::objectPath("table.obj"));
ObjLoader cueObj(billiardgl::objectPath("cue.obj"));
ObjLoader benchObj(billiardgl::objectPath("bench.obj"));
ObjLoader wardObj(billiardgl::objectPath("wardrobe.obj"));
```

- [ ] **Step 2: Update material texture loads**

In `initTable`, replace:

```cpp
textureIDtest[0] = loadTexture((".//tex//" + tableObj.materials[0]->texture).c_str());
textureIDtest[1] = loadTexture((".//tex//" + tableObj.materials[1]->texture).c_str());
```

with:

```cpp
const std::string tableTexture0 = billiardgl::texturePath(tableObj.materials[0]->texture);
const std::string tableTexture1 = billiardgl::texturePath(tableObj.materials[1]->texture);
textureIDtest[0] = loadTexture(tableTexture0.c_str());
textureIDtest[1] = loadTexture(tableTexture1.c_str());
```

In `initDecoration`, replace any `(".//tex//" + ...).c_str()` texture loads with the same pattern:

```cpp
const std::string wardTexture = billiardgl::texturePath(wardObj.materials[0]->texture);
textureWard = loadTexture(wardTexture.c_str());
```

Use local variable names that match the object being loaded.

In `initCue`, replace:

```cpp
textureCue[0] = loadTexture((".//tex//" + cueObj.materials[0]->texture).c_str());
textureCue[1] = loadTexture((".//tex//" + cueObj.materials[1]->texture).c_str());
```

with:

```cpp
const std::string cueTexture0 = billiardgl::texturePath(cueObj.materials[0]->texture);
const std::string cueTexture1 = billiardgl::texturePath(cueObj.materials[1]->texture);
textureCue[0] = loadTexture(cueTexture0.c_str());
textureCue[1] = loadTexture(cueTexture1.c_str());
```

- [ ] **Step 3: Update fixed texture loads**

In `initLoadTexture`, replace calls shaped like:

```cpp
Ball[1].texture = loadTexture("tex/B1.bmp");
```

with:

```cpp
Ball[1].texture = loadTexture(billiardgl::texturePath("B1.bmp").c_str());
```

Apply the same pattern to every fixed texture in `initLoadTexture`, including `ground.bmp`, `wall.bmp`, `wall1.bmp`, `wall2.bmp`, `ceiling.bmp`, `black.bmp`, `green.bmp`, `wood.bmp`, `5.bmp`, `6.bmp`, `7.bmp`, and `flame2.bmp`.

- [ ] **Step 4: Update MTL loading**

In `src/Billiards/ObjLoader.cpp`, replace:

```cpp
f.open(".//obj//" + filename, ios::in);
```

with:

```cpp
f.open(billiardgl::objectPath(filename), ios::in);
```

- [ ] **Step 5: Verify no hardcoded asset directories remain in modified source**

Run:

```bash
rg -n "\"tex/|\"obj/|audio//|\\.//tex|\\.//obj" src/Billiards -g '!dependencies/**'
```

Expected after Task 7 audio replacements: no direct `tex/`, `obj/`, `.//tex`, `.//obj`, or `audio//` paths in source. If this task runs before Task 7, audio matches may still appear and should be removed there.

- [ ] **Step 6: Commit**

```bash
git add src/Billiards/billiards.cpp src/Billiards/ObjLoader.cpp
git commit -m "use configured resource paths"
```

## Task 7: Replace Windows-Only Calls

**Files:**
- Modify: `src/Billiards/billiards.cpp`

- [ ] **Step 1: Replace hit sound calls**

Replace each:

```cpp
mciSendString(TEXT("play audio//hit.wav"), NULL, 0, NULL);
```

with:

```cpp
billiardgl::playHit();
```

- [ ] **Step 2: Replace pocket and game-over sound calls**

Replace:

```cpp
mciSendString(TEXT("play audio//ballin.wav"), NULL, 0, NULL);
```

with:

```cpp
billiardgl::playBallIn();
```

Replace:

```cpp
mciSendString(TEXT("play audio//GameOver.wav"), NULL, 0, NULL);
```

with:

```cpp
billiardgl::playGameOver();
```

- [ ] **Step 3: Replace background music**

Replace the body of `b_music()`:

```cpp
void b_music()
{
    PlaySound(TEXT("audio//background.wav"), NULL, SND_FILENAME | SND_ASYNC | SND_LOOP);
    //mciSendString(TEXT("play audio//background.wav"), NULL, 0, NULL);
}
```

with:

```cpp
void b_music()
{
    billiardgl::playBackgroundLoop();
}
```

- [ ] **Step 4: Replace sleep**

Replace:

```cpp
Sleep(1000);
```

with:

```cpp
billiardgl::sleepMilliseconds(1000);
```

- [ ] **Step 5: Replace `fopen_s`**

In `loadTexture`, replace:

```cpp
FILE* pFile;
fopen_s(&pFile, file_name, "rb");
if (pFile == 0)
    return 0;
```

with:

```cpp
FILE* pFile = std::fopen(file_name, "rb");
if (pFile == 0)
    return 0;
```

- [ ] **Step 6: Replace `_itoa_s`**

In `drawString`, replace:

```cpp
char str2[2];
_itoa_s(CurrPlayer + 1, str2, 10);
myString(18, 700, GLUT_BITMAP_TIMES_ROMAN_24, str1);
myString(140, 700, GLUT_BITMAP_TIMES_ROMAN_24, str2);
```

with:

```cpp
std::string str2 = std::to_string(CurrPlayer + 1);
myString(18, 700, GLUT_BITMAP_TIMES_ROMAN_24, str1);
myString(140, 700, GLUT_BITMAP_TIMES_ROMAN_24, const_cast<char*>(str2.c_str()));
```

- [ ] **Step 7: Remove unused Win32 font function**

Delete this declaration near the top of `billiards.cpp`:

```cpp
void selectFont(int size, int charset, const char* face);
```

Delete the full `selectFont` function definition:

```cpp
void selectFont(int size, int charset, const char* face) {
    HFONT hFont = CreateFontA(size, 0, 0, 0, FW_MEDIUM, 0, 0, 0,
        charset, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, face);
    HFONT holdFont = (HFONT)SelectObject(wglGetCurrentDC(), hFont);
    DeleteObject(holdFont);
}
```

- [ ] **Step 8: Verify no Windows-only calls remain in source**

Run:

```bash
rg -n "PlaySound|mciSendString|TEXT\\(|Sleep\\(|fopen_s|_itoa_s|CreateFont|HFONT|wglGetCurrentDC|windows.h|mmsystem.h|#pragma comment" src/Billiards/billiards.cpp src/Billiards/ObjLoader.cpp src/Billiards/particle.h
```

Expected: no matches in `billiards.cpp`, `ObjLoader.cpp`, or `particle.h`. `platform_audio.cpp` is allowed to contain `_WIN32`-guarded WinMM calls.

- [ ] **Step 9: Commit**

```bash
git add src/Billiards/billiards.cpp
git commit -m "replace windows-only runtime calls"
```

## Task 8: Build and Fix Portable Compile Errors

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `src/Billiards/billiards.cpp`
- Modify: `src/Billiards/ObjLoader.cpp`
- Modify: `src/Billiards/particle.h`
- Modify: `src/Billiards/platform_audio.cpp`
- Modify: `src/Billiards/resource_path.cpp`

- [ ] **Step 1: Configure**

Run:

```bash
cmake -S . -B build
```

Expected: configuration succeeds and prints build files written to `build`.

- [ ] **Step 2: Build**

Run:

```bash
cmake --build build
```

Expected: build succeeds and creates `build/Billiards`.

- [ ] **Step 3: If `OpenGL::GLU` is unavailable, use `${OPENGL_glu_LIBRARY}`**

If CMake reports that `OpenGL::GLU` is not a known target but GLU was found, replace the link block:

```cmake
target_link_libraries(Billiards PRIVATE
    OpenGL::GL
    OpenGL::GLU
    GLEW::GLEW
    ${GLUT_LIBRARIES}
)
```

with:

```cmake
target_link_libraries(Billiards PRIVATE
    OpenGL::GL
    ${OPENGL_glu_LIBRARY}
    GLEW::GLEW
    ${GLUT_LIBRARIES}
)
```

Then rerun:

```bash
cmake -S . -B build
cmake --build build
```

Expected: configuration and build succeed.

- [ ] **Step 4: If `GLUT_INCLUDE_DIR` is empty on macOS, use framework include behavior**

If macOS configuration finds GLUT as a framework and `GLUT_INCLUDE_DIR` is empty, keep `${GLUT_INCLUDE_DIR}` in `target_include_directories`; CMake accepts empty include entries. Do not add `/System/Library/Frameworks/GLUT.framework/Headers` unless the compiler reports missing `GL/freeglut.h`.

- [ ] **Step 5: Commit build fixes**

```bash
git add CMakeLists.txt src/Billiards
git commit -m "fix portable cmake build"
```

## Task 9: Runtime Smoke Test

**Files:**
- Modify only files required by runtime failures discovered during this task.

- [ ] **Step 1: Run the executable from the build directory**

Run:

```bash
./build/Billiards
```

Expected: a GLUT window opens, the game enters the main loop, and missing audio on Linux/macOS does not crash the process.

- [ ] **Step 2: Verify asset failures are visible**

If the program prints `Open obj file error!`, inspect the asset root by confirming this path exists:

```bash
ls src/Billiards/obj/table.obj src/Billiards/tex/B1.bmp src/Billiards/audio/background.wav
```

Expected: all three files exist.

- [ ] **Step 3: Fix asset root only if runtime lookup fails**

If asset lookup fails even though the files exist, verify CMake configured an absolute asset root:

```bash
grep -R "BILLIARDGL_ASSET_ROOT" build/CMakeFiles/Billiards.dir
```

Expected: the output includes `/src/Billiards`.

If the value is missing or relative, replace this line in `CMakeLists.txt`:

```cmake
set(BILLIARDGL_ASSET_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/src/Billiards")
```

with:

```cmake
get_filename_component(BILLIARDGL_ASSET_ROOT
    "${CMAKE_CURRENT_SOURCE_DIR}/src/Billiards"
    ABSOLUTE
)
```

Then rerun:

```bash
cmake -S . -B build
cmake --build build
./build/Billiards
```

Expected: the game finds OBJ and texture assets from the build directory.

- [ ] **Step 4: Commit runtime fixes**

If this task required code changes, run:

```bash
git add src/Billiards CMakeLists.txt
git commit -m "fix runtime asset lookup"
```

If no changes were needed, skip the commit.

## Task 10: Document Build Instructions

**Files:**
- Modify: `README.md`

- [ ] **Step 1: Add a cross-platform build section**

Append this section to `README.md`:

````markdown
## Cross-Platform Build

The portable build uses CMake and keeps the existing FreeGLUT/GLEW/OpenGL rendering path.

### Linux

Install the development dependencies for your distribution. On Debian or Ubuntu:

```bash
sudo apt-get install cmake g++ libgl1-mesa-dev libglu1-mesa-dev libglew-dev freeglut3-dev
```

Build:

```bash
cmake -S . -B build
cmake --build build
./build/Billiards
```

### macOS

Install dependencies with Homebrew:

```bash
brew install cmake glew freeglut
```

Build:

```bash
cmake -S . -B build
cmake --build build
./build/Billiards
```

OpenGL and GLUT are deprecated on macOS, so compiler warnings are expected. The first migration phase accepts those warnings as long as the program builds and runs.

### Audio

Linux and macOS use no-op audio in the first migration phase. Sound calls are routed through `platform_audio`, so the game can run without crashing even when sound is silent.
````

- [ ] **Step 2: Verify Markdown fences**

Run:

```bash
python3 - <<'PY'
from pathlib import Path
text = Path("README.md").read_text()
assert text.count("```") % 2 == 0
print("README markdown fences balanced")
PY
```

Expected:

```text
README markdown fences balanced
```

- [ ] **Step 3: Commit**

```bash
git add README.md
git commit -m "document cross-platform build"
```

## Task 11: Final Verification and Push

**Files:**
- No planned source changes.

- [ ] **Step 1: Run source scans**

Run:

```bash
rg -n "PlaySound|mciSendString|TEXT\\(|Sleep\\(|fopen_s|_itoa_s|CreateFont|HFONT|wglGetCurrentDC|windows.h|mmsystem.h|#pragma comment|<gl/glut.h>" src/Billiards/billiards.cpp src/Billiards/ObjLoader.cpp src/Billiards/particle.h
```

Expected: no matches.

- [ ] **Step 2: Rebuild from a clean build directory**

Run:

```bash
cmake -S . -B build
cmake --build build
```

Expected: configuration and build succeed.

- [ ] **Step 3: Check git state**

Run:

```bash
git status -sb
```

Expected: clean working tree on the implementation branch.

- [ ] **Step 4: Push to the user's fork**

Run:

```bash
git push
```

Expected: branch pushes to `mine/master`, because local `master` tracks `mine/master`.
