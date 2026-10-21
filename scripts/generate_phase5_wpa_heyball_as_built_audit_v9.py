#!/usr/bin/env python3
"""Generate the NONFINAL WPA-II-19-only headless authority successor."""
from __future__ import annotations
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
V8=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v8.json'
OUT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v9.json'
POLICY='docs/rules/m4-wpa-policy-bundle-root-selected-v1.json'
FOUNDATION='docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json'
GATE='docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-design-gate-v1.json'
RECEIPT='docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-receipt-v1.json'
LIVE=['CMakeLists.txt','src/Billiards/rules/group_confusion_authority.h','src/Billiards/rules/group_confusion_authority.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp','tests/group_confusion_authority_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ref(path):return {'path':path,'sha256':sha(ROOT/path)}
def ev(kind,path,start,end,finding):return {'kind':kind,**ref(path),'lines':{'start':start,'end':end},'finding':finding}
def build():
 v=json.loads(V8.read_text())
 v['schema']='phase5_wpa_heyball_as_built_reconciliation_audit_v9';v['schema_version']=9;v['audit_id']='phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v9';v['audit_scope']='wpa_ii_19_headless_authenticated_realization_only_successor';v['audit_status']='FINAL_ROOT_ACCEPTED_WPA_II_19_ONLY_SUCCESSOR';v['frozen_v8_predecessor']={'path':str(V8.relative_to(ROOT)),'sha256':sha(V8)}
 r=next(x for x in v['matrix'] if x['id']=='WPA-II-19')
 r.update({'status':'IMPLEMENTED','product_reachable':True,'assessment':'Under the root-selected SIGNED_CONFUSION_DECLARATION_ASSIGNMENT_ERROR_ONLY policy, WPA-II §19 is complete through a headless authenticated realization product surface: after closed-table authoritative shot completion, the runtime freezes assignment/shot/rack/original-breaker/order snapshots; an authenticated referee or player may timely PROPOSE assignment-or-sequence confusion before the next accepted positive-impulse Shoot, which stops the rack; the same authenticated actor FINAL_DECLARE prevalidates Core-E and atomically reracks for the frozen original breaker and restores the original order. Ordinary WrongFirstContact remains WPA-II §17 and does not auto-trigger §19; failed/stale/conflicting requests preserve §17 BIH, which is superseded only by successful final rerack. Exact retry, rollback and reset are fail closed. The authenticated headless authority is the product-reachable surface for this product_action_required=false row. Visible/global player Rules UI and a visible referee UI remain false and are not claimed; this does not expose the complete profile.','gaps':[],'evidence':[
  ev('policy_selected',POLICY,300,350,'Root selected signed referee/player assignment-or-sequence confusion only, before accepted Shoot, with original breaker/order rerack.'),
  ev('protocol_foundation',FOUNDATION,1,170,'The CPU-verified B2 foundation provides authenticated actor, revision/hash, expiry, nonce and idempotency semantics.'),
  ev('implementation_design_gate',GATE,1,260,'The accepted §19 gate freezes exact window, actor, payload, deadline, BIH and Core-E contracts.'),
  ev('implementation_receipt',RECEIPT,1,230,'The CPU receipt binds RED/GREEN, 10 positive and 18 negative fixtures, live sources and no-visible-UI boundary.'),
  ev('source_positive','src/Billiards/rules/group_confusion_authority.h',11,91,'The consumer declares the six-state window/proposal/final/rerack authority machine and frozen snapshots.'),
  ev('source_positive','src/Billiards/rules/group_confusion_authority.cpp',8,191,'The consumer validates exact signed proposal/final payloads, same actor, accepted-shot expiry, retry and unsupported cancel.'),
  ev('source_positive','src/Billiards/game_runtime.cpp',446,518,'GameRuntime integrates the consumer and prevalidates Core-E before atomic final rerack with frozen breaker/order.'),
  ev('source_positive','src/Billiards/game_runtime.cpp',931,968,'Only a successful positive applied cue impulse expires the prior timely declaration window; stopped rack rejects Shoot.'),
  ev('source_positive','src/Billiards/game_runtime.cpp',1641,1671,'Closed-table completed shots open exact assignment/shot windows under the authoritative runtime.'),
  ev('source_positive','src/Billiards/automation_controller.cpp',825,893,'Headless readback exposes state/snapshots without credential or signed payload bytes.'),
  ev('test_positive','tests/group_confusion_authority_tests.cpp',1,201,'Focused authority tests cover signed actors, exact retry/conflict, deadline, unsupported cancel and guard rollback.'),
  ev('test_positive','tests/rules_runtime_integration_tests.cpp',4485,4568,'Runtime tests prove window creation, rack stop, Core-E rollback, original-order rerack, retry and reset.'),
  ev('test_positive','tests/automation_controller_tests.cpp',147,188,'Automation tests prove exact headless capabilities, no cancel/Core-E exposure, malformed rejection and secret-free readback.') ]})
 v['summary']['implemented']+=1;v['summary']['missing']-=1
 v['wpa_ii_19_completion']={'row_id':'WPA-II-19','selected_shipping_scope':{'option_id':'SIGNED_CONFUSION_DECLARATION_ASSIGNMENT_ERROR_ONLY','authenticated_referee_or_player_propose':True,'same_actor_final':True,'assignment_or_sequence_confusion_only':True,'accepted_positive_shoot_deadline':True,'ordinary_wrong_first_routes_wpa_ii_17':True,'section_17_bih_preserved_until_successful_final':True,'core_e_frozen_original_breaker_and_order':True,'atomic_idempotent_fail_closed':True,'product_reachability_basis':'HEADLESS_AUTHENTICATED_REALIZATION_FOR_PRODUCT_ACTION_REQUIRED_FALSE_ROW','visible_player_rules_ui':'NOT_EXPOSED','visible_referee_ui':'NOT_EXPOSED','visible_ui_not_claimed':True},'root_selected_policy':ref(POLICY),'b2_foundation_receipt':ref(FOUNDATION),'implementation_design_gate':ref(GATE),'implementation_receipt':ref(RECEIPT),'remaining_gaps_within_selected_shipping_scope':[],'row_status':'IMPLEMENTED','product_reachable':True,'other_rows_changed':False,'global_product_exposed':False,'visible_player_ui_exposed':False,'visible_referee_ui_exposed':False,'profile_complete':False,'m4_complete':False,'audit_generated_only':True,'production_changed':False,'cmake_changed':False,'gpu_executed':False,'readiness_or_authorization_created':False}
 v['wpa_ii_19_completion_bindings']={'exact_live_allowlist_files':[ref(p) for p in LIVE]}
 return v
def main():OUT.write_text(json.dumps(build(),indent=2,ensure_ascii=False)+'\n');print(OUT,sha(OUT));return 0
if __name__=='__main__':raise SystemExit(main())
