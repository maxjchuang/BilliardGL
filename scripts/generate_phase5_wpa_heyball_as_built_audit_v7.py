#!/usr/bin/env python3
"""Generate the read-only NONFINAL WPA-II-09 selected-scope audit successor."""
from __future__ import annotations
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
V6=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v6.json'
OUT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v7.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ev(kind,path,start,end,finding):return {'kind':kind,'path':path,'sha256':sha(ROOT/path),'lines':{'start':start,'end':end},'finding':finding}
def build():
 v=json.loads(V6.read_text());v['schema']='phase5_wpa_heyball_as_built_reconciliation_audit_v7';v['schema_version']=7;v['audit_id']='phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v7';v['audit_scope']='wpa_ii_09_root_selected_coordinate_only_completion_successor';v['audit_status']='FINAL_ROOT_ACCEPTED_WPA_II_09_ONLY_SUCCESSOR';v['frozen_v6_predecessor']={'path':str(V6.relative_to(ROOT)),'sha256':sha(V6)}
 r=next(x for x in v['matrix'] if x['id']=='WPA-II-09')
 r.update({'status':'IMPLEMENTED','product_reachable':True,'assessment':'Within the root-selected M4 shipping policy COORDINATE_PLACEMENT_ONLY_POSITIVE_IMPULSE_CONSUMES, WPA-II §9 is product reachable and complete for the shipped coordinate-placement scope: the current actor may repeatedly preview and commit legal Anywhere/BehindHeadString cue-ball coordinates; every commit retains BIH and atomically replaces the exact offer; only one successful authoritative applied finite strictly-positive cue-ball impulse consumes the entitlement and starts the shot lifecycle; zero, rejected, unsupported, non-applied miscue and failed-launch attempts preserve it; reset/turn invalidation and retries fail closed. Physical cue-part reposition and forward-stroke classification remain deliberately DISABLED_UNSUPPORTED and are a root-selected product-scope defer, not a claim that those physical/referee capabilities were implemented.','gaps':[],'evidence':[
  ev('policy_selected','docs/rules/m4-wpa-policy-bundle-root-selected-v1.json',87,92,'Root selected the coordinate-only positive-impulse consumption policy for WPA-II-09.'),
  ev('implementation_design_gate','docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-design-gate-v1.json',1,160,'The independently accepted gate freezes replacement-offer, positive-impulse, rollback and unsupported-boundary contracts.'),
  ev('implementation_receipt','docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-receipt-v1.json',1,150,'CPU-verified receipt binds the exact 5+3 implementation and selected product boundary.'),
  ev('source_positive','src/Billiards/rules/ball_in_hand_authority.h',8,115,'Core-G ACK binds replacement placement decision identity and the positive-impulse authority action.'),
  ev('source_positive','src/Billiards/game_runtime.cpp',736,825,'Runtime consumes BIH only on the exact authoritative strictly-positive applied impulse and arms BehindHeadString context before collection.'),
  ev('source_positive','src/Billiards/game_runtime.cpp',1786,1905,'Coordinate placement retains actor/scope entitlement and atomically stages one replacement offer.'),
  ev('source_positive','src/Billiards/render/modern/modern_product_session.cpp',190,218,'Shipping ACK verifier requires retained entitlement, exact scope and replacement identity.'),
  ev('source_positive','src/Billiards/render/modern/modern_product_session.cpp',575,660,'The narrow product seam refreshes authority after each exact coordinate commit and leaves global Rules UI hidden.'),
  ev('test_positive','tests/game_runtime_tests.cpp',35,100,'GameRuntime contact regressions retain authoritative applied/miscue contact behavior.'),
  ev('test_positive','tests/rules_runtime_integration_tests.cpp',2069,2650,'Runtime fixtures prove repeated coordinate commits, zero/miscue preservation, positive-only consumption, later-step failure boundary and BehindHeadString arming.'),
  ev('test_positive','tests/rules_decision_session_integration_tests.cpp',770,1010,'Product fixtures prove repeated preview/confirm refresh, retry/conflict, exact scope and hidden global UI.') ]})
 v['summary']['implemented']+=1;v['summary']['partial']-=1
 v['m4_required_unclosed']=[x for x in v['m4_required_unclosed'] if x!='WPA-II-09']
 for plan in v['next_deterministic_implementation_plan']:
  if 'WPA-II-09' in plan.get('rows',[]):plan['rows'].remove('WPA-II-09')
 v['next_deterministic_implementation_plan']=[p for p in v['next_deterministic_implementation_plan'] if p.get('rows')]
 v['wpa_ii_09_completion']={'row_id':'WPA-II-09','selected_shipping_scope':{'option_id':'COORDINATE_PLACEMENT_ONLY_POSITIVE_IMPULSE_CONSUMES','coordinate_preview_confirm_repeated':True,'strict_positive_authoritative_impulse_consumes_once':True,'zero_rejected_unsupported_miscue_failed_launch_preserve':True,'atomic_idempotent_fail_closed':True,'physical_cue_part_reposition':'DISABLED_UNSUPPORTED_SELECTED_SCOPE_DEFER','forward_stroke_classifier':'DISABLED_UNSUPPORTED_SELECTED_SCOPE_DEFER','defer_is_not_physical_implementation_claim':True},'root_selected_policy':{'path':'docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','sha256':sha(ROOT/'docs/rules/m4-wpa-policy-bundle-root-selected-v1.json')},'implementation_design_gate':{'path':'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-design-gate-v1.json','sha256':sha(ROOT/'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-design-gate-v1.json')},'implementation_receipt':{'path':'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-receipt-v1.json','sha256':sha(ROOT/'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-receipt-v1.json')},'remaining_gaps_within_selected_shipping_scope':[],'row_status':'IMPLEMENTED','product_reachable':True,'other_rows_changed':False,'global_product_exposed':False,'profile_complete':False,'m4_complete':False,'audit_generated_only':True,'production_changed':False,'cmake_changed':False,'gpu_executed':False,'readiness_or_authorization_created':False}
 paths=['src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp','tests/game_runtime_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']
 v['wpa_ii_09_completion_bindings']={'production_and_test_files':[{'path':p,'sha256':sha(ROOT/p)} for p in paths]}
 return v
def main():OUT.write_text(json.dumps(build(),indent=2,ensure_ascii=False)+'\n');print(OUT,sha(OUT));return 0
if __name__=='__main__':raise SystemExit(main())
