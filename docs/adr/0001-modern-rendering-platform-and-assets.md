# ADR 0001: Modern rendering platform, UI text, and glTF ingestion

- Status: Accepted for Phase 0; implementation beyond the isolated spike is deferred
- Date: 2026-07-27
- Scope: presentation layer only

## Context

BilliardGL's production renderer currently uses GLUT and legacy fixed-function OpenGL. The modernization plan needs a platform baseline that does not couple graphics dependencies into the simulation or headless automation. Technical art also needs stable assumptions for ball finish, identity markings, and mesh complexity before production assets begin.

## Decisions

### OpenGL context and shaders

The modern renderer will target the OpenGL 3.3 feature set. On macOS it will request an OpenGL 4.1 Core Profile context and compile `#version 410 core` shaders. Other platforms may compile equivalent `#version 330 core` sources selected by a small platform preamble; shader bodies must stay feature-compatible with 3.3 unless an explicitly gated capability is introduced.

The legacy GLUT context remains unchanged during migration. A modern context must be created through a platform/window abstraction capable of setting an explicit Core Profile. The Phase 0 executable uses Cocoa's `NSOpenGLContext` directly because Apple's GLUT API cannot reliably request a 4.1 Core Profile. This is evidence for the context decision, not the production window abstraction.

All presentation rendering uses linear working values. Color textures are sampled as sRGB; data textures (normal, roughness, metallic, masks) are linear. The final framebuffer performs one sRGB conversion after tone mapping. Phase 1 will use an HDR intermediate target and compare ACES-fit against Reinhard before freezing exposure. The Phase 0 PPM is deliberately an untagged diagnostic image and does not validate color-management parity.

### Product UI text

Use FreeType to rasterize glyphs and a signed-distance-field atlas for the shipping HUD. Introduce a small engine-owned text layout/rendering boundary so shaping can later be backed by HarfBuzz without changing HUD view models. Cache atlases by font face, nominal size, and scale bucket; do not rasterize glyphs during ordinary frames.

Dear ImGui is permitted only for developer/debug panels. RmlUi is not selected for the first product HUD because its document/CSS runtime is unnecessary for the small in-match surface and would add another layout and integration layer. The Spike's embedded 5x7 bitmap is only proof that Core Profile UI geometry, alpha blending, and screenshot capture work; it is not a product font or text API.

Before shipping a font, verify its redistribution license and coverage for Simplified Chinese plus Latin numerals. Prefer an OFL-licensed Noto Sans CJK subset, with attribution and deterministic subsetting recorded in the asset manifest.

### glTF 2.0 ingestion

Use `cgltf` as the runtime glTF 2.0 parser. It is a small C single-header-style implementation, allows engine-owned allocation and I/O, and does not force exceptions, a JSON object model, image decoding, or an OpenGL upload policy into the resource layer. Keep `stb_image` for development PNG/JPEG decoding initially.

The supported Phase 1 subset is `.gltf` and `.glb`, indexed triangle primitives, positions, normals, tangents, UV0, vertex colors, node transforms, and metallic-roughness materials. Reject unsupported required extensions with an actionable error. Run an offline validation/conversion step in CI and emit engine metadata separately; do not modify source assets on load. Texture compression and KTX2/BasisU remain deferred until profiling demonstrates need.

`tinygltf` was considered because it is convenient and feature-rich, but its C++/JSON/image dependency surface and compile-time cost are larger than needed. Assimp was rejected for this path because broad format support obscures the exact glTF contract and increases dependency size. Existing OBJ loading remains available only during migration.

### Technical-art contracts frozen for M1

- Ball clearcoat is a material response, not baked highlight art. The forward shader must expose base color, perceptual roughness, specular/IOR-equivalent control, and a single clearcoat weight/roughness lobe. Phase 1 may implement the lobe directly even though core glTF metallic-roughness lacks it; authoring may use `KHR_materials_clearcoat`, with a documented fallback.
- Ball identity uses one shared sphere mesh and material family. Numbers, stripes, and spot identities come from an atlas/decal mask selected per instance. Do not create sixteen uniquely tessellated ball meshes or bake lighting into identity textures. Reserve atlas padding and mip-safe borders; keep logos/number art in sRGB and masks in linear space.
- Use authored LODs rather than runtime mesh simplification. For the first table, define ball LOD0 for close-up/replay, LOD1 for ordinary play, and LOD2 for distant/background views, selected by projected screen radius with hysteresis. Collision and simulation geometry never change with visual LOD. Exact triangle budgets are set after Phase 0 reference-camera measurement; until then, asset publishing must retain the unsimplified source and deterministic LOD derivation settings.
- Normal-map convention is OpenGL (+Y). Tangent space follows glTF/MikkTSpace expectations. Meters are the interchange unit, +Y is up in authored glTF, and the importer owns conversion into the current world convention.

## Isolation and failure behavior

`BilliardsRenderCoreProfileSpike` links only Cocoa and OpenGL. It does not link `BILLIARDGL_CORE_SOURCES`, SDL audio, GLUT, GLEW, the game renderer, or screenshot implementation. Its framebuffer and PPM writer are local to `spikes/`. Consequently, configuring or running headless simulation targets cannot initialize a graphics context through this change.

Shader compilation, program linking, pixel-format creation, Core Profile verification, framebuffer completeness, and screenshot writes fail loudly with a non-zero exit code. The Spike prints the driver GL/GLSL strings, profile, output size, and demonstrated components on success.

## Consequences and follow-up

- macOS OpenGL is deprecated and capped at 4.1, but the chosen 3.3 feature baseline remains sufficient for the planned forward PBR slice.
- The product window/context abstraction must replace or isolate GLUT before production modern rendering can become the default. This ADR does not select GLFW versus SDL video; that choice should consider the existing SDL audio dependency and Windows packaging.
- FreeType, `cgltf`, production font assets, shader preprocessing, HDR/sRGB validation, and glTF fixtures are Phase 1 work. They are intentionally not vendored by this Spike.
- Delete the Spike target and `spikes/render_core_profile/` to roll back the executable without changing production behavior.

## Verification

On macOS, build and run:

```sh
/opt/homebrew/bin/cmake -S . -B build/render-spike
/opt/homebrew/bin/cmake --build build/render-spike --target BilliardsRenderCoreProfileSpike
build/render-spike/BilliardsRenderCoreProfileSpike --screenshot /tmp/billiardgl-render-core-profile-spike.ppm
```

Success requires a Core Profile report, a 640x360 P6 PPM, and visibly distinct shader-shaded ball, tiled texture plane, and `CORE 41` bitmap text. The registered CTest is `BilliardsRenderCoreProfileSpikeScreenshot`.
