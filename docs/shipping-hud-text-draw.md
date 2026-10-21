# Shipping HUD text and draw-list foundation

The shipping HUD follows ADR 0001. Product text is rasterized with FreeType
into an engine-owned signed-distance-field atlas. GLUT bitmap fonts, embedded
5×7 glyphs, and ImGui are not part of this product path.

The implementation is isolated under `src/Billiards/render/ui` and does not
create an OpenGL context or modify `ModernRenderer`. Renderer upload and draw
submission are the next integration step.

The approved `focused_practice_hud_shipping_tokens_v1` JSON now has a strict
C++ loader and CPU icon expander. The source asset remains byte-identical at
SHA-256 `abf73aa283279c28677b5c97a9b02b3130b0d0125048c3124c85a9c0e72df860`.

## Font lifecycle

`FontAtlasConfig` identifies an atlas by installed font path, face index,
nominal pixel size, and scale bucket. Atlas dimensions, glyph padding and SDF
spread are explicit limits. `requiredHudCodepoints()` contains printable Latin,
digits, punctuation and the em dash used by neutral HUD values.

`SdfFontAtlas::initialize` performs the complete resource phase:

1. Open the configured face with FreeType.
2. Validate every required codepoint.
3. Rasterize every glyph once.
4. Generate deterministic SDF pixels and deterministic shelf packing.
5. Record bearings, advances, atlas rectangles, line height and tabular-digit
   advance.
6. Destroy the FreeType face and library and freeze the atlas.

Ordinary-frame APIs expose lookup, UTF-8 measurement and immutable pixels only.
They have no rasterization entry point. Draw-list construction records the
initial rasterization count and fails if it changes. An unbaked runtime glyph
returns `font_glyph_not_prebaked`; it is never added on demand.

The macOS resolver returns a path under the installed system font directories
and a face index. It never copies a font into the repository. Tests inject
`/System/Library/Fonts/Menlo.ttc` directly for repeatability. Shipping font
licensing and Simplified-Chinese coverage remain an asset-packaging gate before
a redistributable font can replace the system resolver.

## Draw-list contract

`HudDrawListBuilder` consumes only `ProductHudViewModel`, the resolved
`HudLayoutResult`, and a frozen `SdfFontAtlas`. It emits backend-neutral:

- translucent panel rectangles;
- solid power segments;
- line/crosshair primitives;
- semantic icon/shape tokens;
- clipped SDF glyph quads with normalized atlas UVs.

Text measurement uses cached advances and tabular digit cells. Glyph rectangles
and UVs are adjusted when clipped, and every primitive carries its clip
rectangle. The product list never contains debug nodes. Debug telemetry builds
into a separate list and returns an empty list for release capture.

CPU draw-list acceptance covers 1024×768, 1280×720, 1920×1080, and 1920×1080
at 144 DPI. Every primitive and clip rectangle must remain inside the resolved
safe area.

## Shipping icon expansion

`loadHudShippingTokenSet` validates the complete schema, identity, authorship,
external-asset prohibition, exact 16-icon inventory, aliases, standard and
color-vision-safe palettes, five-state usage, typography/layout/accessibility,
debug isolation, and GPU contract. Unknown or missing fields, bad types,
non-finite values, invalid colors, out-of-bounds or degenerate geometry, alias
cycles, debug tokens, and external assets fail with explicit codes.

`expandShippingHudIcons` replaces each approved `Icon` with clipped CPU
triangles generated from normalized 24×24 rect, circle, polyline, or polygon
commands. Stroke widths scale with the physical icon and are pixel-snapped;
round caps/joins use fixed topology. Each triangle preserves both source and
resolved token IDs. SDF label slots remain explicit `UiIconLabel` requests.
`ProductSettings::colorVisionMode` selects standard or color-vision-safe
palette through a pure function and never mutates gameplay authority.

Multiple separately approved packs may only be combined through
`mergeHudShippingTokenSets`. The registry retains each product namespace and
palette by token-set ID; multi-namespace expansion therefore requires an
explicit ID instead of silently flattening Shell and HUD colors. Set-ID,
same-namespace token/alias/palette, and alias-cycle conflicts fail closed.
The reviewed 43-token `focused_practice_shell_shipping_tokens_v1` pack has its
own exact-schema loader; unknown ProductShell tokens remain hard errors.

`ProductUiFrameCoordinator` is the GPU-agnostic per-frame selector. In normal
product mode it emits exactly one expanded CPU draw list: Match uses Gameplay
HUD, while the other eight ProductScreen states retain ProductShell controls
(including training reset/catalog and replay transport/camera/seek/file).
Automation, windowless, and non-product modes emit no overlay. Its canonical
bytes/SHA are deterministic CPU evidence, not a GPU submission claim.

## Explicit failures

Initialization fails closed with stable codes for invalid configuration,
missing files, bad font/face, unsupported pixel size, missing glyph,
rasterization failure, invalid SDF input, and atlas overflow. UTF-8, text size,
uninitialized atlas and unbaked frame glyph errors are also explicit.

Verification target:

```sh
cmake --build build --target BilliardsShippingHudRenderTests
cmake --build build --target BilliardsHudShippingTokenTests
cmake --build build --target BilliardsProductUiFrameCoordinatorTests
ctest --test-dir build --output-on-failure \
  -R 'BilliardsShippingHudRenderTests|BilliardsHudShippingTokenTests|BilliardsProductUiFrameCoordinatorTests|BilliardsProductShellTests'
```

The draw-list, token, and coordinator targets are CPU evidence. The independent
Core 4.1 submitter and focused FBO smoke are documented in
`docs/product-ui-gpu-submission.md`; they prove the submission component but do
not prove the production `ModernRenderer` call-site, a product screenshot, or
the pending 594-case capture matrix.
