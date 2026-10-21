#include "deterministic_hash.h"
#include "render/modern/trajectory_inset_visible_integration.h"
#include "render/modern/modern_product_ui_runtime.h"
#include "render/modern/modern_renderer_product_ui.h"
#include "render/modern/modern_product_session.h"
#include "render/modern/product_ui_gl_core41.h"
#include "render/ui/product_shell_draw_list.h"
#include "render/ui/product_ui_gpu_renderer.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "training/trajectory_inset_renderer.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <cstdlib>
#include <iostream>
#include <set>
#include <fstream>
#include <stdexcept>
#include <string>
#include <vector>

#if defined(__APPLE__)
#include <OpenGL/gl3.h>
#else
#include <GL/glew.h>
#endif

namespace core41_gl_link_shim {
struct State {
  GLuint nextHandle = 100;
  GLuint productUiProgram = 0;
  GLuint productUiAtlas = 0;
  GLenum pendingError = GL_NO_ERROR;
  bool injectionActive = false;
  bool beginSetupError = false;
  bool restoreError = false;
  bool restoreThrow = false;
  int restoreAttempts = 0;
};
State state;
void reset() { state = {}; state.nextHandle = 100; }
}  // namespace core41_gl_link_shim

extern "C" {
GLenum glGetError(void) {
  const GLenum value = core41_gl_link_shim::state.pendingError;
  core41_gl_link_shim::state.pendingError = GL_NO_ERROR;
  return value;
}
GLboolean glIsEnabled(GLenum) { return GL_FALSE; }
void glGetIntegerv(GLenum name, GLint *value) {
  if (name == GL_CURRENT_PROGRAM) *value = 41;
  else if (name == GL_VERTEX_ARRAY_BINDING) *value = 42;
  else if (name == GL_ARRAY_BUFFER_BINDING) *value = 43;
  else if (name == GL_ACTIVE_TEXTURE) *value = GL_TEXTURE0;
  else if (name == GL_TEXTURE_BINDING_2D) *value = 44;
  else if (name == GL_UNPACK_ALIGNMENT) *value = 4;
  else if (name == GL_BLEND_SRC_RGB || name == GL_BLEND_SRC_ALPHA)
    *value = GL_ONE;
  else if (name == GL_BLEND_DST_RGB || name == GL_BLEND_DST_ALPHA)
    *value = GL_ZERO;
  else if (name == GL_BLEND_EQUATION_RGB || name == GL_BLEND_EQUATION_ALPHA)
    *value = GL_FUNC_ADD;
  else if (name == GL_SCISSOR_BOX) {
    value[0] = 7; value[1] = 9; value[2] = 101; value[3] = 103;
  } else *value = 0;
}
GLuint glCreateShader(GLenum) { return core41_gl_link_shim::state.nextHandle++; }
void glShaderSource(GLuint, GLsizei, const GLchar *const*, const GLint*) {}
void glCompileShader(GLuint) {}
void glGetShaderiv(GLuint, GLenum name, GLint *value) {
  *value = name == GL_COMPILE_STATUS ? GL_TRUE : 0;
}
void glGetShaderInfoLog(GLuint, GLsizei, GLsizei *written, GLchar*) {
  if (written) *written = 0;
}
void glDeleteShader(GLuint) {}
GLuint glCreateProgram(void) {
  core41_gl_link_shim::state.productUiProgram =
      core41_gl_link_shim::state.nextHandle++;
  return core41_gl_link_shim::state.productUiProgram;
}
void glAttachShader(GLuint, GLuint) {}
void glLinkProgram(GLuint) {}
void glGetProgramiv(GLuint, GLenum name, GLint *value) {
  *value = name == GL_LINK_STATUS ? GL_TRUE : 0;
}
void glGetProgramInfoLog(GLuint, GLsizei, GLsizei *written, GLchar*) {
  if (written) *written = 0;
}
void glDeleteProgram(GLuint) {}
void glGenVertexArrays(GLsizei count, GLuint *values) {
  while (count-- > 0) *values++ = core41_gl_link_shim::state.nextHandle++;
}
void glGenBuffers(GLsizei count, GLuint *values) {
  while (count-- > 0) *values++ = core41_gl_link_shim::state.nextHandle++;
}
void glGenTextures(GLsizei count, GLuint *values) {
  while (count-- > 0) {
    *values = core41_gl_link_shim::state.nextHandle++;
    core41_gl_link_shim::state.productUiAtlas = *values++;
  }
}
void glDeleteVertexArrays(GLsizei, const GLuint*) {}
void glDeleteBuffers(GLsizei, const GLuint*) {}
void glDeleteTextures(GLsizei, const GLuint*) {}
void glBindVertexArray(GLuint) {}
void glBindBuffer(GLenum, GLuint) {}
void glEnableVertexAttribArray(GLuint) {}
void glVertexAttribPointer(GLuint, GLint, GLenum, GLboolean, GLsizei,
                           const void*) {}
void glActiveTexture(GLenum) {}
void glBindTexture(GLenum, GLuint texture) {
  auto &shim = core41_gl_link_shim::state;
  if (shim.injectionActive && shim.beginSetupError &&
      texture == shim.productUiAtlas)
    shim.pendingError = GL_INVALID_OPERATION;
}
void glPixelStorei(GLenum, GLint) {}
void glTexParameteri(GLenum, GLenum, GLint) {}
void glTexImage2D(GLenum, GLint, GLint, GLsizei, GLsizei, GLint, GLenum,
                  GLenum, const void*) {}
GLint glGetUniformLocation(GLuint, const GLchar*) { return 0; }
void glUniform1i(GLint, GLint) {}
void glUseProgram(GLuint program) {
  auto &shim = core41_gl_link_shim::state;
  if (program == 41) {
    ++shim.restoreAttempts;
    if (shim.injectionActive && shim.restoreThrow)
      throw std::runtime_error("core41_restore_throw");
    if (shim.injectionActive && shim.restoreError)
      shim.pendingError = GL_INVALID_OPERATION;
  }
}
void glEnable(GLenum) {}
void glDisable(GLenum) {}
void glBlendEquationSeparate(GLenum, GLenum) {}
void glBlendFuncSeparate(GLenum, GLenum, GLenum, GLenum) {}
void glBufferData(GLenum, GLsizeiptr, const void*, GLenum) {}
void glScissor(GLint, GLint, GLsizei, GLsizei) {}
void glDrawArrays(GLenum, GLint, GLsizei) {}
}

namespace game = billiardgl;
namespace modern = billiardgl::render::modern;
namespace training = billiardgl::training;
namespace ui = billiardgl::render::ui;
namespace hud = billiardgl::product_hud;

namespace {

bool sameRect(const hud::HudRect &first, const hud::HudRect &second) {
  return first.x == second.x && first.y == second.y &&
         first.width == second.width && first.height == second.height;
}

template <typename Value>
void appendBits(std::string &output, const Value &value) {
  output.append(reinterpret_cast<const char *>(&value), sizeof(value));
}

void appendField(std::string &output, const std::string &value) {
  const std::uint64_t length = value.size();
  appendBits(output, length);
  output += value;
}

std::string
geometryDigest(const training::TrajectoryInsetDrawPackage &package) {
  std::string canonical;
  for (const auto &vertex : package.vertices) {
    appendBits(canonical, vertex.xPhysicalPx);
    appendBits(canonical, vertex.yPhysicalPx);
    for (const float channel : vertex.linearRgba)
      appendBits(canonical, channel);
    appendBits(canonical, vertex.insetOwnershipToken);
  }
  for (const auto &batch : package.semanticBatches) {
    appendField(canonical, batch.parentSourceKey);
    appendField(canonical, batch.primitiveId);
    appendField(canonical, batch.arcSegmentId);
    appendField(canonical, batch.insetMemberKey);
    appendBits(canonical, batch.track);
    appendBits(canonical, batch.role);
    appendBits(canonical, batch.firstVertex);
    appendBits(canonical, batch.vertexCount);
    appendBits(canonical, batch.insetOwnershipToken);
  }
  return billiardgl::sha256Hex(canonical);
}

std::string layoutDigest(const ui::TrajectoryInsetShellLayout &layout) {
  std::string canonical;
  const auto rect = [&](const hud::HudRect &value) {
    appendBits(canonical, value.x);
    appendBits(canonical, value.y);
    appendBits(canonical, value.width);
    appendBits(canonical, value.height);
  };
  appendField(canonical, layout.profileId);
  appendBits(canonical, layout.insetVisible);
  appendBits(canonical, layout.insetControlOn);
  appendBits(canonical, layout.worldOverlayVisible);
  appendBits(canonical, layout.worldOverlayControlOn);
  rect(layout.authoritativePanelRect);
  rect(layout.panelRect);
  rect(layout.plotCutoutRect);
  rect(layout.headerRect);
  rect(layout.titleRect);
  for (const auto &value : layout.orientationLabelRects)
    rect(value);
  for (const auto &value : layout.legendRects)
    rect(value);
  rect(layout.toggleRect);
  rect(layout.toggleLabelRect);
  rect(layout.focusRect);
  for (const auto &value : layout.borderRects)
    rect(value);
  appendField(canonical, layout.title);
  appendField(canonical, layout.toggleLabel);
  for (const auto &value : layout.orientationLabels)
    appendField(canonical, value);
  for (const auto &value : layout.legendEntries)
    appendField(canonical, value);
  return billiardgl::sha256Hex(canonical);
}

std::string exclusionDigest(const ui::TrajectoryInsetShellRequest &request) {
  std::string canonical;
  appendBits(canonical, request.exclusions.size());
  for (const auto &exclusion : request.exclusions) {
    appendField(canonical, exclusion.id);
    appendBits(canonical, exclusion.active);
    appendBits(canonical, exclusion.rect.x);
    appendBits(canonical, exclusion.rect.y);
    appendBits(canonical, exclusion.rect.width);
    appendBits(canonical, exclusion.rect.height);
  }
  return billiardgl::sha256Hex(canonical);
}

void require(bool condition, const std::string &message) {
  if (!condition) {
    std::cerr << "FAIL " << message << '\n';
    std::exit(1);
  }
}

struct Profile {
  const char *id;
  int width;
  int height;
  float uiScale;
};

constexpr std::array<Profile, 4> kProfiles{{
    {"1920x1080_ui100", 1920, 1080, 1.0f},
    {"1920x1080_ui135", 1920, 1080, 1.35f},
    {"1280x720_ui100", 1280, 720, 1.0f},
    {"1280x720_ui135", 1280, 720, 1.35f},
}};

ui::TrajectoryInsetShellRequest shellRequest(const Profile &profile) {
  ui::TrajectoryInsetShellRequest request;
  request.profileId = profile.id;
  request.physicalWidth = profile.width;
  request.physicalHeight = profile.height;
  request.uiScale = profile.uiScale;
  request.exclusions = {
      {"player status HUD", {32, 24, 420, 108}},
      {"shot power and tip controls",
       {32, static_cast<float>(profile.height) - 96, 600, 72}},
      {"rules decision panel or modal", {500, 160, 420, 360}, false},
      {"caption and accessibility live-region band",
       {360, static_cast<float>(profile.height) - 80, 560, 48}},
      {"safe-area edges and product footer",
       {0, static_cast<float>(profile.height) - 112 * profile.uiScale,
        static_cast<float>(profile.width), 112 * profile.uiScale}},
  };
  return request;
}

training::TrajectoryOverlaySnapshot coaxialFixture() {
  training::TrajectoryOverlaySnapshot snapshot;
  snapshot.actual.visible = snapshot.predicted.visible = true;
  snapshot.actual.endpointVisible = snapshot.predicted.endpointVisible = true;
  snapshot.actual.railCollisionEvents = snapshot.predicted.railCollisionEvents =
      2;
  snapshot.actual.encoding.lineStyle =
      training::TrajectoryLineStyle::SolidWithMarkers;
  snapshot.predicted.encoding.lineStyle = training::TrajectoryLineStyle::Dashed;
  for (std::size_t index = 0; index < 5; ++index) {
    const float z = -127.0f + 63.5f * static_cast<float>(index);
    const training::OverlaySample sample{
        static_cast<std::uint64_t>(index), {0.0f, 86.5f, z}, true};
    snapshot.actual.samples.push_back(sample);
    snapshot.predicted.samples.push_back(sample);
  }
  snapshot.actual.endpoint = snapshot.actual.samples.back().position;
  snapshot.predicted.endpoint = snapshot.predicted.samples.back().position;
  return snapshot;
}

training::TrajectoryInsetLayout
insetLayout(const ui::TrajectoryInsetShellLayout &layout,
            const Profile &profile) {
  return {profile.id,
          profile.width,
          profile.height,
          profile.uiScale,
          {layout.panelRect.x, layout.panelRect.y, layout.panelRect.width,
           layout.panelRect.height},
          {layout.plotCutoutRect.x, layout.plotCutoutRect.y,
           layout.plotCutoutRect.width, layout.plotCutoutRect.height}};
}

struct CurrentFrame {
  ui::TrajectoryInsetShellRequest shellRequest;
  ui::TrajectoryInsetShellLayout layout;
  ui::TrajectoryInsetChromeDrawList chrome;
  training::TrajectoryInsetDrawPackage package;
  modern::TrajectoryInsetVisibleFrameRequest request;
};

CurrentFrame currentFrame(const Profile &profile, game::ProductScreen screen,
                          training::TrajectoryInsetPalette palette, int samples,
                          std::uint64_t serial) {
  CurrentFrame frame;
  frame.shellRequest = shellRequest(profile);
  frame.layout = ui::buildTrajectoryInsetShellLayout(frame.shellRequest);
  frame.chrome = ui::buildTrajectoryInsetChromeDrawList(frame.layout);
  training::TrajectoryInsetBuildRequest build;
  build.generationId = "v19_generation_" + std::to_string(serial);
  build.snapshot = coaxialFixture();
  build.layout = insetLayout(frame.layout, profile);
  build.palette = palette;
  frame.package = training::buildTrajectoryInsetDrawPackage(build);
  require(frame.layout.ok && frame.chrome.ok && frame.package.ok,
          "fixture foundations valid");
  frame.request.present = true;
  frame.request.sessionSerial = serial;
  frame.request.frameSerial = serial;
  frame.request.layoutSerial = serial;
  frame.request.screen = screen;
  frame.request.sessionStable = true;
  frame.request.insetToggleOn = true;
  frame.request.worldOverlayToggleOn = false;
  frame.request.samples = samples;
  frame.request.palette = palette;
  frame.request.package = &frame.package;
  frame.request.shellRequest = &frame.shellRequest;
  frame.request.layout = &frame.layout;
  frame.request.chrome = &frame.chrome;
  frame.request.cp1GenerationId = frame.package.generationId;
  frame.request.cp1Digests = {{frame.package.orderedParentSourceDigest,
                               frame.package.orderedProbeDigest,
                               frame.package.orderedDashEventDigest,
                               frame.package.orderedDashContributorDigest,
                               frame.package.orderedSourceRouteIntervalDigest}};
  frame.request.cp2ProfileId = frame.layout.profileId;
  frame.request.cp1GeometryDigest = geometryDigest(frame.package);
  frame.request.cp2LayoutDigest = layoutDigest(frame.layout);
  frame.request.cp2ExclusionDigest = exclusionDigest(frame.shellRequest);
  return frame;
}

class Dispatch final : public modern::TrajectoryInsetVisibleDispatch {
public:
  modern::TrajectoryInsetVisibleStage failStage =
      modern::TrajectoryInsetVisibleStage::None;
  modern::TrajectoryInsetVisibleStage errorStage =
      modern::TrajectoryInsetVisibleStage::None;
  modern::TrajectoryInsetVisibleStage throwStage =
      modern::TrajectoryInsetVisibleStage::None;
  std::uint64_t nextResource = 1000;
  std::uint64_t state = 0x1234;
  std::uint64_t preparedSessionSerialDelta = 0;
  std::uint64_t preparedLayoutSerialDelta = 0;
  std::uint64_t preparedFrameSerialDelta = 0;
  std::uint64_t preparedResourceGenerationDelta = 0;
  bool driftPreparedGenerationId = false;
  bool driftPreparedDigest = false;
  bool driftPreparedProfile = false;
  bool driftPreparedGeometryDigest = false;
  bool driftPreparedLayoutDigest = false;
  bool driftPreparedExclusionDigest = false;
  int prepareCalls = 0, plotStageCalls = 0, chromeStageCalls = 0,
      presentCalls = 0, worldCalls = 0, lifecycleInvalidations = 0,
      restoreCalls = 0;
  int visiblePlotWrites = 0, visibleSemanticWrites = 0, visibleChromeWrites = 0;
  bool retainedResources = false;
  std::uint64_t retainedStagingResource = 0, invalidatedStagingResource = 0;
  std::vector<std::string> calls;

  std::uint64_t snapshotState() override {
    calls.push_back("snapshot");
    return state;
  }
  bool invoke(modern::TrajectoryInsetVisibleStage stage,
              const modern::TrajectoryInsetVisibleFrameRequest &request,
              modern::TrajectoryInsetVisibleWorkingSet &working,
              std::string &error) override {
    calls.push_back(modern::trajectoryInsetVisibleStageName(stage));
    state ^= static_cast<std::uint64_t>(stage) + 1;
    if (stage == throwStage)
      throw std::runtime_error("injected_visible_helper_throw");
    if (stage == failStage) {
      error = "injected_visible_stage_failure";
      return false;
    }
    if (stage == modern::TrajectoryInsetVisibleStage::PrepareCp3) {
      ++prepareCalls;
      working.preparation.ok = true;
      working.preparation.committedOffscreen = true;
      working.preparation.visibleCompositePerformed = false;
      working.preparation.stateRestored = true;
      working.preparation.frameSerial =
          request.frameSerial + preparedFrameSerialDelta;
      working.preparation.resourceGeneration =
          request.frameSerial + preparedResourceGenerationDelta;
      working.preparation.errorCode.clear();
      working.preparedSessionSerial =
          request.sessionSerial + preparedSessionSerialDelta;
      working.preparedLayoutSerial =
          request.layoutSerial + preparedLayoutSerialDelta;
      working.preparedCp1GenerationId =
          driftPreparedGenerationId ? "stale" : request.cp1GenerationId;
      working.preparedCp1Digests = request.cp1Digests;
      if (driftPreparedDigest)
        working.preparedCp1Digests[2] = "drift";
      working.preparedCp2ProfileId =
          driftPreparedProfile ? "foreign" : request.cp2ProfileId;
      working.preparedCp1GeometryDigest =
          driftPreparedGeometryDigest ? "drift" : request.cp1GeometryDigest;
      working.preparedCp2LayoutDigest =
          driftPreparedLayoutDigest ? "drift" : request.cp2LayoutDigest;
      working.preparedCp2ExclusionDigest =
          driftPreparedExclusionDigest ? "drift" : request.cp2ExclusionDigest;
      working.cp3StagingResource = nextResource++;
    } else if (stage == modern::TrajectoryInsetVisibleStage::StagePlot) {
      ++plotStageCalls;
      working.stagedPlotPixels = 1;
      working.stagedInsetTokens = request.package->probeDenominator;
    } else if (stage == modern::TrajectoryInsetVisibleStage::StageChrome) {
      ++chromeStageCalls;
      working.stagedChromePrimitives = request.chrome->primitives.size();
    }
    return true;
  }
  unsigned readError(modern::TrajectoryInsetVisibleStage stage) override {
    return stage == errorStage ? 1282u : 0u;
  }
  bool restoreState(std::uint64_t saved, std::string &error) override {
    calls.push_back("restore");
    ++restoreCalls;
    if (throwStage == modern::TrajectoryInsetVisibleStage::RestoreState)
      throw std::runtime_error("injected_restore_throw");
    if (failStage == modern::TrajectoryInsetVisibleStage::RestoreState) {
      error = "injected_restore_failure";
      return false;
    }
    state = saved;
    return true;
  }
  unsigned readRestoreError() override {
    return errorStage == modern::TrajectoryInsetVisibleStage::RestoreState
               ? 1282u
               : 0u;
  }
  bool presentAtomic(const modern::TrajectoryInsetVisibleFrameRequest &request,
                     const modern::TrajectoryInsetVisibleWorkingSet &working,
                     std::string &error) override {
    calls.push_back("atomic_present");
    ++presentCalls;
    if (throwStage == modern::TrajectoryInsetVisibleStage::AtomicPresent)
      throw std::runtime_error("injected_present_throw");
    if (failStage == modern::TrajectoryInsetVisibleStage::AtomicPresent ||
        errorStage == modern::TrajectoryInsetVisibleStage::AtomicPresent) {
      error = "injected_present_fail_before_write";
      return false;
    }
    if (working.preparation.frameSerial != request.frameSerial ||
        working.preparation.resourceGeneration != request.frameSerial ||
        working.preparedSessionSerial != request.sessionSerial ||
        working.preparedLayoutSerial != request.layoutSerial ||
        working.preparedCp1GenerationId != request.cp1GenerationId ||
        working.preparedCp1Digests != request.cp1Digests ||
        working.preparedCp2ProfileId != request.cp2ProfileId ||
        working.preparedCp1GeometryDigest != request.cp1GeometryDigest ||
        working.preparedCp2LayoutDigest != request.cp2LayoutDigest ||
        working.preparedCp2ExclusionDigest != request.cp2ExclusionDigest ||
        working.stagedPlotPixels == 0 || working.stagedInsetTokens == 0 ||
        working.stagedChromePrimitives == 0) {
      error = "visible_present_precondition_invalid";
      return false;
    }
    visiblePlotWrites = 1;
    visibleSemanticWrites = 1;
    visibleChromeWrites = 1;
    retainedResources = true;
    retainedStagingResource = working.cp3StagingResource;
    return true;
  }
  void invalidate(modern::TrajectoryInsetVisibleWorkingSet &working) override {
    calls.push_back("invalidate");
    working = {};
    visiblePlotWrites = visibleSemanticWrites = visibleChromeWrites = 0;
    retainedResources = false;
    retainedStagingResource = 0;
  }
  void invalidateLifecycle(
      modern::TrajectoryInsetVisibleInvalidation) noexcept override {
    calls.push_back("invalidate_lifecycle");
    ++lifecycleInvalidations;
    invalidatedStagingResource = retainedStagingResource;
    visiblePlotWrites = visibleSemanticWrites = visibleChromeWrites = 0;
    retainedResources = false;
    retainedStagingResource = 0;
  }
};

void positive48() {
  std::uint64_t serial = 100;
  int cases = 0;
  for (auto screen : {game::ProductScreen::TrainingSession,
                      game::ProductScreen::ReplayPlayback})
    for (const auto &profile : kProfiles)
      for (auto palette : {training::TrajectoryInsetPalette::Standard,
                           training::TrajectoryInsetPalette::ColorVisionSafe})
        for (int samples : {1, 2, 4}) {
          auto frame =
              currentFrame(profile, screen, palette, samples, serial++);
          Dispatch dispatch;
          const auto saved = dispatch.state;
          modern::TrajectoryInsetVisibleIntegration integration(dispatch);
          const auto result = integration.execute(frame.request);
          if (!result.ok)
            std::cerr << "positive failure profile=" << profile.id
                      << " samples=" << samples
                      << " palette=" << static_cast<int>(palette)
                      << " error=" << result.errorCode << '\n';
          require(result.ok && result.presented && result.stateRestored,
                  "48-case current integration succeeds");
          require(dispatch.prepareCalls == 1 && dispatch.plotStageCalls == 1 &&
                      dispatch.chromeStageCalls == 1 &&
                      dispatch.presentCalls == 1 && dispatch.restoreCalls == 1 &&
                      dispatch.worldCalls == 0,
                  "exact prepare plot chrome present counts");
          require(dispatch.visiblePlotWrites == 1 &&
                      dispatch.visibleSemanticWrites == 1 &&
                      dispatch.visibleChromeWrites == 1 &&
                      dispatch.state == saved,
                  "atomic visible ledgers and state restore");
          require(result.orderedLedger ==
                      std::vector<std::string>{
                          "freeze_current_identity", "audit_cp1_cp2",
                          "prepare_cp3", "verify_composite_eligible",
                          "stage_plot", "stage_chrome", "restore_state",
                          "atomic_present", "sealed"},
                  "frozen ordering");
          ++cases;
        }
  require(cases == 48, "exact positive matrix");
}

void rejectedScreensAndAuthority() {
  const std::array<game::ProductScreen, 7> rejected{{
      game::ProductScreen::MainMenu,
      game::ProductScreen::SinglePlayerSetup,
      game::ProductScreen::Match,
      game::ProductScreen::TrainingCatalog,
      game::ProductScreen::ReplayBrowser,
      game::ProductScreen::Settings,
      game::ProductScreen::Help,
  }};
  std::uint64_t serial = 400;
  for (const auto screen : rejected) {
    auto frame =
        currentFrame(kProfiles[0], screen,
                     training::TrajectoryInsetPalette::Standard, 1, serial++);
    Dispatch dispatch;
    modern::TrajectoryInsetVisibleIntegration integration(dispatch);
    const auto result = integration.execute(frame.request);
    require(!result.ok && dispatch.prepareCalls == 0 &&
                dispatch.presentCalls == 0 && dispatch.worldCalls == 0,
            "nonaccepted screen has zero CP3/visible/world calls");
  }
  for (int mutation = 0; mutation < 11; ++mutation) {
    auto frame =
        currentFrame(kProfiles[0], game::ProductScreen::TrainingSession,
                     training::TrajectoryInsetPalette::Standard, 2, serial++);
    if (mutation == 0)
      frame.request.present = false;
    if (mutation == 1)
      frame.request.insetToggleOn = false;
    if (mutation == 2)
      frame.request.worldOverlayToggleOn = true;
    if (mutation == 3)
      frame.request.sessionStable = false;
    if (mutation == 4)
      frame.request.transitioning = true;
    if (mutation == 5)
      frame.request.errored = true;
    if (mutation == 6)
      frame.request.modalVisible = true;
    if (mutation == 7)
      frame.request.frameSerial = 0;
    if (mutation == 8)
      frame.request.layoutSerial = frame.request.frameSerial - 1;
    if (mutation == 9)
      frame.request.sessionSerial = 0;
    if (mutation == 10)
      frame.request.sessionSerial = frame.request.frameSerial - 1;
    Dispatch dispatch;
    modern::TrajectoryInsetVisibleIntegration integration(dispatch);
    const auto result = integration.execute(frame.request);
    require(!result.ok && dispatch.prepareCalls == 0 &&
                dispatch.presentCalls == 0 && dispatch.worldCalls == 0,
            "absent/off/world/modal/serial authority rejects before prepare");
  }
}

void identityMutations() {
  std::uint64_t serial = 600;
  for (int mutation = 0; mutation < 16; ++mutation) {
    auto frame = currentFrame(kProfiles[1], game::ProductScreen::ReplayPlayback,
                              training::TrajectoryInsetPalette::ColorVisionSafe,
                              4, serial++);
    if (mutation == 0)
      frame.request.cp1GenerationId = "stale";
    if (mutation >= 1 && mutation <= 5)
      frame.request.cp1Digests[mutation - 1] = "drift";
    if (mutation == 6)
      frame.request.cp2ProfileId = "foreign";
    if (mutation == 7)
      frame.shellRequest.exclusions.pop_back();
    if (mutation == 8)
      frame.shellRequest.exclusions.push_back({"foreign", {0, 0, 1, 1}, true});
    if (mutation == 9)
      std::swap(frame.shellRequest.exclusions[0],
                frame.shellRequest.exclusions[1]);
    if (mutation == 10)
      frame.layout.plotCutoutRect.x += 1;
    if (mutation == 11)
      frame.chrome.primitives.pop_back();
    if (mutation == 12)
      frame.package.semanticMemberDenominator++;
    if (mutation == 13)
      frame.package.semanticBatches[0].insetOwnershipToken = 0;
    if (mutation == 14)
      frame.shellRequest.exclusions[2].rect.x += 1.0f;
    if (mutation == 15)
      frame.shellRequest.exclusions[2].active =
          !frame.shellRequest.exclusions[2].active;
    Dispatch dispatch;
    modern::TrajectoryInsetVisibleIntegration integration(dispatch);
    const auto result = integration.execute(frame.request);
    require(!result.ok && dispatch.presentCalls == 0,
            "CP1/CP2 current identity mutation fails closed:" +
                std::to_string(mutation) + ":" + result.errorCode);
  }
}

void preparationIdentityMutations() {
  std::uint64_t serial = 740;
  for (int mutation = 0; mutation < 10; ++mutation) {
    auto frame =
        currentFrame(kProfiles[0], game::ProductScreen::TrainingSession,
                     training::TrajectoryInsetPalette::Standard, 2, serial++);
    Dispatch dispatch;
    if (mutation == 0)
      dispatch.preparedSessionSerialDelta = 1;
    if (mutation == 1)
      dispatch.preparedLayoutSerialDelta = 1;
    if (mutation == 2)
      dispatch.preparedFrameSerialDelta = 1;
    if (mutation == 3)
      dispatch.preparedResourceGenerationDelta = 1;
    if (mutation == 4)
      dispatch.driftPreparedGenerationId = true;
    if (mutation == 5)
      dispatch.driftPreparedDigest = true;
    if (mutation == 6)
      dispatch.driftPreparedProfile = true;
    if (mutation == 7)
      dispatch.driftPreparedGeometryDigest = true;
    if (mutation == 8)
      dispatch.driftPreparedLayoutDigest = true;
    if (mutation == 9)
      dispatch.driftPreparedExclusionDigest = true;
    modern::TrajectoryInsetVisibleIntegration integration(dispatch);
    const auto result = integration.execute(frame.request);
    require(!result.ok && !result.presented && dispatch.presentCalls == 0,
            "current CP3 preparation identity drift fails before publish");
  }
}

void validFailValid() {
  const auto stages = modern::orderedTrajectoryInsetVisibleFailureStages();
  std::uint64_t serial = 800;
  for (const auto stage : stages) {
    Dispatch dispatch;
    modern::TrajectoryInsetVisibleIntegration integration(dispatch);
    auto first =
        currentFrame(kProfiles[2], game::ProductScreen::TrainingSession,
                     training::TrajectoryInsetPalette::Standard, 1, serial++);
    require(integration.execute(first.request).ok, "N succeeds");
    dispatch.visiblePlotWrites = dispatch.visibleSemanticWrites =
        dispatch.visibleChromeWrites = 0;
    auto failed =
        currentFrame(kProfiles[2], game::ProductScreen::TrainingSession,
                     training::TrajectoryInsetPalette::Standard, 1, serial++);
    dispatch.failStage = stage;
    const auto failure = integration.execute(failed.request);
    require(!failure.ok && !failure.presented &&
                dispatch.visiblePlotWrites == 0 &&
                dispatch.visibleSemanticWrites == 0 &&
                dispatch.visibleChromeWrites == 0 && dispatch.worldCalls == 0,
            "N+1 has zero visible/semantic/chrome/world residue");
    dispatch.failStage = modern::TrajectoryInsetVisibleStage::None;
    auto third =
        currentFrame(kProfiles[2], game::ProductScreen::TrainingSession,
                     training::TrajectoryInsetPalette::Standard, 1, serial++);
    require(integration.execute(third.request).ok, "N+2 fresh succeeds");
  }
}

void errorsThrowsAndLifecycle() {
  std::uint64_t serial = 1200;
  for (const auto stage :
       modern::orderedTrajectoryInsetVisibleFailureStages()) {
    for (bool helperThrow : {false, true}) {
      auto frame = currentFrame(
          kProfiles[3], game::ProductScreen::ReplayPlayback,
          training::TrajectoryInsetPalette::ColorVisionSafe, 4, serial++);
      Dispatch dispatch;
      if (helperThrow)
        dispatch.throwStage = stage;
      else
        dispatch.errorStage = stage;
      modern::TrajectoryInsetVisibleIntegration integration(dispatch);
      const auto result = integration.execute(frame.request);
      require(!result.ok && !result.presented &&
                  dispatch.visiblePlotWrites == 0 &&
                  dispatch.visibleSemanticWrites == 0 &&
                  dispatch.visibleChromeWrites == 0 &&
                  dispatch.restoreCalls == 1,
              "GL error/helper throw fails without residue");
    }
  }
  for (auto invalidation :
       {modern::TrajectoryInsetVisibleInvalidation::Resize,
        modern::TrajectoryInsetVisibleInvalidation::ContextLoss,
        modern::TrajectoryInsetVisibleInvalidation::ScreenChange,
        modern::TrajectoryInsetVisibleInvalidation::ModalChange,
        modern::TrajectoryInsetVisibleInvalidation::ToggleChange}) {
    Dispatch dispatch;
    modern::TrajectoryInsetVisibleIntegration integration(dispatch);
    auto frame =
        currentFrame(kProfiles[0], game::ProductScreen::TrainingSession,
                     training::TrajectoryInsetPalette::Standard, 2, serial++);
    require(integration.execute(frame.request).ok &&
                integration.hasReusableEligibility() &&
                dispatch.retainedResources &&
                dispatch.retainedStagingResource != 0,
            "lifecycle fixture starts from retained CP3 resources");
    const auto staleStaging = dispatch.retainedStagingResource;
    integration.invalidate(invalidation);
    require(!integration.hasReusableEligibility() &&
                dispatch.lifecycleInvalidations == 1 &&
                !dispatch.retainedResources && dispatch.visiblePlotWrites == 0 &&
                dispatch.visibleSemanticWrites == 0 &&
                dispatch.visibleChromeWrites == 0 &&
                dispatch.retainedStagingResource == 0 &&
                dispatch.invalidatedStagingResource == staleStaging,
            "lifecycle invalidation clears CP3 staging and visible ledgers");
    auto fresh =
        currentFrame(kProfiles[0], game::ProductScreen::TrainingSession,
                     training::TrajectoryInsetPalette::Standard, 2, serial++);
    require(integration.execute(fresh.request).ok && dispatch.retainedResources &&
                dispatch.retainedStagingResource != 0 &&
                dispatch.retainedStagingResource != staleStaging,
            "post-invalidation frame prepares fresh retained resources");
  }
}

ui::TrajectoryInsetVisibleCurrentContext checkpointBContext(
    std::uint64_t serial) {
  ui::TrajectoryInsetVisibleCurrentContext context;
  context.present = true;
  context.sessionSerial = serial;
  context.frameSerial = serial;
  context.layoutSerial = serial;
  context.sessionStable = true;
  context.insetToggleOn = true;
  context.worldOverlayToggleOn = false;
  context.samples = 1;
  context.palette = training::TrajectoryInsetPalette::Standard;
  context.cp1GenerationId = "v19_current_" + std::to_string(serial);
  return context;
}

billiardgl::render::ModernProductSessionSnapshot checkpointBSession(
    game::ProductScreen screen) {
  billiardgl::render::ModernProductSessionSnapshot session;
  session.shell.screen = screen;
  session.shell.trajectoryOverlay = coaxialFixture();
  session.shell.uiScale = 1.0f;
  session.shellViewport.physicalWidth = 1920;
  session.shellViewport.physicalHeight = 1080;
  session.shellViewport.dpi = 96.0f;
  return session;
}

struct CheckpointBDispatch {
  enum class Failure { None, Adapter, Prepare, Publish, Chrome } failure =
      Failure::None;
  int renderInvocations = 0, chromeSubmissions = 0, invalidations = 0;
  int visiblePlotWrites = 0, visibleTokenWrites = 0, visibleChromeWrites = 0;
  std::vector<std::string> ledger;

  modern::TrajectoryInsetVisibleResult execute(
      const modern::TrajectoryInsetVisibleFrameRequest &) {
    ++renderInvocations;
    modern::TrajectoryInsetVisibleResult result;
    if (failure == Failure::Adapter || failure == Failure::Prepare ||
        failure == Failure::Publish) {
      result.errorCode = "injected_checkpoint_b_integration_failure";
      return result;
    }
    result.ok = result.presented = result.stateRestored = true;
    ledger.push_back("atomic_plot_publish");
    return result;
  }
  bool submitChrome(const ui::TrajectoryInsetShellLayout &layout,
                    const ui::TrajectoryInsetChromeDrawList &chrome,
                    std::string &error) {
    ++chromeSubmissions;
    if (failure == Failure::Chrome) {
      error = "injected_checkpoint_b_chrome_failure";
      return false;
    }
    const std::vector<std::string> orderedText{
        "TRAJECTORY / 轨迹", "HEAD / 开球端", "FOOT / 底端",
        "ACTUAL / 实际 — paired continuous rails",
        "PREDICTED / 预测 — centerline dashes",
        "END / 终点 — endpoint marker",
        "Trajectory inset / 轨迹小窗"};
    if (chrome.orderedText != orderedText ||
        !ui::auditTrajectoryInsetChromeDrawList(layout, chrome).ok) {
      error = "checkpoint_b_chrome_inventory_missing";
      return false;
    }
    const auto exact = [&](const std::string &token,
                           ui::TrajectoryInsetChromeKind kind) {
      return std::count_if(chrome.primitives.begin(), chrome.primitives.end(),
                           [&](const auto &primitive) {
                             return primitive.token == token &&
                                    primitive.kind == kind;
                           }) == 1;
    };
    if (!exact("trajectory_inset_toggle",
               ui::TrajectoryInsetChromeKind::Toggle)) {
      error = "checkpoint_b_chrome_inventory_missing";
      return false;
    }
    for (int index = 0; index < 4; ++index)
      if (!exact("trajectory_inset_border_" + std::to_string(index),
                 ui::TrajectoryInsetChromeKind::Border)) {
        error = "checkpoint_b_chrome_inventory_missing";
        return false;
      }
    for (int index = 0; index < 7; ++index)
      if (!exact("trajectory_inset_text_" + std::to_string(index),
                 ui::TrajectoryInsetChromeKind::Text)) {
        error = "checkpoint_b_chrome_inventory_missing";
        return false;
      }
    const std::array<hud::HudRect, 8> expectedRects{{
        layout.titleRect, layout.orientationLabelRects[0],
        layout.orientationLabelRects[1], layout.legendRects[0],
        layout.legendRects[1], layout.legendRects[2], layout.toggleLabelRect,
        layout.toggleRect}};
    for (std::size_t index = 0; index < expectedRects.size(); ++index) {
      const std::string token = index == 7
                                    ? "trajectory_inset_toggle"
                                    : "trajectory_inset_text_" +
                                          std::to_string(index);
      const auto found = std::find_if(chrome.primitives.begin(),
                                      chrome.primitives.end(), [&](const auto &p) {
                                        return p.token == token;
                                      });
      const auto &expected = expectedRects[index];
      if (found == chrome.primitives.end() ||
          std::abs(found->rect.x - expected.x) > 1e-4f ||
          std::abs(found->rect.y - expected.y) > 1e-4f ||
          std::abs(found->rect.width - expected.width) > 1e-4f ||
          std::abs(found->rect.height - expected.height) > 1e-4f) {
        error = "checkpoint_b_chrome_rect_mismatch";
        return false;
      }
    }
    ledger.push_back("product_ui_chrome_submit");
    return true;
  }
  void commitVisible() {
    visiblePlotWrites = visibleTokenWrites = visibleChromeWrites = 1;
  }
  void invalidate() noexcept {
    ++invalidations;
    visiblePlotWrites = visibleTokenWrites = visibleChromeWrites = 0;
  }
};

ui::TrajectoryInsetVisibleProductCallbacks callbacks(CheckpointBDispatch &d) {
  ui::TrajectoryInsetVisibleProductCallbacks value;
  value.executeIntegration = [&](const auto &request) {
    return d.execute(request);
  };
  value.submitChrome = [&](const auto &layout, const auto &chrome,
                           std::string &error) {
    return d.submitChrome(layout, chrome, error);
  };
  value.commitVisible = [&] { d.commitVisible(); };
  value.invalidate = [&] { d.invalidate(); };
  return value;
}

void checkpointBExplicitCurrentRequestAdapter() {
  const billiardgl::PresentationSnapshot presentation;
  std::uint64_t serial = 2000;
  for (const auto screen : {game::ProductScreen::TrainingSession,
                            game::ProductScreen::ReplayPlayback}) {
    auto session = checkpointBSession(screen);
    auto context = checkpointBContext(serial++);
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        session, presentation, {true, false, false}, context);
    require(current.ok && current.request.present &&
                current.request.screen == screen &&
                current.request.sessionSerial == context.sessionSerial &&
                current.request.frameSerial == context.frameSerial &&
                current.request.layoutSerial == context.layoutSerial &&
                current.request.cp1GenerationId == current.package.generationId &&
                current.request.cp1Digests[0] ==
                    current.package.orderedParentSourceDigest &&
                current.request.cp1GeometryDigest.size() == 64 &&
                current.request.cp2LayoutDigest.size() == 64 &&
                current.request.cp2ExclusionDigest.size() == 64 &&
                !current.request.worldOverlayToggleOn,
            "Training and Replay produce exact explicit current identity");
    CheckpointBDispatch dispatch;
    const auto shown = ui::executeTrajectoryInsetVisibleCurrentFrame(
        current, callbacks(dispatch));
    require(shown.ok && dispatch.renderInvocations == 1 &&
                dispatch.chromeSubmissions == 1 &&
                dispatch.ledger == std::vector<std::string>{
                                       "atomic_plot_publish",
                                       "product_ui_chrome_submit"} &&
                dispatch.visiblePlotWrites == 1 &&
                dispatch.visibleTokenWrites == 1 &&
                dispatch.visibleChromeWrites == 1,
            "accepted explicit frame invokes once and publishes before chrome");
    const auto repeated = ui::executeTrajectoryInsetVisibleCurrentFrame(
        current, callbacks(dispatch));
    require(!repeated.ok && dispatch.renderInvocations == 1,
            "same explicit frame is consumed at most once");
  }

  {
    auto focusedContext = checkpointBContext(serial++);
    focusedContext.insetControlFocused = true;
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, focusedContext);
    require(current.ok && current.layout.insetControlFocused &&
                std::count_if(current.chrome.primitives.begin(),
                              current.chrome.primitives.end(), [](const auto &p) {
                                return p.token == "trajectory_inset_focus" &&
                                       p.kind == ui::TrajectoryInsetChromeKind::Focus;
                              }) == 1,
            "focused current frame has exact focus primitive");
  }

  const std::array<game::ProductScreen, 7> rejected{{
      game::ProductScreen::MainMenu,
      game::ProductScreen::SinglePlayerSetup,
      game::ProductScreen::Match,
      game::ProductScreen::TrainingCatalog,
      game::ProductScreen::ReplayBrowser,
      game::ProductScreen::Settings,
      game::ProductScreen::Help,
  }};
  for (const auto screen : rejected) {
    const auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(screen), presentation, {true, false, false},
        checkpointBContext(serial++));
    CheckpointBDispatch dispatch;
    require(!current.ok && dispatch.renderInvocations == 0,
            "exact seven other screens build no visible request");
  }

  for (int mutation = 0; mutation < 10; ++mutation) {
    auto context = checkpointBContext(serial++);
    ui::ProductUiFrameEnvironment environment{true, false, false};
    if (mutation == 0)
      context.present = false;
    if (mutation == 1)
      context.insetToggleOn = false;
    if (mutation == 2)
      context.worldOverlayToggleOn = true;
    if (mutation == 3)
      context.modalVisible = true;
    if (mutation == 4)
      context.transitioning = true;
    if (mutation == 5)
      context.errored = true;
    if (mutation == 6)
      context.sessionSerial = 0;
    if (mutation == 7)
      environment.automation = true;
    if (mutation == 8)
      environment.windowless = true;
    if (mutation == 9)
      environment.productMode = false;
    const auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        environment, context);
    CheckpointBDispatch dispatch;
    require(!current.ok && dispatch.renderInvocations == 0 &&
                dispatch.visiblePlotWrites == 0 &&
                dispatch.visibleTokenWrites == 0 &&
                dispatch.visibleChromeWrites == 0,
            "absent off world modal transition error automation windowless zero");
  }

  {
    std::ifstream input("src/Billiards/render/modern/modern_renderer.cpp");
    const std::string source((std::istreambuf_iterator<char>(input)),
                             std::istreambuf_iterator<char>());
    const auto occurrenceCount = [&](const std::string &needle) {
      std::size_t count = 0, position = 0;
      while ((position = source.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
      }
      return count;
    };
    require(occurrenceCount("executeTrajectoryInsetVisibleCurrentFrame(") == 1 &&
                source.find("makeTrajectoryInsetVisibleCurrentFrame(") !=
                    std::string::npos &&
                source.find("trajectoryInsetVisibleRequest") !=
                    std::string::npos,
            "modern renderFrame must expose explicit current request callsite");
    require(source.find("screen == ProductScreen::TrainingSession ||") ==
                std::string::npos,
            "screen branch alone may not enable visible integration");
  }

  for (const auto failure : {CheckpointBDispatch::Failure::Adapter,
                             CheckpointBDispatch::Failure::Prepare,
                             CheckpointBDispatch::Failure::Publish,
                             CheckpointBDispatch::Failure::Chrome}) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointBDispatch dispatch;
    dispatch.failure = failure;
    const auto failed = ui::executeTrajectoryInsetVisibleCurrentFrame(
        current, callbacks(dispatch));
    require(!failed.ok && dispatch.visiblePlotWrites == 0 &&
                dispatch.visibleTokenWrites == 0 &&
                dispatch.visibleChromeWrites == 0 &&
                dispatch.invalidations == 1,
            "adapter prepare publish chrome failures leave zero residue");
    dispatch.failure = CheckpointBDispatch::Failure::None;
    auto fresh = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    require(ui::executeTrajectoryInsetVisibleCurrentFrame(
                fresh, callbacks(dispatch)).ok,
            "failure requires and accepts a fresh serial");
  }
}

class CheckpointCDispatch
    : public modern::TrajectoryInsetVisibleAtomicFinalDispatch {
public:
  modern::TrajectoryInsetVisibleAtomicFinalStage failStage =
      modern::TrajectoryInsetVisibleAtomicFinalStage::None;
  modern::TrajectoryInsetVisibleAtomicFinalStage errorStage =
      modern::TrajectoryInsetVisibleAtomicFinalStage::None;
  modern::TrajectoryInsetVisibleAtomicFinalStage throwStage =
      modern::TrajectoryInsetVisibleAtomicFinalStage::None;
  std::uint64_t nextHandle = 9000, retainedFinalStaging = 0;
  std::set<std::uint64_t> retainedHandleSet;
  std::set<std::uint32_t> insetMemberTokens;
  bool dirtyBeforeFailure = false;
  int resourceMutation = -1;
  int mainLdrWrites = 0, mainWorldTrajectoryTokenWrites = 0,
      panelContainerUiTokenWrites = 0, chromeWrites = 0;
  int presents = 0, invalidations = 0, restores = 0;
  std::vector<std::string> ledger;

  bool invoke(modern::TrajectoryInsetVisibleAtomicFinalStage stage,
              const ui::TrajectoryInsetVisibleCurrentFrame &current,
              modern::TrajectoryInsetVisibleAtomicFinalResources &resources,
              std::string &error) override {
    ledger.push_back(modern::trajectoryInsetVisibleAtomicFinalStageName(stage));
    if (stage == throwStage)
      throw std::runtime_error("injected_cp4_c_stage_throw");
    if (stage == failStage) {
      if (dirtyBeforeFailure)
        ++mainLdrWrites, ++panelContainerUiTokenWrites;
      error = "injected_cp4_c_stage_failure";
      return false;
    }
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::FreezeIdentity) {
      resources.sessionSerial = current.request.sessionSerial;
      resources.frameSerial = current.request.frameSerial;
      resources.layoutSerial = current.request.layoutSerial;
      resources.packageGeneration = current.request.cp1GenerationId;
      resources.preparationGeneration = current.request.frameSerial;
      resources.cp3ColorStaging = {modern::TrajectoryInsetFinalResourceKind::Texture,
                                  nextHandle++};
      resources.mainLdr = {modern::TrajectoryInsetFinalResourceKind::Texture, 101};
      resources.mainSemantic =
          {modern::TrajectoryInsetFinalResourceKind::Texture, 102};
      resources.width = static_cast<int>(std::lround(
          current.layout.plotCutoutRect.width));
      resources.height = static_cast<int>(std::lround(
          current.layout.plotCutoutRect.height));
      resources.samples = current.request.samples;
      resources.cutout = current.layout.plotCutoutRect;
      resources.panelContainerUiToken = 0xf0000001u;
      resources.panelContainerExcludedFromTrajectoryDenominator = true;
      for (const auto &batch : current.package.semanticBatches)
        insetMemberTokens.insert(batch.insetOwnershipToken);
    }
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::AllocateFinal) {
      resources.finalColor = {modern::TrajectoryInsetFinalResourceKind::Texture,
                              nextHandle++};
      resources.finalSemantic =
          {modern::TrajectoryInsetFinalResourceKind::Texture, nextHandle++};
      resources.finalFramebuffer =
          {modern::TrajectoryInsetFinalResourceKind::Framebuffer, nextHandle++};
      resources.finalSemanticFormat = "R32UI";
      resources.generation++;
    }
    if (stage ==
        modern::TrajectoryInsetVisibleAtomicFinalStage::ValidateIsolation) {
      resources.isolationValidated = true,
      resources.feedbackValidated = true;
      if (resourceMutation == 0)
        resources.finalColor = resources.mainLdr;
      if (resourceMutation == 1)
        resources.finalColor = resources.cp3ColorStaging;
      if (resourceMutation == 2)
        resources.finalSemantic = resources.mainSemantic;
      if (resourceMutation == 3)
        resources.finalFramebuffer = resources.finalColor;
      if (resourceMutation == 4)
        resources.feedbackValidated = false;
      if (resourceMutation == 5)
        ++resources.sessionSerial;
      if (resourceMutation == 6)
        ++resources.frameSerial;
      if (resourceMutation == 7)
        ++resources.layoutSerial;
      if (resourceMutation == 8)
        resources.packageGeneration = "stale";
      if (resourceMutation == 9)
        ++resources.preparationGeneration;
      if (resourceMutation == 10)
        ++resources.width;
      if (resourceMutation == 11)
        ++resources.height;
      if (resourceMutation == 12)
        resources.samples = 3;
      if (resourceMutation == 13)
        resources.cutout.x += 1.0f;
      if (resourceMutation == 14)
        resources.finalSemanticFormat = "RGBA8";
      if (resourceMutation == 20)
        resources.finalColor.handle = 0;
      if (resourceMutation == 21)
        resources.finalSemantic = resources.finalColor;
      if (resourceMutation == 22)
        resources.panelContainerUiToken =
            current.package.semanticBatches.front().insetOwnershipToken;
      if (resourceMutation == 23)
        resources.finalColor = resources.mainSemantic;
      if (resourceMutation == 24)
        resources.finalSemantic = resources.cp3ColorStaging;
      if (resourceMutation == 25)
        resources.finalSemantic = resources.mainLdr;
      if (resourceMutation == 26)
        resources.cp3ColorStaging = resources.mainLdr;
      if (resourceMutation == 27)
        resources.cp3ColorStaging = resources.mainSemantic;
      if (resourceMutation == 28)
        resources.mainLdr = resources.mainSemantic;
    }
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::StagePlot) {
      resources.plotPixels = 1;
      resources.plotPhysicalCoverage = current.layout.plotCutoutRect;
      resources.fixedPaletteSrgb = true;
      resources.toneApplicationCount = 1;
      if (resourceMutation == 15)
        resources.toneApplicationCount = 0;
      if (resourceMutation == 16)
        resources.toneApplicationCount = 2;
      if (resourceMutation == 17)
        resources.fixedPaletteSrgb = false;
    }
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::StageSemantic)
      resources.semanticTokens = current.package.probeDenominator;
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::StageChrome)
      resources.chromePrimitives = current.chrome.primitives.size(),
      resources.chromeAudited =
          ui::auditTrajectoryInsetChromeDrawList(current.layout, current.chrome)
              .ok;
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::StageChrome &&
        resourceMutation == 18)
      resources.chromeAudited = false;
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::StageChrome &&
        resourceMutation == 19)
      resources.chromeRefillsCutout = true;
    if (stage == modern::TrajectoryInsetVisibleAtomicFinalStage::RestoreState)
      ++restores;
    return true;
  }
  unsigned readError(
      modern::TrajectoryInsetVisibleAtomicFinalStage stage) override {
    return stage == errorStage ? 1282u : 0u;
  }
  bool preflightPresent(
      const modern::TrajectoryInsetVisibleAtomicFinalResources &resources,
      std::string &error) override {
    if (throwStage ==
        modern::TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent)
      throw std::runtime_error("injected_cp4_c_present_preflight_throw");
    if (failStage ==
            modern::TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent ||
        errorStage ==
            modern::TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent) {
      error = "injected_cp4_c_present_failure";
      return false;
    }
    if (!resources.complete() || resources.finalColor == resources.finalSemantic ||
        resources.finalColor == resources.cp3ColorStaging ||
        resources.finalColor == resources.mainLdr ||
        resources.finalSemantic == resources.mainSemantic ||
        resources.finalSemanticFormat != "R32UI" ||
        !resources.isolationValidated || !resources.feedbackValidated ||
        !resources.fixedPaletteSrgb || resources.toneApplicationCount != 1 ||
        !resources.chromeAudited || resources.chromeRefillsCutout ||
        !resources.panelContainerExcludedFromTrajectoryDenominator ||
        resources.panelContainerUiToken == 0 ||
        insetMemberTokens.count(resources.panelContainerUiToken) != 0) {
      error = "cp4_c_final_staging_incomplete_or_aliased";
      return false;
    }
    return true;
  }
  void publishAtomicNoexcept(
      const modern::TrajectoryInsetVisibleAtomicFinalResources &resources)
      noexcept override {
    ledger.push_back("atomic_present");
    ++presents;
    ++mainLdrWrites;
    ++panelContainerUiTokenWrites;
    ++chromeWrites;
    retainedFinalStaging = resources.finalFramebuffer.handle;
    retainedHandleSet = {resources.cp3ColorStaging.handle,
                         resources.finalColor.handle,
                         resources.finalSemantic.handle,
                         resources.finalFramebuffer.handle};
  }
  void invalidate(
      modern::TrajectoryInsetVisibleAtomicFinalResources &resources) noexcept
      override {
    ++invalidations;
    resources = {};
    retainedFinalStaging = 0;
    retainedHandleSet.clear();
  }
};

struct LegacyPostWriteErrorDispatch final : CheckpointCDispatch {
  bool postWriteErrorWouldBeVisible = false;
  void publishAtomicNoexcept(
      const modern::TrajectoryInsetVisibleAtomicFinalResources &resources)
      noexcept override {
    CheckpointCDispatch::publishAtomicNoexcept(resources);
    postWriteErrorWouldBeVisible = true;
  }
};

void checkpointCFinalAtomicStaging() {
  const billiardgl::PresentationSnapshot presentation;
  std::uint64_t serial = 3000;
  {
    ui::TrajectoryInsetVisibleCurrentFrame absent;
    CheckpointCDispatch dispatch;
    require(!modern::executeTrajectoryInsetVisibleAtomicFinal(absent, dispatch)
                 .ok &&
                dispatch.presents == 0 && dispatch.mainLdrWrites == 0 &&
                dispatch.mainWorldTrajectoryTokenWrites == 0 &&
                dispatch.panelContainerUiTokenWrites == 0 &&
                dispatch.chromeWrites == 0,
            "C default absent has zero calls and writes");
  }
  {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    LegacyPostWriteErrorDispatch legacy;
    require(modern::executeTrajectoryInsetVisibleAtomicFinal(current, legacy).ok &&
                legacy.postWriteErrorWouldBeVisible &&
                legacy.mainLdrWrites == 1,
            "C publish is terminal: a hypothetical post-write error is too late");
  }
  for (int samples : {1, 2, 4}) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    current.request.samples = samples;
    CheckpointCDispatch dispatch;
    const auto result = modern::executeTrajectoryInsetVisibleAtomicFinal(
        current, dispatch);
    require(result.ok && result.presented && dispatch.presents == 1 &&
                dispatch.restores == 1 && dispatch.mainLdrWrites == 1 &&
                dispatch.mainWorldTrajectoryTokenWrites == 0 &&
                dispatch.panelContainerUiTokenWrites == 1 &&
                dispatch.chromeWrites == 1 &&
                dispatch.ledger == std::vector<std::string>{
                                       "freeze_identity",
                                       "allocate_final_color_semantic_framebuffer",
                                       "validate_isolation", "stage_plot",
                                       "stage_semantic", "stage_chrome",
                                       "restore_state", "atomic_present"},
            "C success stages plot semantic chrome then restores and presents once");
    require(!modern::executeTrajectoryInsetVisibleAtomicFinal(current, dispatch)
                 .ok &&
                dispatch.presents == 1,
            "C repeat same current frame fails before present");
  }
  const auto stages = modern::orderedTrajectoryInsetVisibleAtomicFinalStages();
  for (const auto stage : stages) {
    for (int failureMode = 0; failureMode < 3; ++failureMode) {
      CheckpointCDispatch dispatch;
      auto before = ui::makeTrajectoryInsetVisibleCurrentFrame(
          checkpointBSession(game::ProductScreen::ReplayPlayback), presentation,
          {true, false, false}, checkpointBContext(serial++));
      require(modern::executeTrajectoryInsetVisibleAtomicFinal(before, dispatch)
                  .ok,
              "C N succeeds before injected failure");
      const auto priorHandles = dispatch.retainedHandleSet;
      const int priorLdr = dispatch.mainLdrWrites;
      const int priorWorldToken = dispatch.mainWorldTrajectoryTokenWrites;
      const int priorPanelToken = dispatch.panelContainerUiTokenWrites;
      const int priorChrome = dispatch.chromeWrites;
      auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
          checkpointBSession(game::ProductScreen::ReplayPlayback), presentation,
          {true, false, false}, checkpointBContext(serial++));
      if (failureMode == 0)
        dispatch.failStage = stage;
      if (failureMode == 1)
        dispatch.errorStage = stage;
      if (failureMode == 2)
        dispatch.throwStage = stage;
      const auto failed = modern::executeTrajectoryInsetVisibleAtomicFinal(
          current, dispatch);
      require(!failed.ok && !failed.presented &&
                  dispatch.mainLdrWrites == priorLdr &&
                  dispatch.mainWorldTrajectoryTokenWrites == priorWorldToken &&
                  dispatch.panelContainerUiTokenWrites == priorPanelToken &&
                  dispatch.chromeWrites == priorChrome &&
                  dispatch.retainedFinalStaging == 0 &&
                  dispatch.invalidations == 1,
              "C failure has zero partial plot token chrome or retained staging");
      dispatch.failStage = dispatch.errorStage = dispatch.throwStage =
          modern::TrajectoryInsetVisibleAtomicFinalStage::None;
      auto fresh = ui::makeTrajectoryInsetVisibleCurrentFrame(
          checkpointBSession(game::ProductScreen::ReplayPlayback), presentation,
          {true, false, false}, checkpointBContext(serial++));
      require(modern::executeTrajectoryInsetVisibleAtomicFinal(fresh, dispatch).ok &&
                  dispatch.retainedFinalStaging != 0 &&
                  std::none_of(dispatch.retainedHandleSet.begin(),
                               dispatch.retainedHandleSet.end(),
                               [&](auto handle) {
                                 return priorHandles.count(handle) != 0;
                               }),
              "C valid-fail-valid requires fresh serial and final handles");
    }
  }
  {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointCDispatch dirty;
    dirty.failStage = modern::TrajectoryInsetVisibleAtomicFinalStage::StagePlot;
    dirty.dirtyBeforeFailure = true;
    (void)modern::executeTrajectoryInsetVisibleAtomicFinal(current, dirty);
    require(dirty.mainLdrWrites == 1 &&
                dirty.panelContainerUiTokenWrites == 1 &&
                dirty.mainWorldTrajectoryTokenWrites == 0 &&
                dirty.invalidations == 1,
            "C invalidation cannot erase irreversible LDR or panel-token writes");
  }
  for (int mutation = 0; mutation < 29; ++mutation) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointCDispatch dispatch;
    dispatch.resourceMutation = mutation;
    const auto failed = modern::executeTrajectoryInsetVisibleAtomicFinal(
        current, dispatch);
    require(!failed.ok && dispatch.mainLdrWrites == 0 &&
                dispatch.mainWorldTrajectoryTokenWrites == 0 &&
                dispatch.panelContainerUiTokenWrites == 0 &&
                dispatch.chromeWrites == 0,
            "C resource identity format tone cutout mutation fails precommit:" +
                std::to_string(mutation) + ":" + failed.errorCode);
  }
  {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointCDispatch permissive;
    permissive.resourceMutation = 22;
    const auto failed = modern::executeTrajectoryInsetVisibleAtomicFinal(
        current, permissive);
    require(!failed.ok && permissive.presents == 0,
            "core executor rejects panel token alias before permissive publish");
  }
}

void checkpointDRealGlFinalStagingAdapterContract() {
  std::ifstream header("src/Billiards/render/modern/modern_renderer.h");
  std::ifstream source("src/Billiards/render/modern/modern_renderer.cpp");
  const std::string h((std::istreambuf_iterator<char>(header)),
                      std::istreambuf_iterator<char>());
  const std::string cpp((std::istreambuf_iterator<char>(source)),
                        std::istreambuf_iterator<char>());
  const std::vector<std::string> requiredHeader{
      "TrajectoryInsetVisibleFinalGlAdapter",
      "prepareTrajectoryInsetVisibleFinalStaging",
      "publishedProductOutputFramebuffer",
      "invalidateTrajectoryInsetVisibleFinalStaging"};
  for (const auto &token : requiredHeader)
    require(h.find(token) != std::string::npos,
            "D missing real final staging API:" + token);
  const std::vector<std::string> requiredSource{
      "trajectory_inset_final_color", "trajectory_inset_final_semantic_r32ui",
      "trajectory_inset_final_framebuffer", "GL_R32UI",
      "GL_FRAMEBUFFER_ATTACHMENT_OBJECT_NAME", "glCheckFramebufferStatus",
      "copyCurrentProductLdrToFinal", "compositeCp3InsetColorIntoFinal",
      "writeIndependentInsetSemantic", "TrajectoryInsetVisibleFinalContentOp",
      "productChromeAlreadyInCurrentLdr",
      "trajectory_inset_final_restore", "trajectory_inset_final_error_query",
      "trajectory_inset_terminal_publish_no_gl",
      "publishedProductOutputFramebuffer()"};
  for (const auto &token : requiredSource)
    require(cpp.find(token) != std::string::npos,
            "D missing real GL/static transition:" + token);
  const auto terminal = cpp.find("terminalPublishTrajectoryInsetOutputNoexcept");
  const auto terminalEnd = cpp.find("\n}", terminal);
  require(terminal != std::string::npos && terminalEnd != std::string::npos,
          "D terminal publish function exists");
  const std::string terminalBody = cpp.substr(terminal, terminalEnd - terminal);
  for (const auto &forbidden : {"glBind", "glBlit", "glDraw", "glUseProgram",
                                "glGetError", "throw"})
    require(terminalBody.find(forbidden) == std::string::npos,
            std::string("D terminal publish contains forbidden GL/throw:") +
                forbidden);
  const auto renderBegin = cpp.find("void ModernRenderer::renderFrame");
  const auto captureBegin = cpp.find("bool ModernRenderer::capture(", renderBegin);
  const std::string renderFrame = cpp.substr(renderBegin, captureBegin - renderBegin);
  const auto count = [&](const std::string &needle) {
    std::size_t total = 0, at = 0;
    while ((at = renderFrame.find(needle, at)) != std::string::npos)
      ++total, at += needle.size();
    return total;
  };
  require(count("prepareTrajectoryInsetVisibleFinalStaging(") == 1 &&
              count("publishedProductOutputFramebuffer()") == 1,
          "D renderFrame has one explicit final staging and output selection");
  const auto productUiStage = renderFrame.find("product_ui");
  const auto finalStage =
      renderFrame.find("prepareTrajectoryInsetVisibleFinalStaging(");
  const auto productPresent = renderFrame.find("product_present");
  require(productUiStage != std::string::npos &&
              finalStage != std::string::npos &&
              productPresent != std::string::npos &&
              productUiStage < finalStage && finalStage < productPresent,
          "D final staging is built after current product LDR/chrome and before "
          "the handle-only product present");
  require(renderFrame.find("trajectoryInsetVisibleCurrentContext") !=
              std::string::npos &&
              renderFrame.find("worldOverlayToggleOn = true") ==
                  std::string::npos,
          "D callsite requires explicit current request and world remains off");

  require(cpp.find("floor(gl_FragCoord.xy))-uDestinationOrigin") !=
              std::string::npos &&
              cpp.find("uSourceOrigin+ivec2(gl_FragCoord.xy)") ==
                  std::string::npos,
          "D semantic conversion uses half-pixel-stable destination-local "
          "coordinates, never the old absolute-fragment formula");
  const auto semanticMap = [](int sourceX, int sourceY, int destinationX,
                              int destinationY, double fragmentX,
                              double fragmentY) {
    return std::array<int, 2>{
        sourceX + static_cast<int>(std::floor(fragmentX)) - destinationX,
        sourceY + static_cast<int>(std::floor(fragmentY)) - destinationY};
  };
  const int sourceX = 13, sourceY = 17, destinationX = 347,
            destinationY = 619, width = 37, height = 23;
  require(semanticMap(sourceX, sourceY, destinationX, destinationY,
                      destinationX + 0.5, destinationY + 0.5) ==
                  std::array<int, 2>{{sourceX, sourceY}} &&
              semanticMap(sourceX, sourceY, destinationX, destinationY,
                          destinationX + width - 0.5,
                          destinationY + 0.5) ==
                  std::array<int, 2>{{sourceX + width - 1, sourceY}} &&
              semanticMap(sourceX, sourceY, destinationX, destinationY,
                          destinationX + 0.5,
                          destinationY + height - 0.5) ==
                  std::array<int, 2>{{sourceX, sourceY + height - 1}} &&
              semanticMap(sourceX, sourceY, destinationX, destinationY,
                          destinationX + width - 0.5,
                          destinationY + height - 0.5) ==
                  std::array<int, 2>{{sourceX + width - 1,
                                      sourceY + height - 1}} &&
              semanticMap(sourceX, sourceY, destinationX, destinationY,
                          destinationX + 18.5,
                          destinationY + 11.5) ==
                  std::array<int, 2>{{sourceX + 18, sourceY + 11}},
          "D nonzero asymmetric semantic mapping preserves four corners and "
          "center exactly");
}

class CheckpointDGlDispatch final
    : public modern::TrajectoryInsetVisibleFinalGlDispatch {
public:
  modern::TrajectoryInsetVisibleFinalGlStage failStage =
      modern::TrajectoryInsetVisibleFinalGlStage::None;
  modern::TrajectoryInsetVisibleFinalGlStage errorStage =
      modern::TrajectoryInsetVisibleFinalGlStage::None;
  modern::TrajectoryInsetVisibleFinalGlStage throwStage =
      modern::TrajectoryInsetVisibleFinalGlStage::None;
  modern::TrajectoryInsetVisibleFinalGlStage suppressObservableWriteStage =
      modern::TrajectoryInsetVisibleFinalGlStage::None;
  int contentOpMutation = -1;
  int contentOpFailIndex = -1, contentOpErrorIndex = -1,
      contentOpThrowIndex = -1, contentOpCallIndex = 0;
  std::uint64_t next = 12000, savedState = 0xabc, state = savedState;
  int mainLdrWrites = 0, mainWorldTokenWrites = 0, panelTokenWrites = 0;
  int captures = 0, allocations = 0, restores = 0, destroys = 0,
      terminalPublishes = 0;
  int fullSemanticClears = 0, currentLdrCopies = 0,
      cp3InsetColorDraws = 0, independentSemanticWrites = 0;
  std::vector<modern::TrajectoryInsetVisibleFinalContentOp> contentOps;
  std::vector<modern::TrajectoryInsetVisibleFinalContentOpReceipt>
      contentOpReceipts;
  bool poisoned = false;
  std::set<std::uint64_t> retainedHandles, destroyedHandles,
      lastDestroyedHandles;
  std::vector<std::string> ledger;

  bool contextUsable() const noexcept override { return !poisoned; }
  void poisonContext() noexcept override { poisoned = true; }

  std::uint64_t captureState() override {
    ++captures;
    ledger.push_back("capture_state");
    return savedState;
  }
  bool invoke(modern::TrajectoryInsetVisibleFinalGlStage stage,
              const modern::TrajectoryInsetVisibleFinalGlInput &input,
              modern::TrajectoryInsetVisibleFinalGlResources &resources,
              std::string &error) override {
    ledger.push_back(modern::trajectoryInsetVisibleFinalGlStageName(stage));
    if (stage != modern::TrajectoryInsetVisibleFinalGlStage::FinalErrorQuery)
      state ^= static_cast<std::uint64_t>(stage) + 1;
    if (stage == throwStage)
      throw std::runtime_error("injected_cp4_d_gl_throw");
    if (stage == failStage) {
      error = "injected_cp4_d_gl_failure";
      return false;
    }
    if (stage == suppressObservableWriteStage)
      return true;
    if (stage == modern::TrajectoryInsetVisibleFinalGlStage::Allocate) {
      ++allocations;
      resources.finalColor = {modern::TrajectoryInsetFinalResourceKind::Texture,
                              next++};
      resources.finalInsetSemantic =
          {modern::TrajectoryInsetFinalResourceKind::Texture, next++};
      resources.finalFramebuffer =
          {modern::TrajectoryInsetFinalResourceKind::Framebuffer, next++};
      resources.finalInsetSemanticFormat = "R32UI";
      resources.width = input.outputWidth;
      resources.height = input.outputHeight;
      resources.cp3InputSamples = input.cp3InputSamples;
      resources.finalOutputSamples = input.finalOutputSamples;
      resources.finalColorMetadataSamples = 1;
      resources.finalSemanticMetadataSamples = 1;
    }
    if (stage == modern::TrajectoryInsetVisibleFinalGlStage::Attach)
      resources.attachmentsExact = true;
    if (stage == modern::TrajectoryInsetVisibleFinalGlStage::ValidateIsolation)
      resources.isolationExact = true, resources.feedbackFree = true;
    if (stage == modern::TrajectoryInsetVisibleFinalGlStage::CompositeChrome)
      resources.chromeComposited = true,
      resources.cutoutNotRefilled = true,
      resources.productChromeAlreadyInCurrentLdr = true,
      resources.additionalChromeDrawWrites = 0,
      resources.toneApplicationCount = 1;
    if (stage ==
        modern::TrajectoryInsetVisibleFinalGlStage::FinalFramebufferCheck)
      resources.finalFramebufferComplete = true;
    if (stage == modern::TrajectoryInsetVisibleFinalGlStage::FinalErrorQuery)
      resources.finalErrorQueryComplete = true;
    return true;
  }
  bool executeContentOp(
      const modern::TrajectoryInsetVisibleFinalContentOp &op,
      modern::TrajectoryInsetVisibleFinalContentOpReceipt &receipt,
      std::string &) override {
    const auto suppressedKind = [&] {
      if (suppressObservableWriteStage ==
          modern::TrajectoryInsetVisibleFinalGlStage::ValidateIsolation)
        return modern::TrajectoryInsetVisibleFinalContentOpKind::
            ClearFinalSemanticR32ui;
      if (suppressObservableWriteStage ==
          modern::TrajectoryInsetVisibleFinalGlStage::CopyProductLdr)
        return modern::TrajectoryInsetVisibleFinalContentOpKind::
            CopyCurrentProductLdr;
      if (suppressObservableWriteStage ==
          modern::TrajectoryInsetVisibleFinalGlStage::CompositeInset)
        return modern::TrajectoryInsetVisibleFinalContentOpKind::
            CompositeCp3InsetColor;
      return modern::TrajectoryInsetVisibleFinalContentOpKind::
          CopyCp3InsetSemantic;
    }();
    if (suppressObservableWriteStage !=
            modern::TrajectoryInsetVisibleFinalGlStage::None &&
        op.kind == suppressedKind)
      return true;
    const int call = contentOpCallIndex++;
    if (call == contentOpThrowIndex)
      throw std::runtime_error("injected_cp4_d_content_op_throw");
    if (call == contentOpFailIndex)
      return false;
    auto observed = op;
    if (contentOpMutation == 0) observed.sourceRect.x += 1.0f;
    if (contentOpMutation == 1) observed.destinationRect.x += 1.0f;
    if (contentOpMutation == 2) ++observed.destinationAttachment;
    if (contentOpMutation == 3) observed.destination = observed.source;
    if (contentOpMutation == 4) observed.kind =
        modern::TrajectoryInsetVisibleFinalContentOpKind::CopyCurrentProductLdr;
    contentOps.push_back(observed);
    if (contentOpMutation == 5) contentOps.push_back(observed);
    receipt.kind = observed.kind;
    receipt.actualSource = observed.source;
    receipt.actualDestination = observed.destination;
    receipt.actualDestinationAttachment = observed.destinationAttachment;
    receipt.actualSourceRect = observed.sourceRect;
    receipt.actualDestinationRect = observed.destinationRect;
    receipt.actualSourceFormat = observed.sourceFormat;
    receipt.actualSourceGeneration = observed.sourceGeneration;
    receipt.actualIntegerSourceChannel = observed.integerSourceChannel;
    receipt.integerShaderConversionToR32ui =
        observed.integerShaderConversionToR32ui;
    receipt.actualWriteCount = contentOpMutation == 5 ? 2 : 1;
    receipt.glError = call == contentOpErrorIndex ? 1282u : 0u;
    contentOpReceipts.push_back(receipt);
    if (observed.kind == modern::TrajectoryInsetVisibleFinalContentOpKind::
                       ClearFinalSemanticR32ui)
      ++fullSemanticClears;
    if (observed.kind == modern::TrajectoryInsetVisibleFinalContentOpKind::
                       CopyCurrentProductLdr)
      ++currentLdrCopies;
    if (observed.kind == modern::TrajectoryInsetVisibleFinalContentOpKind::
                       CompositeCp3InsetColor)
      ++cp3InsetColorDraws;
    if (observed.kind == modern::TrajectoryInsetVisibleFinalContentOpKind::
                       CopyCp3InsetSemantic)
      ++independentSemanticWrites;
    return true;
  }
  unsigned readError(modern::TrajectoryInsetVisibleFinalGlStage stage) override {
    return stage == errorStage ? 1282u : 0u;
  }
  bool restoreState(std::uint64_t saved, std::string &error) override {
    ledger.push_back("restore_state");
    ++restores;
    if (failStage == modern::TrajectoryInsetVisibleFinalGlStage::RestoreState) {
      poisoned = true;
      error = "injected_cp4_d_restore_failure";
      return false;
    }
    if (throwStage == modern::TrajectoryInsetVisibleFinalGlStage::RestoreState) {
      poisoned = true;
      throw std::runtime_error("injected_cp4_d_restore_throw");
    }
    state = saved;
    return true;
  }
  void destroy(modern::TrajectoryInsetVisibleFinalGlResources &resources)
      noexcept override {
    ++destroys;
    lastDestroyedHandles.clear();
    for (auto key : {resources.finalColor, resources.finalInsetSemantic,
                     resources.finalFramebuffer})
      if (key.handle) {
        destroyedHandles.insert(key.handle);
        lastDestroyedHandles.insert(key.handle);
      }
    resources = {};
    retainedHandles.clear();
  }
  void terminalPublishOutputHandleNoexcept(
      const modern::TrajectoryInsetVisibleFinalGlResources &resources)
      noexcept override {
    ++terminalPublishes;
    retainedHandles = {resources.finalColor.handle,
                       resources.finalInsetSemantic.handle,
                       resources.finalFramebuffer.handle};
  }
};

modern::TrajectoryInsetVisibleFinalGlInput checkpointDInput(
    const ui::TrajectoryInsetVisibleCurrentFrame &current, int samples) {
  modern::TrajectoryInsetVisibleFinalGlInput input;
  input.current = &current;
  input.sessionSerial = current.request.sessionSerial;
  input.frameSerial = current.request.frameSerial;
  input.layoutSerial = current.request.layoutSerial;
  input.packageGeneration = current.request.cp1GenerationId;
  input.preparationGeneration = current.request.frameSerial;
  input.width = static_cast<int>(std::lround(current.layout.plotCutoutRect.width));
  input.height = static_cast<int>(std::lround(current.layout.plotCutoutRect.height));
  input.outputWidth = current.shellRequest.physicalWidth;
  input.outputHeight = current.shellRequest.physicalHeight;
  input.cp3InputSamples = samples;
  input.finalOutputSamples = 1;
  input.cutout = current.layout.plotCutoutRect;
  input.cp3ColorStaging =
      {modern::TrajectoryInsetFinalResourceKind::Texture, 201};
  input.cp3InsetSemantic =
      {modern::TrajectoryInsetFinalResourceKind::Texture, 204};
  input.cp3InsetSemanticFormat = "GL_RGBA32UI";
  input.cp3InsetSemanticGeneration = current.request.frameSerial;
  input.cp3InsetSemanticChannel = 0;
  input.currentProductLdr =
      {modern::TrajectoryInsetFinalResourceKind::Texture, 202};
  input.mainWorldSemantic =
      {modern::TrajectoryInsetFinalResourceKind::Texture, 203};
  return input;
}

void checkpointDGlMockTransitions() {
  const billiardgl::PresentationSnapshot presentation;
  std::uint64_t serial = 5000;
  {
    modern::TrajectoryInsetVisibleFinalGlInput absent;
    CheckpointDGlDispatch dispatch;
    const auto result = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
        absent, dispatch);
    require(!result.ok && dispatch.terminalPublishes == 0 &&
                dispatch.captures == 0 && dispatch.allocations == 0 &&
                dispatch.restores == 0 && dispatch.destroys == 0 &&
                dispatch.ledger.empty() &&
                dispatch.mainLdrWrites == 0 &&
                dispatch.mainWorldTokenWrites == 0 &&
                dispatch.panelTokenWrites == 0,
            "D default absent has zero GL/publish/main writes");
  }
  for (int samples : {1, 2, 4}) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointDGlDispatch dispatch;
    const auto result = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
        checkpointDInput(current, samples), dispatch);
    require(result.ok && result.stateRestored &&
                dispatch.terminalPublishes == 1 && dispatch.restores == 1 &&
                dispatch.fullSemanticClears == 1 &&
                dispatch.currentLdrCopies == 1 &&
                dispatch.cp3InsetColorDraws == 1 &&
                dispatch.independentSemanticWrites == 1 &&
                dispatch.contentOps.size() == 4 &&
                result.resources.width ==
                    checkpointDInput(current, samples).outputWidth &&
                result.resources.height ==
                    checkpointDInput(current, samples).outputHeight &&
                result.resources.cp3InputSamples == samples &&
                result.resources.finalOutputSamples == 1 &&
                result.resources.finalColorMetadataSamples == 1 &&
                result.resources.finalSemanticMetadataSamples == 1 &&
                result.resources.additionalChromeDrawWrites == 0 &&
                result.resources.productChromeAlreadyInCurrentLdr &&
                result.resources.verifiedContentOpCount == 4 &&
                !result.resources.orderedContentOpDigest.empty() &&
                dispatch.state == dispatch.savedState &&
                dispatch.mainLdrWrites == 0 &&
                dispatch.mainWorldTokenWrites == 0 &&
                dispatch.panelTokenWrites == 0 &&
                dispatch.retainedHandles.size() == 3,
            "D 1/2/4 complete independent staging then handle publish only");
    const auto input = checkpointDInput(current, samples);
    const hud::HudRect full{
        0.0f, 0.0f, static_cast<float>(input.outputWidth),
        static_cast<float>(input.outputHeight)};
    require(dispatch.contentOps[0].kind ==
                    modern::TrajectoryInsetVisibleFinalContentOpKind::
                        ClearFinalSemanticR32ui &&
                dispatch.contentOps[0].source.handle == 0 &&
                dispatch.contentOps[0].destination ==
                    result.resources.finalFramebuffer &&
                dispatch.contentOps[0].destinationAttachment == 1 &&
                sameRect(dispatch.contentOps[0].destinationRect, full) &&
                dispatch.contentOps[0].clearIntegerValue == 0 &&
                dispatch.contentOps[1].source == input.currentProductLdr &&
                dispatch.contentOps[1].kind ==
                    modern::TrajectoryInsetVisibleFinalContentOpKind::
                        CopyCurrentProductLdr &&
                dispatch.contentOps[1].destination ==
                    result.resources.finalFramebuffer &&
                dispatch.contentOps[1].destinationAttachment == 0 &&
                sameRect(dispatch.contentOps[1].sourceRect, full) &&
                sameRect(dispatch.contentOps[1].destinationRect, full) &&
                dispatch.contentOps[2].kind ==
                    modern::TrajectoryInsetVisibleFinalContentOpKind::
                        CompositeCp3InsetColor &&
                dispatch.contentOps[2].source == input.cp3ColorStaging &&
                dispatch.contentOps[2].destination ==
                    result.resources.finalFramebuffer &&
                dispatch.contentOps[2].destinationAttachment == 0 &&
                sameRect(dispatch.contentOps[2].sourceRect,
                         hud::HudRect{
                             0.0f, 0.0f, static_cast<float>(input.width),
                             static_cast<float>(input.height)}) &&
                sameRect(dispatch.contentOps[2].destinationRect, input.cutout) &&
                dispatch.contentOps[3].kind ==
                    modern::TrajectoryInsetVisibleFinalContentOpKind::
                        CopyCp3InsetSemantic &&
                dispatch.contentOps[3].source == input.cp3InsetSemantic &&
                dispatch.contentOps[3].sourceFormat == "GL_RGBA32UI" &&
                dispatch.contentOps[3].sourceGeneration ==
                    input.cp3InsetSemanticGeneration &&
                dispatch.contentOps[3].integerSourceChannel == 0 &&
                dispatch.contentOps[3].integerShaderConversionToR32ui &&
                dispatch.contentOps[3].destination ==
                    result.resources.finalFramebuffer &&
                dispatch.contentOps[3].destinationAttachment == 1 &&
                sameRect(dispatch.contentOps[3].sourceRect,
                         hud::HudRect{
                             0.0f, 0.0f, static_cast<float>(input.width),
                             static_cast<float>(input.height)}) &&
                sameRect(dispatch.contentOps[3].destinationRect, input.cutout),
            "D injectable content-op ledger binds full LDR and cutout sources, "
            "destinations, attachments, and exact rectangles");
  }
  for (const auto noOpStage : {
           modern::TrajectoryInsetVisibleFinalGlStage::ValidateIsolation,
           modern::TrajectoryInsetVisibleFinalGlStage::CopyProductLdr,
           modern::TrajectoryInsetVisibleFinalGlStage::CompositeInset,
           modern::TrajectoryInsetVisibleFinalGlStage::CompositeSemantic,
           modern::TrajectoryInsetVisibleFinalGlStage::CompositeSemantic}) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointDGlDispatch noOp;
    noOp.suppressObservableWriteStage = noOpStage;
    const auto failed = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
        checkpointDInput(current, 4), noOp);
    require(!failed.ok && noOp.terminalPublishes == 0 &&
                noOp.retainedHandles.empty(),
            "D a boolean-only/no-op content stage fails before publication");
  }
  for (int mutation = 0; mutation < 12; ++mutation) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    auto input = checkpointDInput(current, 4);
    if (mutation == 0) input.outputWidth = input.width;
    if (mutation == 1) input.outputHeight = input.height;
    if (mutation == 2) input.cutout.x += 1.0f;
    if (mutation == 3) input.cp3InsetSemantic = input.cp3ColorStaging;
    if (mutation == 4) input.currentProductLdr = input.cp3ColorStaging;
    if (mutation == 5) input.cutout.width += 1.0f;
    if (mutation == 6) input.finalOutputSamples = 2;
    if (mutation == 7) input.cp3InputSamples = 3;
    if (mutation == 8) input.outputWidth += 1;
    if (mutation == 9) input.cp3InsetSemanticFormat = "GL_R32UI";
    if (mutation == 10) ++input.cp3InsetSemanticGeneration;
    if (mutation == 11) input.cp3InsetSemanticChannel = 1;
    CheckpointDGlDispatch dispatch;
    const auto failed =
        modern::prepareTrajectoryInsetVisibleFinalGlStaging(input, dispatch);
    require(!failed.ok && dispatch.terminalPublishes == 0,
            "D wrong full dimensions/source aliases/cutout mapping fail closed:" +
                std::to_string(mutation));
  }
  for (int mutation = 0; mutation < 6; ++mutation) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointDGlDispatch dispatch;
    dispatch.contentOpMutation = mutation;
    const auto failed = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
        checkpointDInput(current, 4), dispatch);
    require(!failed.ok && dispatch.terminalPublishes == 0,
            "D wrong op source/destination/attachment/order/repeat fails");
  }
  for (int op = 0; op < 4; ++op)
    for (int mode = 0; mode < 3; ++mode) {
      auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
          checkpointBSession(game::ProductScreen::TrainingSession),
          presentation, {true, false, false}, checkpointBContext(serial++));
      CheckpointDGlDispatch dispatch;
      if (mode == 0) dispatch.contentOpFailIndex = op;
      if (mode == 1) dispatch.contentOpErrorIndex = op;
      if (mode == 2) dispatch.contentOpThrowIndex = op;
      const auto failed = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
          checkpointDInput(current, 4), dispatch);
      require(!failed.ok && dispatch.restores == 1 &&
                  dispatch.destroys == 1 &&
                  dispatch.terminalPublishes == 0 &&
                  dispatch.retainedHandles.empty(),
              "D each content op fail/error/throw restores, destroys, and "
              "forbids publication");
    }
  for (const auto stage : modern::orderedTrajectoryInsetVisibleFinalGlStages())
    for (int mode = 0; mode < 3; ++mode) {
      CheckpointDGlDispatch dispatch;
      auto before = ui::makeTrajectoryInsetVisibleCurrentFrame(
          checkpointBSession(game::ProductScreen::ReplayPlayback), presentation,
          {true, false, false}, checkpointBContext(serial++));
      auto beforeResult = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
          checkpointDInput(before, 4), dispatch);
      require(beforeResult.ok && dispatch.retainedHandles.size() == 3,
              "D N succeeds before injected transition failure");
      const auto nHandles = dispatch.retainedHandles;
      const auto nFrameSerial = before.request.frameSerial;
      modern::invalidateTrajectoryInsetVisibleFinalGlStaging(
          modern::TrajectoryInsetVisibleFinalGlInvalidation::FrameSuperseded,
          beforeResult, dispatch);
      require(dispatch.retainedHandles.empty() &&
                  std::includes(dispatch.destroyedHandles.begin(),
                                dispatch.destroyedHandles.end(),
                                nHandles.begin(), nHandles.end()),
              "D N handles destroyed before N+1 fresh generation");
      auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
          checkpointBSession(game::ProductScreen::ReplayPlayback), presentation,
          {true, false, false}, checkpointBContext(serial++));
      const int priorRestores = dispatch.restores;
      const int priorPublishes = dispatch.terminalPublishes;
      const auto saved = dispatch.savedState;
      if (mode == 0) dispatch.failStage = stage;
      if (mode == 1) dispatch.errorStage = stage;
      if (mode == 2) dispatch.throwStage = stage;
      const auto failed = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
          checkpointDInput(current, 4), dispatch);
      const bool restoreStageFailure =
          stage == modern::TrajectoryInsetVisibleFinalGlStage::RestoreState;
      const auto n1Handles = dispatch.lastDestroyedHandles;
      require(!failed.ok &&
                  dispatch.terminalPublishes == priorPublishes &&
                  dispatch.mainLdrWrites == 0 &&
                  dispatch.mainWorldTokenWrites == 0 &&
                  dispatch.panelTokenWrites == 0 &&
                  dispatch.retainedHandles.empty() &&
                  dispatch.restores == priorRestores + 1 &&
                  (restoreStageFailure || dispatch.state == saved),
              "D every GL/error/throw destroys staging with zero main writes");
      require(current.request.frameSerial != nFrameSerial &&
                  std::includes(dispatch.destroyedHandles.begin(),
                                dispatch.destroyedHandles.end(),
                                n1Handles.begin(), n1Handles.end()),
              "D N+1 has a fresh serial and destroys every allocated handle");
      if (restoreStageFailure)
        require((mode == 1 ? failed.stateRestored : !failed.stateRestored) &&
                    failed.hardRestoreFailure &&
                    failed.adapterContextPoisoned && dispatch.poisoned &&
                    dispatch.terminalPublishes == priorPublishes,
                "D restore fail/throw is a hard failure with output forbidden");
      dispatch.failStage = dispatch.errorStage = dispatch.throwStage =
          modern::TrajectoryInsetVisibleFinalGlStage::None;
      auto after = ui::makeTrajectoryInsetVisibleCurrentFrame(
          checkpointBSession(game::ProductScreen::ReplayPlayback), presentation,
          {true, false, false}, checkpointBContext(serial++));
      CheckpointDGlDispatch recoveredDispatch;
      CheckpointDGlDispatch *afterDispatch = &dispatch;
      if (restoreStageFailure) {
        const auto capturesBeforePoisonedRetry = dispatch.captures;
        const auto allocationsBeforePoisonedRetry = dispatch.allocations;
        const auto restoresBeforePoisonedRetry = dispatch.restores;
        const auto destroysBeforePoisonedRetry = dispatch.destroys;
        const auto publishesBeforePoisonedRetry = dispatch.terminalPublishes;
        const auto ledgerBeforePoisonedRetry = dispatch.ledger;
        const auto poisonedRetry =
            modern::prepareTrajectoryInsetVisibleFinalGlStaging(
                checkpointDInput(after, 4), dispatch);
        require(!poisonedRetry.ok && poisonedRetry.adapterContextPoisoned &&
                    dispatch.captures == capturesBeforePoisonedRetry &&
                    dispatch.allocations == allocationsBeforePoisonedRetry &&
                    dispatch.restores == restoresBeforePoisonedRetry &&
                    dispatch.destroys == destroysBeforePoisonedRetry &&
                    dispatch.terminalPublishes == publishesBeforePoisonedRetry &&
                    dispatch.ledger == ledgerBeforePoisonedRetry &&
                    dispatch.retainedHandles.empty(),
                "D poisoned context rejects retry with zero GL/output calls");
        modern::invalidateTrajectoryInsetVisibleFinalGlStaging(
            modern::TrajectoryInsetVisibleFinalGlInvalidation::ContextLoss,
            failed, dispatch);
        recoveredDispatch.next = dispatch.next + 1000;
        afterDispatch = &recoveredDispatch;
      }
      auto afterResult = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
          checkpointDInput(after, 4), *afterDispatch);
      require(afterResult.ok && after.request.frameSerial != nFrameSerial &&
                  after.request.frameSerial != current.request.frameSerial &&
                  afterDispatch->retainedHandles.size() == 3 &&
                  std::none_of(afterDispatch->retainedHandles.begin(),
                               afterDispatch->retainedHandles.end(), [&](auto handle) {
                                 return nHandles.count(handle) != 0 ||
                                        n1Handles.count(handle) != 0;
                               }),
              "D N+2 requires a fresh serial and, after poison, a reinitialized "
              "context with handles disjoint from N and N+1");
    }
  for (const auto invalidation : {
           modern::TrajectoryInsetVisibleFinalGlInvalidation::Resize,
           modern::TrajectoryInsetVisibleFinalGlInvalidation::ContextLoss}) {
    auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
        checkpointBSession(game::ProductScreen::TrainingSession), presentation,
        {true, false, false}, checkpointBContext(serial++));
    CheckpointDGlDispatch dispatch;
    auto result = modern::prepareTrajectoryInsetVisibleFinalGlStaging(
        checkpointDInput(current, 2), dispatch);
    const auto stale = dispatch.retainedHandles;
    modern::invalidateTrajectoryInsetVisibleFinalGlStaging(
        invalidation, result, dispatch);
    require(dispatch.retainedHandles.empty() &&
                std::includes(dispatch.destroyedHandles.begin(),
                              dispatch.destroyedHandles.end(), stale.begin(),
                              stale.end()),
            "D resize/context destroys every stale final handle");
  }
}

class CheckpointChromeGlDispatch final : public ui::ProductUiGlDispatch {
public:
  enum class Failure { None, Create, Begin, Upload, Scissor, Draw, End };
  Failure fail = Failure::None, throwAt = Failure::None;
  int creates = 0, begins = 0, uploads = 0, draws = 0, ends = 0;
  int stateCaptures = 0, stateRestores = 0;
  int targetBindQueries = 0;
  int adjacentErrorQueries = 0;
  std::uint32_t actualCurrentLdrFramebuffer = 701;
  std::uint32_t actualCurrentLdrColorTexture = 702;
  std::uint32_t adjacentGlError = 0;
  bool targetBindingValid = true;
  bool stateDirty = false, poisoned = false;
  bool productionStyleBeginFailure = false;
  bool productionStyleRestoreFailure = false;
  bool productionStyleRestoreThrow = false;
  std::vector<ui::UiGpuVertex> actualVertices;
  std::vector<ui::UiGpuBatch> actualBatches;
  std::vector<ui::UiGpuScissor> actualScissors;

  void resetFrameObservation() {
    fail = Failure::None;
    throwAt = Failure::None;
    begins = uploads = draws = ends = 0;
    stateCaptures = stateRestores = 0;
    targetBindQueries = adjacentErrorQueries = 0;
    adjacentGlError = 0;
    targetBindingValid = true;
    stateDirty = poisoned = false;
    productionStyleBeginFailure = false;
    productionStyleRestoreFailure = false;
    productionStyleRestoreThrow = false;
    actualVertices.clear();
    actualBatches.clear();
    actualScissors.clear();
  }

  ui::TrajectoryInsetChromeGpuTargetBinding
  bindAndQueryTrajectoryInsetChromeTarget() override {
    ++targetBindQueries;
    return {actualCurrentLdrFramebuffer, actualCurrentLdrColorTexture,
            targetBindingValid};
  }
  std::uint32_t adjacentTrajectoryInsetChromeGlError() override {
    ++adjacentErrorQueries;
    return adjacentGlError;
  }

  ui::UiGpuResult createResources(const ui::UiGpuResourceCreateInfo &,
                                  ui::UiGpuResourceHandles &handles) override {
    ++creates;
    if (throwAt == Failure::Create) throw std::runtime_error("chrome_create");
    if (fail == Failure::Create) return {false, "chrome_create_failed"};
    handles = {811, 812, 813, 814};
    return {true, {}};
  }
  void destroyResources(ui::UiGpuResourceHandles &handles) noexcept override {
    handles = {};
  }
  void abandonResources(ui::UiGpuResourceHandles &handles) noexcept override {
    handles = {};
  }
  ui::UiGpuResult beginFrame(const ui::UiGpuResourceHandles &, int,
                             int) override {
    ++begins;
    ++stateCaptures;
    stateDirty = true;
    if (throwAt == Failure::Begin) throw std::runtime_error("chrome_begin");
    if (productionStyleBeginFailure)
      return {false, "chrome_production_begin_failed"};
    return fail == Failure::Begin ? ui::UiGpuResult{false, "chrome_begin_failed"}
                                  : ui::UiGpuResult{true, {}};
  }
  ui::UiGpuBeginReceipt beginFrameReceipt(
      const ui::UiGpuResourceHandles &handles, int width,
      int height) override {
    const auto operation = beginFrame(handles, width, height);
    return operation.ok ? ui::UiGpuBeginReceipt::capturedSuccess() :
        ui::UiGpuBeginReceipt::capturedFailure(operation.errorCode);
  }
  ui::UiGpuResult uploadVertices(
      const ui::UiGpuResourceHandles &,
      const std::vector<ui::UiGpuVertex> &vertices) override {
    ++uploads;
    if (throwAt == Failure::Upload) throw std::runtime_error("chrome_upload");
    if (fail == Failure::Upload) return {false, "chrome_upload_failed"};
    actualVertices = vertices;
    return {true, {}};
  }
  ui::UiGpuResult setScissor(const ui::UiGpuScissor &scissor) override {
    if (throwAt == Failure::Scissor) throw std::runtime_error("chrome_scissor");
    if (fail == Failure::Scissor)
      return {false, "chrome_scissor_failed"};
    actualScissors.push_back(scissor);
    return {true, {}};
  }
  ui::UiGpuResult draw(const ui::UiGpuResourceHandles &,
                       const ui::UiGpuBatch &batch) override {
    ++draws;
    if (throwAt == Failure::Draw) throw std::runtime_error("chrome_draw");
    if (fail == Failure::Draw) return {false, "chrome_draw_failed"};
    auto actual = batch;
    if (!actualScissors.empty()) actual.scissor = actualScissors.back();
    actualBatches.push_back(actual);
    return {true, {}};
  }
  ui::UiGpuResult endFrame() override {
    ++ends;
    if (productionStyleRestoreThrow) {
      poisoned = true;
      throw std::runtime_error("chrome_production_restore_throw");
    }
    if (productionStyleRestoreFailure) {
      poisoned = true;
      return {false, "chrome_production_restore_failed"};
    }
    if (throwAt == Failure::End) {
      poisoned = true;
      throw std::runtime_error("chrome_restore");
    }
    if (fail == Failure::End) {
      poisoned = true;
      return {false, "chrome_restore_failed"};
    }
    stateDirty = false;
    ++stateRestores;
    return {true, {}};
  }
};

void actualCore41SingleRestoreOwnershipRed() {
  using core41_gl_link_shim::state;
  core41_gl_link_shim::reset();
  ui::Core41ProductUiGlDispatch core41;
  ui::UiGpuResourceHandles handles;
  const std::vector<std::uint8_t> atlas(16 * 16, 127);
  ui::UiGpuResourceCreateInfo create;
  create.vertexShader = "void main(){}";
  create.fragmentShader = "void main(){}";
  create.atlasWidth = 16;
  create.atlasHeight = 16;
  create.atlasPixels = &atlas;
  require(core41.createResources(create, handles).ok && handles.complete(),
          "D actual Core41 link-shim resources initialize");
  state.restoreAttempts = 0;

  const auto invalid = core41.beginFrameReceipt(handles, 0, 64);
  require(!invalid.setupOk() && !invalid.stateCaptured() &&
              !invalid.restoreRequired() && state.restoreAttempts == 0,
          "D actual Core41 pre-capture rejection restores zero times");

  state.injectionActive = true;
  state.beginSetupError = true;
  const auto capturedFailure = core41.beginFrameReceipt(handles, 64, 64);
  require(!capturedFailure.setupOk() && capturedFailure.stateCaptured() &&
              capturedFailure.restoreRequired() && state.restoreAttempts == 0,
          "D actual Core41 captured begin failure returns typed receipt and internally restores zero times");
  const auto restored = core41.endFrame();
  require(restored.ok && state.restoreAttempts == 1,
          "D actual Core41 outer owner restores captured failure exactly once");

  state.beginSetupError = true;
  const auto restoreFailureBegin = core41.beginFrameReceipt(handles, 64, 64);
  require(!restoreFailureBegin.setupOk() &&
              restoreFailureBegin.stateCaptured() &&
              restoreFailureBegin.restoreRequired(),
          "D actual Core41 exposes captured state before restore failure");
  state.restoreError = true;
  const auto restoreFailure = core41.endFrame();
  require(!restoreFailure.ok && state.restoreAttempts == 2,
          "D actual Core41 restore error remains observable to outer owner");
  state.restoreError = false;

  state.beginSetupError = true;
  const auto restoreThrowBegin = core41.beginFrameReceipt(handles, 64, 64);
  require(!restoreThrowBegin.setupOk() &&
              restoreThrowBegin.stateCaptured() &&
              restoreThrowBegin.restoreRequired(),
          "D actual Core41 exposes captured state before restore throw");
  state.restoreThrow = true;
  bool threw = false;
  try { (void)core41.endFrame(); } catch (...) { threw = true; }
  require(threw && state.restoreAttempts == 3,
          "D actual Core41 restore throw remains observable exactly once");
}

std::string chromePrimitiveDigest(
    const ui::TrajectoryInsetChromeDrawList &chrome) {
  std::string canonical;
  appendBits(canonical, chrome.primitives.size());
  for (const auto &primitive : chrome.primitives) {
    appendBits(canonical, primitive.kind);
    appendBits(canonical, primitive.rect.x);
    appendBits(canonical, primitive.rect.y);
    appendBits(canonical, primitive.rect.width);
    appendBits(canonical, primitive.rect.height);
    appendField(canonical, primitive.token);
    appendBits(canonical, primitive.opacity);
    appendBits(canonical, primitive.excludedFromTrajectoryDenominator);
  }
  for (const auto &text : chrome.orderedText) appendField(canonical, text);
  return billiardgl::sha256Hex(canonical);
}

template <typename Receipt>
std::string submittedBatchDigest(const Receipt &receipt) {
  std::string canonical;
  appendBits(canonical, receipt.submittedBatches.size());
  for (const auto &batch : receipt.submittedBatches) {
    appendBits(canonical, batch.kind);
    appendBits(canonical, batch.scissor.x);
    appendBits(canonical, batch.scissor.y);
    appendBits(canonical, batch.scissor.width);
    appendBits(canonical, batch.scissor.height);
    appendBits(canonical, batch.firstVertex);
    appendBits(canonical, batch.vertexCount);
  }
  return billiardgl::sha256Hex(canonical);
}

template <typename Receipt>
std::string submittedCutoutProofDigest(const Receipt &receipt) {
  std::string canonical;
  appendBits(canonical, receipt.coveredPhysicalSamples.size());
  for (const auto &sample : receipt.coveredPhysicalSamples) {
    appendBits(canonical, sample.x);
    appendBits(canonical, sample.y);
    appendBits(canonical, sample.sampleIndex);
    appendField(canonical, sample.semanticToken);
  }
  return billiardgl::sha256Hex(canonical);
}

struct IndependentChromeRaster {
  std::size_t coveredSamples = 0;
  bool cutoutUntouched = true;
  std::string digest;
};

std::array<double, 2> chromeSamplePosition(int samples, int index) {
  static constexpr std::array<std::array<double, 2>, 4> sample4{{
      {{0.125, 0.625}}, {{0.375, 0.125}},
      {{0.625, 0.875}}, {{0.875, 0.375}}}};
  static constexpr std::array<std::array<double, 2>, 2> sample2{{
      {{0.25, 0.75}}, {{0.75, 0.25}}}};
  if (samples == 1) return {{0.5, 0.5}};
  if (samples == 2) return sample2[static_cast<std::size_t>(index)];
  return sample4[static_cast<std::size_t>(index)];
}

bool physicalSampleInsideCutout(int x, int y, int sampleIndex, int samples,
                               const hud::HudRect &cutout) {
  const auto sub = chromeSamplePosition(samples, sampleIndex);
  const double physicalX = x + sub[0], physicalY = y + sub[1];
  return physicalX >= cutout.x && physicalX < cutout.x + cutout.width &&
         physicalY >= cutout.y && physicalY < cutout.y + cutout.height;
}

void fractionalCutoutSampleRegression() {
  const hud::HudRect cutout{1293.599976f, 604.799988f,
                            561.599976f, 280.799988f};
  const int edgePixelX = 1855, edgePixelY = 768;
  const bool legacyIntegerFormula =
      edgePixelX >= cutout.x && edgePixelX < cutout.x + cutout.width &&
      edgePixelY >= cutout.y && edgePixelY < cutout.y + cutout.height;
  require(legacyIntegerFormula &&
              physicalSampleInsideCutout(
                  edgePixelX - 1, edgePixelY, 0, 1, cutout) &&
              !physicalSampleInsideCutout(
                  edgePixelX, edgePixelY, 0, 1, cutout) &&
              physicalSampleInsideCutout(
                  edgePixelX - 1, edgePixelY, 1, 2, cutout) &&
              !physicalSampleInsideCutout(
                  edgePixelX, edgePixelY, 1, 2, cutout) &&
              physicalSampleInsideCutout(
                  edgePixelX, edgePixelY, 0, 4, cutout) &&
              !physicalSampleInsideCutout(
                  edgePixelX, edgePixelY, 1, 4, cutout),
          "D fractional right edge uses exact asymmetric 1x/2x/4x sample centers, not integer pixel index");
}

IndependentChromeRaster rasterUploadedChrome(
    const CheckpointChromeGlDispatch &dispatch, int width, int height,
    int samples, const hud::HudRect &cutout) {
  struct Point { double x = 0.0, y = 0.0; };
  const std::array<std::array<double, 2>, 4> sample4{{
      {{0.125, 0.625}}, {{0.375, 0.125}},
      {{0.625, 0.875}}, {{0.875, 0.375}}}};
  const std::array<std::array<double, 2>, 2> sample2{{
      {{0.25, 0.75}}, {{0.75, 0.25}}}};
  const auto position = [&](int index) {
    if (samples == 1) return std::array<double, 2>{{0.5, 0.5}};
    if (samples == 2) return sample2[static_cast<std::size_t>(index)];
    return sample4[static_cast<std::size_t>(index)];
  };
  const auto physical = [&](const ui::UiGpuVertex &vertex) {
    return Point{(static_cast<double>(vertex.ndcX) + 1.0) * 0.5 * width,
                 (1.0 - static_cast<double>(vertex.ndcY)) * 0.5 * height};
  };
  const auto edge = [](const Point &a, const Point &b, const Point &p) {
    return (b.x - a.x) * (p.y - a.y) -
           (b.y - a.y) * (p.x - a.x);
  };
  const auto topLeftYDown = [](const Point &a, const Point &b) {
    const double dx = b.x - a.x, dy = b.y - a.y;
    return dy < 0.0 || (dy == 0.0 && dx > 0.0);
  };
  const auto covers = [&](Point a, Point b, Point c, const Point &p) {
    if (edge(a, b, c) < 0.0) std::swap(b, c);
    const std::array<double, 3> values{{edge(a, b, p), edge(b, c, p),
                                        edge(c, a, p)}};
    const std::array<bool, 3> owned{{topLeftYDown(a, b), topLeftYDown(b, c),
                                     topLeftYDown(c, a)}};
    constexpr double epsilon = 1e-9;
    for (std::size_t index = 0; index < values.size(); ++index)
      if (values[index] < -epsilon ||
          (std::abs(values[index]) <= epsilon && !owned[index]))
        return false;
    return true;
  };
  std::set<std::string> canonicalSamples;
  for (const auto &batch : dispatch.actualBatches) {
    require(batch.firstVertex + batch.vertexCount <=
                dispatch.actualVertices.size() && batch.vertexCount % 3 == 0,
            "D independently rastered batch range is in bounds/triangular");
    for (std::size_t offset = 0; offset < batch.vertexCount; offset += 3) {
      const Point a = physical(dispatch.actualVertices[batch.firstVertex + offset]);
      const Point b = physical(dispatch.actualVertices[batch.firstVertex + offset + 1]);
      const Point c = physical(dispatch.actualVertices[batch.firstVertex + offset + 2]);
      const auto &clip = dispatch.actualVertices[batch.firstVertex + offset];
      const int minX = std::max(0, static_cast<int>(std::floor(
          std::min({a.x, b.x, c.x}))));
      const int maxX = std::min(width - 1, static_cast<int>(std::ceil(
          std::max({a.x, b.x, c.x}))));
      const int minY = std::max(0, static_cast<int>(std::floor(
          std::min({a.y, b.y, c.y}))));
      const int maxY = std::min(height - 1, static_cast<int>(std::ceil(
          std::max({a.y, b.y, c.y}))));
      for (int y = minY; y <= maxY; ++y)
        for (int x = minX; x <= maxX; ++x)
          for (int sample = 0; sample < samples; ++sample) {
            const auto sub = position(sample);
            const Point p{x + sub[0], y + sub[1]};
            const double bottomY = height - p.y;
            if (p.x < batch.scissor.x ||
                p.x >= batch.scissor.x + batch.scissor.width ||
                bottomY < batch.scissor.y ||
                bottomY >= batch.scissor.y + batch.scissor.height ||
                p.x < clip.clipLeft || p.x >= clip.clipRight ||
                bottomY < clip.clipBottom || bottomY >= clip.clipTop ||
                !covers(a, b, c, p))
              continue;
            std::string key;
            appendBits(key, x);
            appendBits(key, y);
            appendBits(key, sample);
            canonicalSamples.insert(key);
          }
    }
  }
  std::string canonical;
  for (const auto &sample : canonicalSamples) canonical += sample;
  IndependentChromeRaster result;
  result.coveredSamples = canonicalSamples.size();
  result.digest = billiardgl::sha256Hex(canonical);
  for (const auto &sample : canonicalSamples) {
    int x = 0, y = 0, sampleIndex = 0;
    std::memcpy(&x, sample.data(), sizeof(x));
    std::memcpy(&y, sample.data() + sizeof(x), sizeof(y));
    std::memcpy(&sampleIndex,
                sample.data() + sizeof(x) + sizeof(y), sizeof(sampleIndex));
    if (physicalSampleInsideCutout(x, y, sampleIndex, samples, cutout))
      result.cutoutUntouched = false;
  }
  return result;
}

template <typename Receipt>
bool exactSemanticStreamUnion(
    const Receipt &receipt, const CheckpointChromeGlDispatch &dispatch) {
  std::size_t vertexCursor = 0, batchCursor = 0;
  for (const auto &row : receipt.orderedSemanticRows) {
    if (row.vertexRanges.empty() || row.batchRanges.empty()) return false;
    for (const auto &range : row.vertexRanges) {
      if (range.first != vertexCursor || range.count == 0 ||
          range.first + range.count > dispatch.actualVertices.size())
        return false;
      vertexCursor += range.count;
    }
    for (const auto &range : row.batchRanges) {
      if (range.first != batchCursor || range.count == 0 ||
          range.first + range.count > dispatch.actualBatches.size())
        return false;
      batchCursor += range.count;
    }
  }
  return vertexCursor == dispatch.actualVertices.size() &&
         batchCursor == dispatch.actualBatches.size();
}

const std::vector<std::string> &frozenChromeBilingualStrings() {
  static const std::vector<std::string> values{
      "TRAJECTORY / 轨迹", "HEAD / 开球端", "FOOT / 底端",
      "ACTUAL / 实际 — paired continuous rails",
      "PREDICTED / 预测 — centerline dashes",
      "END / 终点 — endpoint marker",
      "Trajectory inset / 轨迹小窗"};
  return values;
}

std::vector<std::uint32_t> frozenChromeCodepoints() {
  std::vector<std::uint32_t> result;
  for (const auto &text : frozenChromeBilingualStrings()) {
    std::vector<std::uint32_t> decoded;
    std::string error;
    require(ui::decodeUtf8(text, decoded, error),
            "D frozen bilingual string decodes without fallback");
    result.insert(result.end(), decoded.begin(), decoded.end());
  }
  std::sort(result.begin(), result.end());
  result.erase(std::unique(result.begin(), result.end()), result.end());
  return result;
}

billiardgl::render::ModernProductUiRuntimeConfig chromeRuntimeConfig() {
  constexpr const char *fontPath =
      "/System/Library/Fonts/Supplemental/Arial Unicode.ttf";
  auto config =
      billiardgl::render::ModernProductUiRuntimeConfig::trustedDefaults(
          ".", fontPath, 0);
  config.fontSha256 =
      "876af2cd4854644e7f3e7feb2f688997fdb3343c6df6693611209c9dfb47ccec";
  config.supplementalUtf8Strings = frozenChromeBilingualStrings();
  return config;
}

// Tests-first RED for the frozen CP4-D Option-A correction.  This deliberately
// names the missing typed Product-UI supplemental submission/receipt API.  The
// production path must consume the exact current chrome, convert it through
// the existing solid/SDF renderer, bind the actual current-LDR target, and
// return an actual-work receipt before final staging may claim the chrome is
// present.
void checkpointDProductUiChromeSubmissionCorrectionRed() {
  const billiardgl::PresentationSnapshot presentation;
  std::uint64_t serial = 9000;
  std::size_t positiveCases = 0;
  const bool core41Only = std::getenv("CP4_CORE41_ONLY") != nullptr;
  CheckpointChromeGlDispatch positiveDispatch;
  billiardgl::render::ModernProductUiRuntime positiveRuntime(positiveDispatch);
  billiardgl::render::ModernRendererProductUiPass positivePass(positiveRuntime);
  const auto positiveConfig = chromeRuntimeConfig();
  require(positivePass.initialize(positiveConfig).ok,
          "D shared 96-case concrete runtime/atlas initializes once");
  if (!core41Only) {
  for (const auto screen : {game::ProductScreen::TrainingSession,
                            game::ProductScreen::ReplayPlayback})
      for (const auto &profile : kProfiles)
      for (const auto palette : {training::TrajectoryInsetPalette::Standard,
                                 training::TrajectoryInsetPalette::ColorVisionSafe})
        for (const int samples : {1, 2, 4})
          for (const bool focused : {false, true}) {
            auto session = checkpointBSession(screen);
            session.shellViewport.physicalWidth = profile.width;
            session.shellViewport.physicalHeight = profile.height;
            session.shell.uiScale = profile.uiScale;
            auto context = checkpointBContext(serial++);
            context.palette = palette;
            context.samples = samples;
            context.insetControlFocused = focused;
            auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
                session, presentation, {true, false, false}, context);
            require(current.ok && current.chrome.primitives.size() ==
                                      static_cast<std::size_t>(focused ? 17 : 16),
                    "D chrome RED fixture has exact current 16/17 inventory");

            billiardgl::render::TrajectoryInsetProductUiChromeTarget target;
            target.sessionSerial = current.request.sessionSerial;
            target.frameSerial = current.request.frameSerial;
            target.layoutSerial = current.request.layoutSerial;
            target.cp1GenerationId = current.request.cp1GenerationId;
            target.cp2LayoutDigest = current.request.cp2LayoutDigest;
            target.cp2ExclusionDigest = current.request.cp2ExclusionDigest;
            target.palette = palette;
            target.physicalWidth = profile.width;
            target.physicalHeight = profile.height;
            target.cp1Digests = current.request.cp1Digests;
            target.cp1GeometryDigest = current.request.cp1GeometryDigest;
            target.chromeSemanticDigest = chromePrimitiveDigest(current.chrome);
            target.samples = samples;
            target.insetControlFocused = focused;

            positiveDispatch.resetFrameObservation();
            require(positiveRuntime.resize(
                        profile.width, profile.height, 96.0f).ok,
                    "D shared concrete runtime resizes for current profile");
            require(positiveRuntime.trajectoryInsetChromeAtlasContainsExactCodepoints(
                        frozenChromeCodepoints()) &&
                        current.chrome.orderedText ==
                            frozenChromeBilingualStrings(),
                    "D actual SHA-bound atlas contains exact bilingual set");
            target.atlasCacheKey =
                positiveRuntime.trajectoryInsetChromeAtlasCacheKey();
            target.currentLdrFramebuffer =
                positiveDispatch.actualCurrentLdrFramebuffer;
            target.currentLdrColorTexture =
                positiveDispatch.actualCurrentLdrColorTexture;
            const billiardgl::render::TrajectoryInsetProductUiChromeReceipt receipt =
                positivePass.submitTrajectoryInsetChrome(current, target);
            const std::size_t focusRows = std::count_if(
                receipt.orderedSemanticRows.begin(),
                receipt.orderedSemanticRows.end(), [](const auto &row) {
                  return row.token == "trajectory_inset_focus" &&
                         row.kind == ui::TrajectoryInsetChromeKind::Focus;
                });
            bool exactSemanticRows =
                receipt.orderedSemanticRows.size() ==
                current.chrome.primitives.size();
            for (std::size_t row = 0;
                 exactSemanticRows && row < current.chrome.primitives.size();
                 ++row) {
              const auto &expected = current.chrome.primitives[row];
              const auto &actual = receipt.orderedSemanticRows[row];
              exactSemanticRows =
                  actual.token == expected.token &&
                  actual.kind == expected.kind &&
                  sameRect(actual.rect, expected.rect) &&
                  actual.firstVertex < actual.firstVertex + actual.vertexCount &&
                  actual.firstBatch < actual.firstBatch + actual.batchCount;
            }
            const bool physicalSdfFringeOutsideCutout = std::none_of(
                receipt.coveredPhysicalSamples.begin(),
                receipt.coveredPhysicalSamples.end(), [&](const auto &sample) {
                  const auto &cutout = current.layout.plotCutoutRect;
                  return physicalSampleInsideCutout(
                      sample.x, sample.y, sample.sampleIndex, samples, cutout);
                });
            const auto independentRaster = rasterUploadedChrome(
                positiveDispatch, profile.width, profile.height, samples,
                current.layout.plotCutoutRect);
            if (!receipt.ok || !exactSemanticRows ||
                receipt.cutoutZeroCoverageProofDigest != independentRaster.digest ||
                receipt.coveredPhysicalSampleCount !=
                    independentRaster.coveredSamples ||
                !exactSemanticStreamUnion(receipt, positiveDispatch) ||
                positiveDispatch.actualBatches != receipt.submittedBatches ||
                positiveDispatch.actualVertices.size() !=
                    receipt.submittedSolidVertexCount +
                        receipt.submittedSdfVertexCount) {
              std::cerr << "D_CHROME_DIAG error=" << receipt.errorCode
                        << " ok=" << receipt.ok
                        << " rows=" << exactSemanticRows
                        << " digest="
                        << (receipt.cutoutZeroCoverageProofDigest ==
                            independentRaster.digest)
                        << " samples=" << receipt.coveredPhysicalSampleCount
                        << '/' << independentRaster.coveredSamples
                        << " union="
                        << exactSemanticStreamUnion(receipt, positiveDispatch)
                        << " batches=" << positiveDispatch.actualBatches.size() << '/'
                        << receipt.submittedBatches.size()
                        << " vertices=" << positiveDispatch.actualVertices.size() << '/'
                        << receipt.submittedSolidVertexCount +
                               receipt.submittedSdfVertexCount
                        << '\n';
            }
            require(receipt.ok && receipt.stateRestored && receipt.glError == 0 &&
                        receipt.sessionSerial == target.sessionSerial &&
                        receipt.frameSerial == target.frameSerial &&
                        receipt.layoutSerial == target.layoutSerial &&
                        receipt.targetCurrentLdrFramebuffer ==
                            target.currentLdrFramebuffer &&
                        receipt.targetCurrentLdrColorTexture ==
                            target.currentLdrColorTexture &&
                        receipt.cp1Digests == target.cp1Digests &&
                        receipt.cp1GeometryDigest == target.cp1GeometryDigest &&
                        receipt.cp2LayoutDigest == target.cp2LayoutDigest &&
                        receipt.cp2ExclusionDigest == target.cp2ExclusionDigest &&
                        receipt.chromeSemanticDigest ==
                            target.chromeSemanticDigest &&
                        receipt.atlasCacheKey == target.atlasCacheKey &&
                        receipt.palette == target.palette &&
                        receipt.physicalWidth == target.physicalWidth &&
                        receipt.physicalHeight == target.physicalHeight &&
                        receipt.semanticPrimitiveCount ==
                            current.chrome.primitives.size() &&
                        receipt.submittedBatchCount > 0 &&
                        receipt.submittedDrawCallCount > 0 &&
                        receipt.submittedSolidVertexCount > 0 &&
                        receipt.submittedSdfVertexCount > 0 &&
                        exactSemanticRows &&
                        focusRows == static_cast<std::size_t>(focused ? 1 : 0) &&
                        receipt.orderedPrimitiveDigest ==
                            chromePrimitiveDigest(current.chrome) &&
                        receipt.orderedBatchDigest ==
                            submittedBatchDigest(receipt) &&
                        receipt.cutoutZeroCoverageProofDigest ==
                            independentRaster.digest &&
                        receipt.coveredPhysicalSampleCount ==
                            independentRaster.coveredSamples &&
                        !receipt.coveredPhysicalSamples.empty() &&
                        physicalSdfFringeOutsideCutout &&
                        independentRaster.cutoutUntouched &&
                        exactSemanticStreamUnion(receipt, positiveDispatch) &&
                        positiveDispatch.targetBindQueries == 1 &&
                        positiveDispatch.actualVertices.size() ==
                            receipt.submittedSolidVertexCount +
                                receipt.submittedSdfVertexCount &&
                        positiveDispatch.actualBatches == receipt.submittedBatches &&
                        !positiveDispatch.stateDirty &&
                        positiveDispatch.stateRestores == 1,
                    "D exact current chrome has actual resource-bound UI receipt");
            ++positiveCases;
          }
  }
  require(core41Only || positiveCases == 96,
          "D chrome correction exact 96-case matrix is complete");

  auto current = ui::makeTrajectoryInsetVisibleCurrentFrame(
      checkpointBSession(game::ProductScreen::TrainingSession), presentation,
      {true, false, false}, checkpointBContext(serial++));
  billiardgl::render::TrajectoryInsetProductUiChromeTarget baseTarget;
  baseTarget.sessionSerial = current.request.sessionSerial;
  baseTarget.frameSerial = current.request.frameSerial;
  baseTarget.layoutSerial = current.request.layoutSerial;
  baseTarget.cp1GenerationId = current.request.cp1GenerationId;
  baseTarget.cp1Digests = current.request.cp1Digests;
  baseTarget.cp1GeometryDigest = current.request.cp1GeometryDigest;
  baseTarget.cp2LayoutDigest = current.request.cp2LayoutDigest;
  baseTarget.cp2ExclusionDigest = current.request.cp2ExclusionDigest;
  baseTarget.chromeSemanticDigest = chromePrimitiveDigest(current.chrome);
  baseTarget.palette = current.request.palette;
  baseTarget.physicalWidth = current.shellRequest.physicalWidth;
  baseTarget.physicalHeight = current.shellRequest.physicalHeight;
  baseTarget.samples = current.request.samples;
  baseTarget.currentLdrFramebuffer = 701;
  baseTarget.currentLdrColorTexture = 702;

  for (const bool restoreThrows : {false, true}) {
    core41_gl_link_shim::reset();
    int targetQueries = 0;
    int adjacentErrorQueries = 0;
    ui::Core41ProductUiGlDispatch actualCore41;
    actualCore41.configureTrajectoryInsetChromeQueries(
        [&] {
          ++targetQueries;
          return ui::TrajectoryInsetChromeGpuTargetBinding{701, 702, true};
        },
        [&] {
          ++adjacentErrorQueries;
          return static_cast<std::uint32_t>(GL_NO_ERROR);
        });
    billiardgl::render::ModernProductUiRuntime actualRuntime(actualCore41);
    const auto config = chromeRuntimeConfig();
    require(actualRuntime.initialize(config).ok &&
                actualRuntime.resize(baseTarget.physicalWidth,
                                     baseTarget.physicalHeight, 96.0f).ok,
            "D actual Core41 runtime/GPU transaction initializes");
    core41_gl_link_shim::state.restoreAttempts = 0;
    auto actualTarget = baseTarget;
    actualTarget.atlasCacheKey =
        actualRuntime.trajectoryInsetChromeAtlasCacheKey();
    core41_gl_link_shim::state.injectionActive = true;
    core41_gl_link_shim::state.beginSetupError = true;
    core41_gl_link_shim::state.restoreError = !restoreThrows;
    core41_gl_link_shim::state.restoreThrow = restoreThrows;
    const auto failed =
        actualRuntime.submitTrajectoryInsetChrome(current, actualTarget);
    require(!failed.ok && failed.adapterContextPoisoned &&
                targetQueries == 1 &&
                core41_gl_link_shim::state.restoreAttempts == 1 &&
                failed.finalPrepareCalls == 0 &&
                failed.terminalPublishCalls == 0 &&
                failed.productPresentCalls == 0,
            "D actual Core41 through runtime/GPU captured begin failure restores exactly once and poisons");
    const std::array<int, 3> callsBefore{{
        targetQueries, adjacentErrorQueries,
        core41_gl_link_shim::state.restoreAttempts}};
    const auto poisonedRetry =
        actualRuntime.submitTrajectoryInsetChrome(current, actualTarget);
    require(!poisonedRetry.ok && poisonedRetry.adapterContextPoisoned &&
                callsBefore == std::array<int, 3>{{
                                   targetQueries, adjacentErrorQueries,
                                   core41_gl_link_shim::state.restoreAttempts}} &&
                poisonedRetry.finalPrepareCalls == 0 &&
                poisonedRetry.productPresentCalls == 0,
            "D actual Core41 poisoned same-context retry performs zero work");

    actualRuntime.contextLost();
    core41_gl_link_shim::reset();
    core41_gl_link_shim::state.nextHandle = 500;
    int recoveredTargetQueries = 0;
    ui::Core41ProductUiGlDispatch recoveredCore41;
    recoveredCore41.configureTrajectoryInsetChromeQueries(
        [&] {
          ++recoveredTargetQueries;
          return ui::TrajectoryInsetChromeGpuTargetBinding{1701, 1702, true};
        }, [] { return static_cast<std::uint32_t>(GL_NO_ERROR); });
    billiardgl::render::ModernProductUiRuntime recovered(recoveredCore41);
    require(recovered.initialize(config).ok &&
                recovered.resize(baseTarget.physicalWidth,
                                 baseTarget.physicalHeight, 96.0f).ok,
            "D context loss permits fresh actual Core41 resource namespace");
    core41_gl_link_shim::state.restoreAttempts = 0;
    auto recoveredTarget = baseTarget;
    recoveredTarget.atlasCacheKey =
        recovered.trajectoryInsetChromeAtlasCacheKey();
    recoveredTarget.currentLdrFramebuffer = 1701;
    recoveredTarget.currentLdrColorTexture = 1702;
    const auto recoveredReceipt =
        recovered.submitTrajectoryInsetChrome(current, recoveredTarget);
    if (!recoveredReceipt.ok || recoveredTargetQueries != 1 ||
        core41_gl_link_shim::state.restoreAttempts != 1)
      std::cerr << "CORE41_RECOVERY_DIAG error="
                << recoveredReceipt.errorCode << " poisoned="
                << recoveredReceipt.adapterContextPoisoned << " targetQueries="
                << recoveredTargetQueries << " restores="
                << core41_gl_link_shim::state.restoreAttempts << '\n';
    require(recoveredReceipt.ok && recoveredTargetQueries == 1 &&
                core41_gl_link_shim::state.restoreAttempts == 1,
            "D explicit context loss plus fresh actual Core41/runtime recovers N+2");
  }

  for (int mutation = 0; mutation < 13; ++mutation) {
    auto stale = current;
    auto target = baseTarget;
    if (mutation == 0) ++stale.request.sessionSerial;
    if (mutation == 1) ++stale.request.frameSerial;
    if (mutation == 2) ++stale.request.layoutSerial;
    if (mutation == 3) stale.request.cp1Digests[0] += "drift";
    if (mutation == 4) stale.request.cp1GeometryDigest += "drift";
    if (mutation == 5) stale.request.cp2LayoutDigest += "drift";
    if (mutation == 6) stale.request.cp2ExclusionDigest += "drift";
    if (mutation == 7) stale.chrome.primitives.front().rect.x += 1.0f;
    if (mutation == 8)
      stale.chrome.primitives.push_back(stale.chrome.primitives.front());
    if (mutation == 9)
      stale.chrome.orderedText[0] = "missing glyph \xF4\x8F\xBF\xBF";
    if (mutation == 10) stale.layout.insetControlFocused = true;
    if (mutation == 11)
      stale.chrome.primitives.front().rect = stale.layout.plotCutoutRect;
    if (mutation == 12) target.atlasCacheKey = "stale_cached_atlas";
    CheckpointChromeGlDispatch dispatch;
    billiardgl::render::ModernProductUiRuntime runtime(dispatch);
    const auto config = chromeRuntimeConfig();
    require(runtime.initialize(config).ok &&
                runtime.resize(baseTarget.physicalWidth,
                               baseTarget.physicalHeight, 96.0f).ok,
            "D mutation runtime reaches initialized current atlas/dispatch");
    baseTarget.atlasCacheKey = runtime.trajectoryInsetChromeAtlasCacheKey();
    target.atlasCacheKey = mutation == 12 ? "stale_cached_atlas"
                                           : baseTarget.atlasCacheKey;
    const auto failed = runtime.submitTrajectoryInsetChrome(stale, target);
    require(!failed.ok && failed.finalPrepareCalls == 0 &&
                failed.terminalPublishCalls == 0 &&
                failed.productPresentCalls == 0 &&
                failed.fallbackPresentCalls == 0,
            "D noncurrent/cache/glyph/focus/cutout mutations fail pre-present");
  }

  for (const auto failure : {CheckpointChromeGlDispatch::Failure::Create,
                             CheckpointChromeGlDispatch::Failure::Begin,
                             CheckpointChromeGlDispatch::Failure::Upload,
                             CheckpointChromeGlDispatch::Failure::Scissor,
                             CheckpointChromeGlDispatch::Failure::Draw,
                             CheckpointChromeGlDispatch::Failure::End})
    for (const bool throwing : {false, true}) {
      CheckpointChromeGlDispatch dispatch;
      billiardgl::render::ModernProductUiRuntime runtime(dispatch);
      const auto config = chromeRuntimeConfig();
      if (failure == CheckpointChromeGlDispatch::Failure::Create) {
        if (throwing) dispatch.throwAt = failure;
        else dispatch.fail = failure;
        bool initializationFailed = false;
        try { initializationFailed = !runtime.initialize(config).ok; }
        catch (...) { initializationFailed = true; }
        require(initializationFailed && dispatch.creates == 1,
                "D create failure/throw reaches exact initialization stage");
        continue;
      }
      require(runtime.initialize(config).ok &&
                  runtime.resize(baseTarget.physicalWidth,
                                 baseTarget.physicalHeight, 96.0f).ok,
              "D stage failure fixture initializes before injection");
      baseTarget.atlasCacheKey = runtime.trajectoryInsetChromeAtlasCacheKey();
      if (throwing) dispatch.throwAt = failure;
      else dispatch.fail = failure;
      const auto failed =
          runtime.submitTrajectoryInsetChrome(current, baseTarget);
      require(!failed.ok && failed.finalPrepareCalls == 0 &&
                  failed.terminalPublishCalls == 0 &&
                  failed.productPresentCalls == 0 &&
                  failed.fallbackPresentCalls == 0 &&
                  (failure == CheckpointChromeGlDispatch::Failure::End
                       ? failed.adapterContextPoisoned
                       : !dispatch.stateDirty),
              "D GL failure/throw restores or poisons and forbids fallback");
      if (failure == CheckpointChromeGlDispatch::Failure::End) {
        const std::array<int, 5> callsBefore{{dispatch.begins, dispatch.uploads,
                                              dispatch.draws, dispatch.ends,
                                              dispatch.targetBindQueries}};
        const auto poisonedRetry =
            runtime.submitTrajectoryInsetChrome(current, baseTarget);
        require(!poisonedRetry.ok && poisonedRetry.adapterContextPoisoned &&
                    callsBefore == std::array<int, 5>{{
                                       dispatch.begins, dispatch.uploads,
                                       dispatch.draws, dispatch.ends,
                                       dispatch.targetBindQueries}} &&
                    poisonedRetry.finalPrepareCalls == 0 &&
                    poisonedRetry.productPresentCalls == 0,
                "D restore poison same-context retry performs exact zero work");
        runtime.contextLost();
        CheckpointChromeGlDispatch recoveredDispatch;
        billiardgl::render::ModernProductUiRuntime recovered(recoveredDispatch);
        require(recovered.initialize(config).ok &&
                    recovered.resize(baseTarget.physicalWidth,
                                     baseTarget.physicalHeight, 96.0f).ok,
                "D explicit context loss permits fresh runtime namespace");
        auto recoveredTarget = baseTarget;
        recoveredTarget.atlasCacheKey =
            recovered.trajectoryInsetChromeAtlasCacheKey();
        const auto recoveredReceipt =
            recovered.submitTrajectoryInsetChrome(current, recoveredTarget);
        require(recoveredReceipt.ok &&
                    recoveredDispatch.targetBindQueries == 1 &&
                    recoveredDispatch.actualCurrentLdrFramebuffer == 701 &&
                    recoveredDispatch.actualCurrentLdrColorTexture == 702,
                "D context loss plus fresh runtime recovers exact new target");
      }
    }

  for (int mutation = 0; mutation < 3; ++mutation) {
    CheckpointChromeGlDispatch dispatch;
    billiardgl::render::ModernProductUiRuntime runtime(dispatch);
    billiardgl::render::ModernRendererProductUiPass pass(runtime);
    const auto config = chromeRuntimeConfig();
    require(pass.initialize(config).ok &&
                runtime.resize(baseTarget.physicalWidth,
                               baseTarget.physicalHeight, 96.0f).ok,
            "D target/error fixture initializes concrete pass/runtime");
    auto target = baseTarget;
    target.atlasCacheKey = runtime.trajectoryInsetChromeAtlasCacheKey();
    if (mutation == 0) dispatch.targetBindingValid = false;
    if (mutation == 1) ++dispatch.actualCurrentLdrColorTexture;
    if (mutation == 2) dispatch.adjacentGlError = 0x0502;
    const auto failed = pass.submitTrajectoryInsetChrome(current, target);
    require(!failed.ok && dispatch.targetBindQueries == 1 &&
                (mutation == 2 ? dispatch.adjacentErrorQueries == 1 : true) &&
                failed.finalPrepareCalls == 0 &&
                failed.terminalPublishCalls == 0 &&
                failed.productPresentCalls == 0 &&
                failed.fallbackPresentCalls == 0,
            "D invalid/mismatched actual LDR target or adjacent GL error fails");
  }

  for (const bool restoreThrows : {false, true}) {
    CheckpointChromeGlDispatch dispatch;
    billiardgl::render::ModernProductUiRuntime runtime(dispatch);
    const auto config = chromeRuntimeConfig();
    require(runtime.initialize(config).ok &&
                runtime.resize(baseTarget.physicalWidth,
                               baseTarget.physicalHeight, 96.0f).ok,
            "D production-style begin/restore fixture initializes");
    auto target = baseTarget;
    target.atlasCacheKey = runtime.trajectoryInsetChromeAtlasCacheKey();
    dispatch.productionStyleBeginFailure = true;
    dispatch.productionStyleRestoreFailure = !restoreThrows;
    dispatch.productionStyleRestoreThrow = restoreThrows;
    const auto failed = runtime.submitTrajectoryInsetChrome(current, target);
    require(!failed.ok && failed.adapterContextPoisoned &&
                dispatch.begins == 1 && dispatch.ends == 1 &&
                dispatch.stateRestores == 0 && dispatch.stateDirty &&
                failed.finalPrepareCalls == 0 &&
                failed.terminalPublishCalls == 0 &&
                failed.productPresentCalls == 0,
            "D production-style begin fail plus restore fail/throw poisons once");
    const std::array<int, 5> callsBefore{{dispatch.begins, dispatch.uploads,
                                          dispatch.draws, dispatch.ends,
                                          dispatch.targetBindQueries}};
    const auto retry = runtime.submitTrajectoryInsetChrome(current, target);
    require(!retry.ok && retry.adapterContextPoisoned &&
                callsBefore == std::array<int, 5>{{
                                   dispatch.begins, dispatch.uploads,
                                   dispatch.draws, dispatch.ends,
                                   dispatch.targetBindQueries}},
            "D production-style restore poison persists on same context");
  }

  {
    const auto read = [](const char *path) {
      std::ifstream input(path);
      return std::string(std::istreambuf_iterator<char>(input),
                         std::istreambuf_iterator<char>());
    };
    const auto occurrences = [](const std::string &source,
                                const std::string &needle) {
      std::size_t count = 0, position = 0;
      while ((position = source.find(needle, position)) != std::string::npos) {
        ++count;
        position += needle.size();
      }
      return count;
    };
    const std::string renderer =
        read("src/Billiards/render/modern/modern_renderer.cpp");
    const std::string passSource =
        read("src/Billiards/render/modern/modern_renderer_product_ui.cpp");
    const std::string runtimeSource =
        read("src/Billiards/render/modern/modern_product_ui_runtime.cpp");
    const auto productUi = renderer.find("product_ui");
    const auto chromeSubmit =
        renderer.find("productUiPass->submitTrajectoryInsetChrome(");
    const auto validateReceipt =
        renderer.find("validateTrajectoryInsetProductUiChromeReceipt(",
                      chromeSubmit);
    const auto sealChrome =
        renderer.find("productChromeAlreadyInCurrentLdr = true",
                      validateReceipt);
    const auto finalPrepare =
        renderer.find("prepareTrajectoryInsetVisibleFinalStaging(", sealChrome);
    require(chromeSubmit != std::string::npos &&
                validateReceipt != std::string::npos &&
                sealChrome != std::string::npos &&
                finalPrepare != std::string::npos &&
                occurrences(renderer,
                        "productUiPass->submitTrajectoryInsetChrome(") == 1 &&
                productUi < chromeSubmit && chromeSubmit < validateReceipt &&
                validateReceipt < sealChrome && sealChrome < finalPrepare,
            "D renderFrame exact-one current chrome receipt precedes final stage");
    require(occurrences(passSource, "submitTrajectoryInsetChrome(") == 2 &&
                passSource.find("backend_.submitTrajectoryInsetChrome(") !=
                    std::string::npos &&
                occurrences(runtimeSource, "submitTrajectoryInsetChrome(") == 2 &&
                runtimeSource.find(
                    "trajectory_inset_chrome_echo_receipt_forbidden") !=
                    std::string::npos,
            "D typed receipt traverses Product UI pass into concrete runtime");
    require(renderer.find("trajectoryInsetAcceptedFailureBlocksProductPresent") !=
                    std::string::npos &&
                renderer.find("productOutput ? productOutput") ==
                    std::string::npos &&
                renderer.find("invalidateTrajectoryInsetVisibleFinalStaging") <
                    renderer.find("trajectoryInsetAcceptedFailureBlocksProductPresent"),
            "D accepted chrome failure invalidates and forbids ordinary-LDR fallback");
  }

  // A permissive/no-op dispatcher cannot turn an echoed request into proof.
  CheckpointChromeGlDispatch noOp;
  billiardgl::render::ModernProductUiRuntime noOpRuntime(noOp);
  const auto noOpConfig = chromeRuntimeConfig();
  require(noOpRuntime.initialize(noOpConfig).ok &&
              noOpRuntime.resize(baseTarget.physicalWidth,
                                 baseTarget.physicalHeight, 96.0f).ok,
          "D echo/no-op fixture uses initialized runtime");
  baseTarget.atlasCacheKey = noOpRuntime.trajectoryInsetChromeAtlasCacheKey();
  const auto echoed = noOpRuntime.submitTrajectoryInsetChrome(
      current, baseTarget,
      billiardgl::render::TrajectoryInsetProductUiChromeReceipt::echoForTest(
          current, baseTarget));
  require(!echoed.ok && echoed.submittedBatchCount == 0 &&
              echoed.submittedDrawCallCount == 0 &&
              echoed.finalPrepareCalls == 0 && echoed.terminalPublishCalls == 0,
          "D fake echoed receipt/no-op work cannot establish chrome presence");
}

} // namespace

int main() {
  actualCore41SingleRestoreOwnershipRed();
  fractionalCutoutSampleRegression();
  positive48();
  rejectedScreensAndAuthority();
  identityMutations();
  preparationIdentityMutations();
  validFailValid();
  errorsThrowsAndLifecycle();
  checkpointBExplicitCurrentRequestAdapter();
  checkpointCFinalAtomicStaging();
  checkpointDRealGlFinalStagingAdapterContract();
  checkpointDGlMockTransitions();
  checkpointDProductUiChromeSubmissionCorrectionRed();
  std::cout << "PASS phase4_v19_cp4_visible_integration positive=48 "
               "screen_authority_negatives=17 identity_negatives=16 "
               "valid_fail_valid=8 gl_error_throw=16 lifecycle=5 "
               "checkpoint_b_positive=2 rejected_screens=7 "
               "checkpoint_b_authority=10 checkpoint_b_failures=4 "
               "checkpoint_c_samples=3 checkpoint_c_atomic_failures=24 "
               "checkpoint_c_resource_negatives=29 "
               "checkpoint_d_gl_static=1 "
               "checkpoint_d_samples=3 checkpoint_d_failures=33 "
               "world_calls=0 gpu=0 billiards=0 readiness=0 auth=0\n";
}
