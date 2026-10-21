# Product UI GPU submission contract

Status: independent OpenGL 4.1 Core submitter and FBO smoke **PASS**; production
`ModernRenderer` call-site and 594-case capture matrix **NOT RUN**.

## Production facade

`ModernProductUiRuntime` is the single production-facing call surface. Its
trusted-default configuration binds the HUD manifest, HUD token set, and Shell
token set to their reviewed SHA-256 identities; callers may inject alternate
absolute paths only together with explicit expected hashes. The font path and
atlas configuration are injected as well, with an optional font SHA for a
packaged font (installed system fonts may omit it).

`initialize(config)` verifies bytes before parsing, runs the HUD manifest and
both independent strict token loaders, merges the explicit registry, freezes
the SDF atlas, builds the coordinator, and initializes the injected GPU
renderer. Missing/tampered files, schema/token drift, registry conflicts, font
or atlas failures, and context/resource errors fail closed with prefixed error
codes. `render(sessionSnapshot, presentationSnapshot, environment)` then does
exactly `coordinator.build -> gpu.render` and returns overlay, screen,
canonical SHA, submission state, metrics, and any error.

Non-product, automation, and windowless calls short-circuit before all assets
and GPU state, returning deterministic zero-draw evidence even if the runtime
was never initialized or its context was lost. `resize` records the current
physical target and rejects a stale session viewport; it does not recalculate
layout or apply scale. `contextLost` preserves validated CPU assets while
abandoning stale GL names, `reinitializeGpu` rebuilds only GPU resources, and
`shutdown` releases the current resource set.

`BilliardsModernProductUiRuntime` is a self-contained static CPU/facade target.
A production Core 4.1 caller links it together with
`BilliardsProductUiOpenGL41`, constructs one dispatch and runtime, and needs
only `uiRuntime.render(...)` per frame.

`ProductUiGpuRenderer` consumes an already expanded `ProductUiFrame`. It has no
gameplay, settings, shell-navigation, or token-resolution authority. A frame
with `ProductUiOverlayKind::None` performs zero resource checks and zero draw
calls. Product frames fail closed if they contain debug nodes, an opaque
`UiPrimitiveKind::Icon`, an atlas mismatch, invalid viewport/color/geometry,
unsupported primitives, or degenerate triangles.

The CPU layer converts top-left physical pixels directly to NDC and converts
each top-left clip rectangle to a bottom-left OpenGL scissor. DPI and UI scale
have already been resolved by the coordinator and are not applied again.
Consecutive items with equal clip and shader mode form batches while preserving
the backend-neutral stream order: ordinary Panel/SolidRect/Line/GlyphQuad
primitives, expanded icon triangles, then icon-label SDF requests.

`Core41ProductUiGlDispatch` owns the concrete OpenGL contract:

- GLSL 4.10 Core, VAO, streaming VBO, and immutable single-channel R8 SDF atlas;
- straight-alpha blending using source alpha and `GL_FRAMEBUFFER_SRGB`;
- incoming standard/CVD role colors remain unchanged in vertices; the fragment
  shader converts their sRGB channels to linear before framebuffer encoding;
- depth disabled during UI, scissor enabled per batch, and caller program,
  VAO/VBO, texture, blend, depth, scissor, and framebuffer-sRGB state restored;
- shader compile/link, atlas/uniform/resource creation, upload, scissor, draw,
  and GL errors return explicit stable prefixes;
- ordinary shutdown deletes resources, while context loss abandons stale object
  names and requires a complete shader/buffer/atlas rebuild in the new context.

The dispatch interface is injectable. `BilliardsProductUiGpuRendererTests` uses
a headless recording backend to verify call order, NDC/scissor transforms,
solid/SDF/triangle/label batches, metrics, resize behavior, standard/CVD value
preservation, zero draw, opaque-icon/debug rejection, context loss, cleanup,
and injected create/upload/draw failures. It links FreeType only—no OpenGL,
Cocoa, SDL, or window system.

`BilliardsProductUiGpuSmoke` creates an isolated macOS OpenGL 4.1 Core context,
submits a clipped panel into an sRGB FBO, reads inside/outside pixels, checks
metrics and GL state restoration, and reports PASS. This is a focused submitter
smoke, not a product screenshot and not evidence that the pending 594 captures
were executed.

The remaining production integration point is deliberately small and is not
implemented in this change: after the scene target is bound and the
`ProductUiFrameCoordinator` has built the current frame, call
`ProductUiGpuRenderer::render(frame)` and publish its result/metrics into the
existing renderer diagnostics. Context teardown/loss must call `shutdown()` or
`contextLost()` respectively. No alternate layout, scaling, token, or palette
logic belongs at that call-site.

Verification:

```sh
cmake --build build --target \
  BilliardsProductUiGpuRendererTests BilliardsModernProductUiRuntimeTests \
  BilliardsProductUiGpuSmoke
ctest --test-dir build --output-on-failure \
  -R 'BilliardsProductUiGpuRendererTests|BilliardsModernProductUiRuntimeTests|BilliardsProductUiGpuSmoke'
```
