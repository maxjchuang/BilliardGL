#include "render/modern/modern_renderer.h"
#include "render/modern/trajectory_inset_atomic_transaction.h"
#include "render/modern/trajectory_inset_visible_integration.h"
#include "render/ui/product_ui_frame_coordinator.h"
#include "training/trajectory_inset_model.h"
#include "training/trajectory_inset_renderer.h"

#include <array>
#include <algorithm>
#include <cstdlib>
#include <iostream>
#include <fstream>
#include <stdexcept>
#include <string>

namespace {

namespace modern = billiardgl::render::modern;
namespace training = billiardgl::training;
namespace ui = billiardgl::render::ui;

constexpr std::array<const char *, 5> kRequiredDemangledSymbolAnchors{{
    "billiardgl::render::ModernRenderer::prepareTrajectoryInsetVisibleFinalStaging",
    "billiardgl::training::trajectoryInsetRoleName",
    "billiardgl::training::trajectoryInsetSampleProfile1x",
    "billiardgl::render::modern::validateTrajectoryInsetAtomicIdentity",
    "billiardgl::render::modern::orderedTrajectoryInsetVisibleAtomicFinalStages",
}};

void expect(bool condition, const char *message) {
  if (!condition)
    throw std::runtime_error(message);
}

void verifyImplementationObjectAnchors() {
  expect(std::string(training::trajectoryInsetRoleName(
             training::TrajectoryInsetRole::ActualLeftCore)) ==
             "actual_left_core",
         "trajectory inset model object anchor failed");

  const training::TrajectoryInsetSampleProfile profile =
      training::trajectoryInsetSampleProfile1x();
  expect(profile.id == "center_1x" && profile.positions.size() == 1 &&
             profile.positions[0][0] == 0.5f &&
             profile.positions[0][1] == 0.5f,
         "trajectory inset renderer object anchor failed");

  modern::TrajectoryInsetAtomicIdentityContract identity;
  std::string identityError;
  expect(!modern::validateTrajectoryInsetAtomicIdentity(identity,
                                                         identityError) &&
             identityError ==
                 "trajectory_inset_generation_or_serial_identity_invalid",
         "trajectory inset atomic transaction object anchor failed");

  const auto stages = modern::orderedTrajectoryInsetVisibleAtomicFinalStages();
  expect(stages.size() == 8 &&
             stages.front() ==
                 modern::TrajectoryInsetVisibleAtomicFinalStage::FreezeIdentity &&
             stages.back() ==
                 modern::TrajectoryInsetVisibleAtomicFinalStage::AtomicPresent,
         "trajectory inset visible integration object anchor failed");
}

void verifyModernRendererPreinitializedCp4FailClosed() {
  billiardgl::render::ModernRenderer renderer(
      billiardgl::render::ModernRendererOptions{});
  ui::TrajectoryInsetVisibleCurrentFrame current;
  std::string error = "sentinel";
  expect(!renderer.prepareTrajectoryInsetVisibleFinalStaging(current, error),
         "preinitialized CP4 staging unexpectedly succeeded");
  expect(error == "trajectory_inset_final_missing_cp3_staging",
         "preinitialized CP4 staging returned the wrong failure");
  expect(renderer.publishedProductOutputFramebuffer() == 0,
         "preinitialized CP4 staging published an output framebuffer");
}

training::TrajectoryInsetDrawPackage markerFixture(const char* name, training::TrajectoryInsetPalette palette) {
  std::ifstream in(std::string("tests/fixtures/trajectory_inset_") + name + "_20260930.txt");
  expect(static_cast<bool>(in), "marker fixture missing");
  training::TrajectoryInsetBuildRequest request;
  request.generationId="consumer-marker-regression";
  request.palette=palette;
  request.layout.profileId=name;
  std::string tag; in >> tag;
  expect(tag=="FIX_LAYOUT", "fixture layout");
  auto& l=request.layout; auto& p=l.panelRect; auto& q=l.plotRect;
  in >> l.physicalWidth >> l.physicalHeight >> l.uiScale >> p.x >> p.y >> p.width >> p.height
     >> q.x >> q.y >> q.width >> q.height;
  for(int t=0;t<2;++t) {
    int id=0; std::size_t count=0; in >> tag >> id;
    expect(tag=="FIX_TRACK" && id==t, "fixture track");
    auto& track=t ? request.snapshot.actual : request.snapshot.predicted;
    in >> track.visible >> track.endpointVisible >> track.endpoint.x >> track.endpoint.y
       >> track.endpoint.z >> track.railCollisionEvents >> count;
    for(std::size_t i=0;i<count;++i) {
      training::OverlaySample sample; in >> tag >> sample.tick >> sample.position.x
        >> sample.position.y >> sample.position.z >> sample.visible;
      expect(tag=="FIX_SAMPLE", "fixture sample"); track.samples.push_back(sample);
    }
  }
  in >> tag; expect(static_cast<bool>(in) && tag=="FIX_END", "fixture end");
  return training::buildTrajectoryInsetDrawPackage(request);
}

void verifyMarkerConsumerIdentity() {
  for(auto palette : {training::TrajectoryInsetPalette::Standard, training::TrajectoryInsetPalette::ColorVisionSafe})
  for(const char* name : {"retrace", "subpixel_tail"}) {
    const auto package=markerFixture(name,palette);
    expect(package.ok, "marker fixture CP1 rejected");
    billiardgl::render::TrajectoryInsetAtomicFrameInput input;
    input.cp1GenerationId=package.generationId;
    input.cp1OrderedDigests={{package.orderedParentSourceDigest, package.orderedProbeDigest,
      package.orderedDashEventDigest, package.orderedDashContributorDigest, package.orderedSourceRouteIntervalDigest}};
    std::string error;
    if(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(package,input,error))
      throw std::runtime_error(std::string(name)+":"+error);
    const auto leaderIndex=std::find_if(package.semanticBatches.begin(),package.semanticBatches.end(),
      [](const auto& b){return b.role==training::TrajectoryInsetRole::PredictedEventVisitLeader;})-package.semanticBatches.begin();
    expect(leaderIndex < static_cast<std::ptrdiff_t>(package.semanticBatches.size()), "strict test leader missing");
    auto bad=package;
    bad.semanticBatches[leaderIndex].role=static_cast<training::TrajectoryInsetRole>(999);
    expect(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(bad,input,error) &&
      error=="trajectory_inset_cp1_unknown_role", "unknown visit role must fail closed");
    bad=package; bad.semanticBatches.back().role=static_cast<training::TrajectoryInsetRole>(999);
    expect(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(bad,input,error) &&
      error=="trajectory_inset_cp1_unknown_role", "unknown original role must fail closed");
    bad=package; bad.semanticBatches[leaderIndex].primitiveId="visit_leader_foreign";
    expect(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(bad,input,error) &&
      error=="trajectory_inset_cp1_visit_mapping_invalid", "foreign leader identity must fail closed");
    bad=package; bad.semanticBatches[leaderIndex].parentSourceKey="foreign_event";
    expect(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(bad,input,error),
      "orphan leader must fail closed");
    bad=package; bad.semanticBatches[leaderIndex].role=training::TrajectoryInsetRole::PredictedEventVisitMarker;
    expect(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(bad,input,error),
      "leader cannot duplicate glyph role");
    bad=package;
    const auto visit=std::find_if(bad.probes.begin(),bad.probes.end(),
      [](const auto& p){return !p.sourceCoreMemberKey.empty();});
    expect(visit!=bad.probes.end(), "strict test visit missing");
    visit->sourceCoreMemberKey.clear();
    expect(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(bad,input,error),
      "discarded original source provenance must fail closed");
    bad=package; bad.semanticBatches[leaderIndex].insetOwnershipToken=package.semanticBatches[(leaderIndex+1)%package.semanticBatches.size()].insetOwnershipToken;
    expect(!billiardgl::render::validateTrajectoryInsetPackageForSubmission(bad,input,error),
      "duplicate ownership token must fail closed");
    std::cout << "consumer_marker_identity fixture=" << name << " palette=" << static_cast<int>(palette)
      << " events=" << package.predictedDashEvents.size() << " probes=" << package.probeDenominator
      << " result=PASS unknown_role=REJECT foreign_leader=REJECT orphan_leader=REJECT duplicate_glyph=REJECT missing_source=REJECT duplicate_token=REJECT\n";
  }
}

} // namespace

int main() {
  try {
    for (const char *anchor : kRequiredDemangledSymbolAnchors)
      expect(anchor != nullptr && anchor[0] != '\0',
             "empty final-link symbol anchor");
    verifyImplementationObjectAnchors();
    verifyModernRendererPreinitializedCp4FailClosed();
    verifyMarkerConsumerIdentity();
    std::cout << "PASS_PHASE4_V19_MODERN_RENDERER_CP4_FINAL_LINK "
                 "anchors=5 gpu_context=0 gpu_calls=0\n";
    return EXIT_SUCCESS;
  } catch (const std::exception &error) {
    std::cerr << "FAIL_PHASE4_V19_MODERN_RENDERER_CP4_FINAL_LINK: "
              << error.what() << '\n';
    return EXIT_FAILURE;
  }
}
