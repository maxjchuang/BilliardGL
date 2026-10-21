#include "render/ui/product_ui_gpu_renderer.h"

#include <cmath>
#include <cstdlib>
#include <iostream>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

using namespace billiardgl::render::ui;

void expect(bool value, const std::string& message)
{
    if (!value) throw std::runtime_error("product UI GPU: " + message);
}

bool close(float first, float second)
{
    return std::fabs(first - second) < 0.0001f;
}

SdfFontAtlas makeAtlas()
{
    FontAtlasConfig config;
    config.fontPath = "/System/Library/Fonts/Menlo.ttc";
    config.nominalPixelSize = 32;
    config.atlasWidth = 1024;
    config.atlasHeight = 1024;
    SdfFontAtlas atlas;
    expect(atlas.initialize(config).ok, "test SDF atlas initializes");
    return atlas;
}

class RecordingDispatch final : public ProductUiGlDispatch {
public:
    UiGpuResult createResources(const UiGpuResourceCreateInfo& info,
        UiGpuResourceHandles& handles) override
    {
        ++creates;
        vertexShader = info.vertexShader;
        fragmentShader = info.fragmentShader;
        atlasWidth = info.atlasWidth;
        atlasHeight = info.atlasHeight;
        atlasBytes = info.atlasPixels == nullptr ? 0 :
            info.atlasPixels->size();
        if (!createError.empty()) return {false, createError};
        handles = {11, 12, 13, 14};
        return {true, {}};
    }

    void destroyResources(UiGpuResourceHandles& handles) noexcept override
    {
        ++destroys;
        handles = {};
    }

    void abandonResources(UiGpuResourceHandles& handles) noexcept override
    {
        ++abandons;
        handles = {};
    }

    UiGpuResult beginFrame(const UiGpuResourceHandles&, int width,
        int height) override
    {
        ++begins;
        viewportWidth = width;
        viewportHeight = height;
        calls.push_back("begin");
        return beginError.empty() ? UiGpuResult{true, {}} :
            UiGpuResult{false, beginError};
    }

    UiGpuResult uploadVertices(const UiGpuResourceHandles&,
        const std::vector<UiGpuVertex>& value) override
    {
        ++uploads;
        vertices = value;
        calls.push_back("upload");
        return uploadError.empty() ? UiGpuResult{true, {}} :
            UiGpuResult{false, uploadError};
    }

    UiGpuResult setScissor(const UiGpuScissor& value) override
    {
        scissors.push_back(value);
        calls.push_back("scissor");
        return scissorError.empty() ? UiGpuResult{true, {}} :
            UiGpuResult{false, scissorError};
    }

    UiGpuResult draw(const UiGpuResourceHandles&,
        const UiGpuBatch& value) override
    {
        draws.push_back(value);
        calls.push_back("draw");
        return drawError.empty() ? UiGpuResult{true, {}} :
            UiGpuResult{false, drawError};
    }

    UiGpuResult endFrame() override
    {
        ++ends;
        calls.push_back("end");
        return endError.empty() ? UiGpuResult{true, {}} :
            UiGpuResult{false, endError};
    }

    void clearFrame()
    {
        vertices.clear();
        scissors.clear();
        draws.clear();
        calls.clear();
    }

    int creates = 0;
    int destroys = 0;
    int abandons = 0;
    int begins = 0;
    int uploads = 0;
    int ends = 0;
    int viewportWidth = 0;
    int viewportHeight = 0;
    int atlasWidth = 0;
    int atlasHeight = 0;
    std::size_t atlasBytes = 0;
    std::string vertexShader;
    std::string fragmentShader;
    std::vector<UiGpuVertex> vertices;
    std::vector<UiGpuScissor> scissors;
    std::vector<UiGpuBatch> draws;
    std::vector<std::string> calls;
    std::string createError;
    std::string beginError;
    std::string uploadError;
    std::string scissorError;
    std::string drawError;
    std::string endError;
};

ProductUiFrame frame(const SdfFontAtlas& atlas)
{
    ProductUiFrame value;
    value.ok = true;
    value.overlay = ProductUiOverlayKind::Shell;
    value.physicalWidth = 200;
    value.physicalHeight = 100;
    value.dpi = 144.0f;
    value.draw.ok = true;
    value.draw.atlasCacheKey = atlas.cacheKey();
    value.draw.safeRect = {0.0f, 0.0f, 200.0f, 100.0f};

    UiPrimitive panel;
    panel.kind = UiPrimitiveKind::Panel;
    panel.rect = {10.0f, 20.0f, 40.0f, 20.0f};
    panel.clipRect = {5.0f, 10.0f, 80.0f, 50.0f};
    panel.color = {0.25f, 0.5f, 0.75f, 0.5f};
    value.draw.primitives.push_back(panel);

    const GlyphRecord* glyph = atlas.findGlyph('A');
    expect(glyph != nullptr, "A is prebaked");
    UiPrimitive glyphQuad;
    glyphQuad.kind = UiPrimitiveKind::GlyphQuad;
    glyphQuad.rect = {60.0f, 20.0f, 12.0f, 18.0f};
    glyphQuad.clipRect = panel.clipRect;
    glyphQuad.color = {0.9f, 0.8f, 0.7f, 1.0f};
    glyphQuad.u0 = static_cast<float>(glyph->atlasX) / atlas.width();
    glyphQuad.v0 = static_cast<float>(glyph->atlasY) / atlas.height();
    glyphQuad.u1 = static_cast<float>(glyph->atlasX + glyph->atlasWidth) /
        atlas.width();
    glyphQuad.v1 = static_cast<float>(glyph->atlasY + glyph->atlasHeight) /
        atlas.height();
    value.draw.primitives.push_back(glyphQuad);

    UiPrimitive solid;
    solid.kind = UiPrimitiveKind::Line;
    solid.rect = {90.0f, 30.0f, 30.0f, 1.0f};
    solid.clipRect = {80.0f, 20.0f, 60.0f, 40.0f};
    solid.color = {0.1f, 0.2f, 0.3f, 1.0f};
    value.draw.primitives.push_back(solid);

    UiTriangle triangle;
    triangle.first = {90.0f, 40.0f};
    triangle.second = {110.0f, 40.0f};
    triangle.third = {100.0f, 55.0f};
    triangle.clipRect = solid.clipRect;
    triangle.color = {0.1f, 0.2f, 0.3f, 1.0f};
    triangle.sourceToken = "test_source";
    triangle.resolvedToken = "test_resolved";
    value.draw.triangles.push_back(triangle);

    UiIconLabel label;
    label.center = {150.0f, 50.0f};
    label.clipRect = {140.0f, 35.0f, 30.0f, 30.0f};
    label.color = {1.0f, 0.9f, 0.8f, 1.0f};
    label.requestedPixelSize = 13.0f;
    label.text = "8";
    label.fontRole = "caption_sdf";
    value.draw.iconLabels.push_back(label);
    return value;
}

void testProductInsetBilingualInk()
{
    std::vector<TrajectoryInsetShellLayout> layouts;
    std::set<std::uint32_t> codepoints;
    for (auto size : {std::pair<int,int>{1280,720},{1920,1080},{2560,1440}})
    for (float scale : {.85f,1.f,1.35f}) {
        TrajectoryInsetShellRequest request;
        request.profileId="product_"+std::to_string(size.first)+"x"+
            std::to_string(size.second)+"_ui"+std::to_string(static_cast<int>(std::lround(scale*100)));
        request.physicalWidth=size.first; request.physicalHeight=size.second; request.uiScale=scale;
        request.exclusions={{"player status HUD",{},false},{"shot power and tip controls",{},false},
            {"rules decision panel or modal",{},false},{"caption and accessibility live-region band",{},false},
            {"safe-area edges and product footer",{},false}};
        auto layout=buildTrajectoryInsetShellLayout(request);
        const auto audit = auditTrajectoryInsetShellLayout(request,layout);
        expect(layout.ok && layout.insetVisible && audit.ok,
            "product chrome layout keeps complete plot and non-overlapping text boxes: " +
                request.profileId + ":" + layout.errorCode + ":" + audit.errorCode);
        for(const auto& text:buildTrajectoryInsetChromeDrawList(layout).orderedText){
            std::vector<std::uint32_t> cp;std::string error;
            expect(decodeUtf8(text,cp,error),"valid bilingual text");
            codepoints.insert(cp.begin(),cp.end());
        }
        layouts.push_back(layout);
    }
    for(std::uint32_t cp='0';cp<='9';++cp)codepoints.insert(cp);
    FontAtlasConfig config;config.fontPath="/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
    config.nominalPixelSize=32;
    SdfFontAtlas atlas;
    expect(atlas.initialize(config,{codepoints.begin(),codepoints.end()}).ok,"actual Arial Unicode atlas");
    RecordingDispatch dispatch;
    dispatch.configureTrajectoryInsetChromeTarget({21,22,true});
    ProductUiGpuRenderer renderer(dispatch);
    expect(renderer.initialize(atlas).ok,"bilingual renderer initializes");
    for(const auto& layout:layouts){
        const auto result=renderer.renderTrajectoryInsetChrome(buildTrajectoryInsetChromeDrawList(layout),
            layout,2560,1440);
        expect(result.ok,"every product text run fits measured width and complete actual ink: "+result.errorCode);
        expect(layout.legendTextPx>=13.f,"no subminimum glyph size");
    }
    auto bad=layouts.front();bad.title=std::string(100,'A');
    auto rejected=renderer.renderTrajectoryInsetChrome(buildTrajectoryInsetChromeDrawList(bad),bad,2560,1440);
    expect(!rejected.ok && rejected.errorCode.find("trajectory_inset_text_width_exceeds_clip:")==0,
        "overwide title fails instead of clipping or shrinking");
    bad=layouts.front();bad.titleRect.height=1;
    rejected=renderer.renderTrajectoryInsetChrome(buildTrajectoryInsetChromeDrawList(bad),bad,2560,1440);
    expect(!rejected.ok && rejected.errorCode.find("trajectory_inset_text_ink_exceeds_clip:")==0,
        "short line box rejects clipped glyph ink");
}

void testWorldAnnotationDoesNotClaimInteractiveChrome()
{
    const auto atlas = makeAtlas();
    RecordingDispatch dispatch;
    ProductUiGpuRenderer renderer(dispatch);
    expect(renderer.initialize(atlas).ok, "world annotation setup");
    auto ordinary = frame(atlas);
    expect(renderer.render(ordinary).ok, "ordinary UI renders");
    const auto before = dispatch.vertices;
    ordinary.draw.triangles.front().worldAnnotation = true;
    expect(renderer.render(ordinary).ok, "world annotation renders");
    expect(before.size() == dispatch.vertices.size(), "geometry inventory preserved");
    std::size_t worldVertices = 0;
    for (std::size_t i = 0; i < before.size(); ++i) {
        const auto& a = before[i];
        const auto& b = dispatch.vertices[i];
        expect(a.ndcX == b.ndcX && a.ndcY == b.ndcY &&
            a.color.red == b.color.red && a.color.green == b.color.green &&
            a.color.blue == b.color.blue && a.color.alpha == b.color.alpha,
            "annotation classification does not change color or geometry");
        expect(a.interactiveChrome == 1.0f, "default UI remains chrome");
        if (b.interactiveChrome == 0.0f) ++worldVertices;
    }
    expect(worldVertices == 3, "only typed world triangle loses chrome coverage");
}

void testSubmissionContractAndMetrics()
{
    const SdfFontAtlas atlas = makeAtlas();
    RecordingDispatch dispatch;
    ProductUiGpuRenderer renderer(dispatch);
    const UiGpuResult initialized = renderer.initialize(atlas);
    expect(initialized.ok && renderer.initialized() && dispatch.creates == 1 &&
        dispatch.atlasWidth == atlas.width() &&
        dispatch.atlasHeight == atlas.height() &&
        dispatch.atlasBytes == atlas.pixels().size() &&
        dispatch.vertexShader.find("#version 410 core") != std::string::npos &&
        dispatch.fragmentShader.find("srgbToLinear") != std::string::npos,
        "Core 4.1 shader and immutable SDF atlas resource contract is explicit");

    const ProductUiGpuFrameResult rendered = renderer.render(frame(atlas));
    expect(rendered.ok && dispatch.begins == 1 && dispatch.uploads == 1 &&
        dispatch.ends == 1 && rendered.metrics.inputPrimitives == 3 &&
        rendered.metrics.inputTriangles == 1 &&
        rendered.metrics.inputIconLabels == 1 &&
        rendered.metrics.solidVertices == 15 &&
        rendered.metrics.sdfVertices == 12 &&
        rendered.metrics.batches == 1 && rendered.metrics.drawCalls == 1 &&
        rendered.metrics.rejectedOpaqueIcons == 0,
        "solid, triangle, glyph, and icon-label vertices retain order in one measured batch");
    expect(dispatch.calls.front() == "begin" && dispatch.calls[1] == "upload" &&
        dispatch.calls.back() == "end" && dispatch.draws.size() == 1 &&
        rendered.metrics.drawCalls == dispatch.draws.size() &&
        dispatch.draws[0].kind == UiGpuBatchKind::Solid,
        "reported draw count equals backend draw submissions exactly");
    expect(dispatch.scissors[0] == UiGpuScissor{0, 0, 200, 100} &&
        close(dispatch.vertices[0].clipLeft, 5.0f) &&
        close(dispatch.vertices[0].clipBottom, 40.0f) &&
        close(dispatch.vertices[0].clipRight, 85.0f) &&
        close(dispatch.vertices[0].clipTop, 90.0f),
        "one viewport scissor retains each source clip in bottom-left vertex bounds");
    expect(close(dispatch.vertices[0].ndcX, -0.9f) &&
        close(dispatch.vertices[0].ndcY, 0.6f) &&
        close(dispatch.vertices[0].color.red, 0.25f) &&
        close(dispatch.vertices[0].color.green, 0.5f) &&
        close(dispatch.vertices[0].color.blue, 0.75f) &&
        close(dispatch.vertices[0].color.alpha, 0.5f),
        "physical pixels map to NDC and sRGB role values remain byte-equivalent floats");
}

void testResizeContextLossAndZeroDraw()
{
    const SdfFontAtlas atlas = makeAtlas();
    RecordingDispatch dispatch;
    ProductUiGpuRenderer renderer(dispatch);

    ProductUiFrame none;
    none.overlay = ProductUiOverlayKind::None;
    expect(renderer.render(none).ok && dispatch.begins == 0 &&
        dispatch.uploads == 0 && dispatch.draws.empty(),
        "None/non-product frame is zero draw even before initialization");
    expect(renderer.initialize(atlas).ok, "renderer initializes for resize");
    ProductUiFrame resized = frame(atlas);
    resized.physicalWidth = 400;
    resized.physicalHeight = 200;
    resized.dpi = 192.0f;
    expect(renderer.render(resized).ok && dispatch.viewportWidth == 400 &&
        dispatch.viewportHeight == 200 &&
        close(dispatch.vertices[0].ndcX, -0.95f) &&
        close(dispatch.vertices[0].ndcY, 0.8f) && dispatch.creates == 1,
        "resize changes only physical-pixel-to-NDC mapping and does not rebuild or double-scale resources");

    renderer.contextLost();
    expect(!renderer.initialized() && dispatch.abandons == 1 &&
        dispatch.destroys == 0 &&
        renderer.render(frame(atlas)).errorCode ==
            "product_ui_gpu_not_initialized",
        "context loss abandons stale names without deleting into a replacement context");
    expect(renderer.initialize(atlas).ok && dispatch.creates == 2,
        "new context performs a full shader/buffer/atlas rebuild");
    renderer.shutdown();
    expect(dispatch.destroys == 1 && !renderer.initialized(),
        "ordinary shutdown destroys complete resources exactly once");
}

void testStandardAndCvdColorsRemainUnmodified()
{
    const SdfFontAtlas atlas = makeAtlas();
    RecordingDispatch dispatch;
    ProductUiGpuRenderer renderer(dispatch);
    expect(renderer.initialize(atlas).ok, "palette fixture initializes");
    ProductUiFrame standard = frame(atlas);
    standard.palette = HudPaletteMode::Standard;
    standard.draw.primitives.front().color =
        {49.0f / 255.0f, 183.0f / 255.0f, 200.0f / 255.0f, 1.0f};
    expect(renderer.render(standard).ok, "standard palette submits");
    const UiColor standardGpu = dispatch.vertices.front().color;
    dispatch.clearFrame();
    ProductUiFrame cvd = frame(atlas);
    cvd.palette = HudPaletteMode::ColorVisionSafe;
    cvd.draw.primitives.front().color =
        {93.0f / 255.0f, 217.0f / 255.0f, 193.0f / 255.0f, 1.0f};
    expect(renderer.render(cvd).ok, "CVD palette submits");
    const UiColor cvdGpu = dispatch.vertices.front().color;
    expect(close(standardGpu.red, 49.0f / 255.0f) &&
        close(standardGpu.green, 183.0f / 255.0f) &&
        close(cvdGpu.red, 93.0f / 255.0f) &&
        close(cvdGpu.green, 217.0f / 255.0f) &&
        standardGpu.red != cvdGpu.red,
        "standard/CVD sRGB role values reach shader vertices without CPU recoloring");
}

void testFailClosedInputsAndDispatchErrors()
{
    const SdfFontAtlas atlas = makeAtlas();
    RecordingDispatch dispatch;
    ProductUiGpuRenderer renderer(dispatch);
    expect(renderer.initialize(atlas).ok, "failure fixture initializes");

    ProductUiFrame invalid = frame(atlas);
    invalid.draw.containsDebug = true;
    expect(renderer.render(invalid).errorCode ==
        "product_ui_gpu_debug_draw_forbidden" && dispatch.begins == 0,
        "debug draw fails before any GPU call");
    invalid = frame(atlas);
    invalid.draw.atlasCacheKey = "wrong";
    expect(renderer.render(invalid).errorCode ==
        "product_ui_gpu_atlas_mismatch" && dispatch.begins == 0,
        "atlas mismatch is explicit before submission");
    invalid = frame(atlas);
    UiPrimitive icon;
    icon.kind = UiPrimitiveKind::Icon;
    icon.rect = {0.0f, 0.0f, 24.0f, 24.0f};
    icon.clipRect = icon.rect;
    invalid.draw.primitives.push_back(icon);
    const ProductUiGpuFrameResult opaque = renderer.render(invalid);
    expect(opaque.errorCode == "product_ui_gpu_opaque_icon_forbidden" &&
        opaque.metrics.rejectedOpaqueIcons == 1 && dispatch.begins == 0,
        "opaque/unexpanded Icon is counted and rejected before submission");
    invalid = frame(atlas);
    invalid.draw.primitives.front().kind =
        static_cast<UiPrimitiveKind>(999);
    expect(renderer.render(invalid).errorCode ==
        "product_ui_gpu_primitive_kind_unsupported" &&
        dispatch.begins == 0,
        "unsupported primitive kinds fail before submission");

    dispatch.uploadError = "injected_upload_failure";
    expect(renderer.render(frame(atlas)).errorCode ==
        "injected_upload_failure" && dispatch.ends == 1,
        "upload failure is returned and frame state is closed");
    dispatch.uploadError.clear();
    dispatch.drawError = "injected_draw_failure";
    expect(renderer.render(frame(atlas)).errorCode ==
        "injected_draw_failure" && dispatch.ends == 2,
        "draw/GL failure is returned and frame state is closed");

    RecordingDispatch createFailure;
    createFailure.createError = "injected_shader_failure";
    ProductUiGpuRenderer failed(createFailure);
    expect(failed.initialize(atlas).errorCode == "injected_shader_failure" &&
        !failed.initialized() && createFailure.destroys == 1,
        "shader/resource initialization failure is explicit and partial handles are cleaned");
}

}  // namespace

int main()
{
    try {
        testSubmissionContractAndMetrics();
        testWorldAnnotationDoesNotClaimInteractiveChrome();
        testProductInsetBilingualInk();
        testResizeContextLossAndZeroDraw();
        testStandardAndCvdColorsRemainUnmodified();
        testFailClosedInputsAndDispatchErrors();
        std::cout << "Product UI GPU recording-dispatch checks passed\n";
        return EXIT_SUCCESS;
    } catch (const std::exception& error) {
        std::cerr << error.what() << '\n';
        return EXIT_FAILURE;
    }
}
