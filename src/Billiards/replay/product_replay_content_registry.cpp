#include "replay/product_replay_content_registry.h"

#include "deterministic_hash.h"
#include "rules/rack_executor.h"

#include <sstream>
#include <utility>

namespace billiardgl::replay {
namespace {

ProductReplayContentRegistryResult success()
{
    return {true,""};
}

ProductReplayContentRegistryResult failure(const char* code)
{
    return {false,code};
}

bool validHash(const std::string& value)
{
    if(value.size()!=64)return false;
    for(char digit:value)
        if(!((digit>='0'&&digit<='9')||(digit>='a'&&digit<='f')))
            return false;
    return true;
}

}  // namespace

TrustedReplayContentBinding officialSingleRackReplayBinding(
    const PhysicsProfile& profile)
{
    TrustedReplayContentBinding binding;
    binding.physicsProfile=profile;
    binding.physicsProfileId=profile.id;
    binding.physicsProfileSha256=physicsProfileSha256(profile);
    binding.scope=ProductReplayContentScope::SingleRackMatch;
    binding.minimumFormatVersion=kReplayFormatVersion;
    binding.maximumFormatVersion=kReplayRulesFormatVersion;
    binding.allowsLegacyRules=false;
    binding.allowsChineseEightBallRules=true;
    GameRuntime official;
    const ActionResult installed=official.replaceStateForScenario(
        official.state(),profile,nullptr,false,
        PhysicsBoundaryMode::ProductionTable);
    const ActionResult rulesInstalled=installed.ok ?
        official.setRulesMode(RulesMode::ChineseEightBallV1) : installed;
    rules::RerackFoundationParameters parameters;
    parameters.deterministicSeed=rules::kOfficialRackDefaultSeed;
    parameters.breakerPlayerId=0;
    const rules::RackFoundationPlan rack=rulesInstalled.ok ?
        official.stageOfficialRerack(parameters) :
        rules::RackFoundationPlan{};
    const rules::RackFoundationResult committed=rack.ok ?
        official.commitRackFoundation(
            rack,"product.match.start.official-rack-v1") :
        rules::RackFoundationResult{};
    if(committed.ok){
        binding.initialStateSha256=gameStateSha256(official.state());
        binding.initialAuthorityStateSha256=
            replayAuthorityStateSha256(official.state());
        binding.initialRulesRevision=official.rulesAuthorityRevision();
        binding.initialRulesEffectSha256=
            official.rulesReplayEffectStateHash();
    }
    std::ostringstream canonical;
    canonical<<"billiardgl_product_single_rack_match_v1|"
        <<kOfficialSingleRackReplayRulesetId<<'|'
        <<"ChineseEightBallV1|ProductionTable|"
        <<rules::kOfficialRackAlgorithmId<<'|'
        <<rules::kOfficialRackDefaultSeed<<"|0|"
        <<binding.physicsProfileId<<'|'<<binding.physicsProfileSha256;
    binding.contentSha256=sha256Hex(canonical.str());
    return binding;
}

ProductReplayContentRegistryResult
ProductReplayContentRegistry::addTrustedContent(
    TrustedReplayContentBinding binding)
{
    const bool hasAnyInitialBinding=!binding.initialStateSha256.empty()||
        !binding.initialAuthorityStateSha256.empty()||
        binding.initialRulesRevision!=0||
        !binding.initialRulesEffectSha256.empty();
    const bool hasCompleteInitialBinding=
        validHash(binding.initialStateSha256)&&
        validHash(binding.initialAuthorityStateSha256)&&
        validHash(binding.initialRulesEffectSha256);
    if(!validHash(binding.contentSha256)||binding.physicsProfileId.empty()||
       !validHash(binding.physicsProfileSha256)||
       binding.physicsProfileId!=binding.physicsProfile.id||
       binding.physicsProfileSha256!=physicsProfileSha256(
            binding.physicsProfile)||
       binding.minimumFormatVersion<kReplayFormatVersion||
       binding.maximumFormatVersion>kReplayRulesFormatVersion||
       binding.minimumFormatVersion>binding.maximumFormatVersion||
       (binding.scope==ProductReplayContentScope::SingleRackMatch&&
        (!hasCompleteInitialBinding||binding.initialRulesRevision==0))||
       (binding.scope==ProductReplayContentScope::TrainingAttempt&&
        hasAnyInitialBinding&&!hasCompleteInitialBinding)||
       (!binding.allowsLegacyRules&&
        !binding.allowsChineseEightBallRules))
        return failure("product_replay_registry_binding_invalid");
    if(bindings_.count(binding.contentSha256)!=0)
        return failure("product_replay_registry_duplicate_content");
    bindings_.emplace(binding.contentSha256,std::move(binding));
    return success();
}

ProductReplayContentRegistryResult
ProductReplayContentRegistry::addTrainingAttempt(
    const std::string& contentSha256,const PhysicsProfile& profile)
{
    TrustedReplayContentBinding binding;
    binding.contentSha256=contentSha256;
    binding.physicsProfileId=profile.id;
    binding.physicsProfileSha256=physicsProfileSha256(profile);
    binding.physicsProfile=profile;
    binding.scope=ProductReplayContentScope::TrainingAttempt;
    binding.minimumFormatVersion=kReplayFormatVersion;
    binding.maximumFormatVersion=kReplayFormatVersion;
    binding.allowsLegacyRules=true;
    binding.allowsChineseEightBallRules=true;
    return addTrustedContent(std::move(binding));
}

ProductReplayContentRegistryResult
ProductReplayContentRegistry::addOfficialSingleRack(
    const PhysicsProfile& profile)
{
    return addTrustedContent(officialSingleRackReplayBinding(profile));
}

ProductReplayContentRegistryResult ProductReplayContentRegistry::authorize(
    const Document& document,const TrustedReplayContentBinding*& binding) const
{
    binding=nullptr;
    const auto found=bindings_.find(document.contentSha256);
    if(found==bindings_.end())
        return failure("product_replay_content_untrusted");
    const TrustedReplayContentBinding& candidate=found->second;
    if(document.physicsProfileId!=candidate.physicsProfileId||
       document.physicsProfileSha256!=candidate.physicsProfileSha256)
        return failure("product_replay_binding_mismatch");
    if(!candidate.initialStateSha256.empty()&&
       (gameStateSha256(document.initialState)!=
            candidate.initialStateSha256||
        replayAuthorityStateSha256(document.initialState)!=
            candidate.initialAuthorityStateSha256||
        document.initialRulesRevision!=candidate.initialRulesRevision||
        document.initialRulesEffectSha256!=
            candidate.initialRulesEffectSha256))
        return failure("product_replay_initial_authority_mismatch");
    if(document.formatVersion<candidate.minimumFormatVersion||
       document.formatVersion>candidate.maximumFormatVersion)
        return failure("product_replay_format_scope_mismatch");
    if((document.rulesMode==RulesMode::Legacy&&
            !candidate.allowsLegacyRules)||
       (document.rulesMode==RulesMode::ChineseEightBallV1&&
            !candidate.allowsChineseEightBallRules))
        return failure("product_replay_rules_scope_mismatch");
    binding=&candidate;
    return success();
}

}  // namespace billiardgl::replay
