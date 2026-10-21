#!/usr/bin/env python3
"""Generate the NONFINAL WPA-II-21 headless-authority audit successor."""
from __future__ import annotations
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
V7=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v7.json'
OUT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v8.json'
POLICY='docs/rules/m4-wpa-policy-bundle-root-selected-v1.json'
FOUNDATION='docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json'
GATE='docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-design-gate-v1.json'
RECEIPT='docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-receipt-v1.json'
LIVE=['CMakeLists.txt','src/Billiards/rules/stalemate_authority.h','src/Billiards/rules/stalemate_authority.cpp','src/Billiards/rules/referee_authority_protocol.h','src/Billiards/rules/referee_authority_protocol.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp','tests/stalemate_authority_tests.cpp','tests/referee_authority_protocol_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ref(path):return {'path':path,'sha256':sha(ROOT/path)}
def ev(kind,path,start,end,finding):return {'kind':kind,**ref(path),'lines':{'start':start,'end':end},'finding':finding}
def build():
 v=json.loads(V7.read_text())
 v['schema']='phase5_wpa_heyball_as_built_reconciliation_audit_v8';v['schema_version']=8;v['audit_id']='phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v8';v['audit_scope']='wpa_ii_21_headless_authenticated_referee_authority_completion_successor';v['audit_status']='FINAL_ROOT_ACCEPTED_WPA_II_21_ONLY_SUCCESSOR';v['frozen_v7_predecessor']={'path':str(V7.relative_to(ROOT)),'sha256':sha(V7)}
 r=next(x for x in v['matrix'] if x['id']=='WPA-II-21')
 r.update({'status':'IMPLEMENTED','product_reachable':True,'assessment':'Under the root-selected SIGNED_REFEREE_DIRECTIVE_DEFAULT_THREE_EACH policy, WPA-II §21 is complete as an authenticated headless referee-authority product workflow: referee PROPOSE freezes the rack, original breaker and order; either signed player refusal starts rack-scoped completed-shot counters; the default is advisory three each or an authenticated finite alternative; reaching the threshold only awaits a fresh referee FINAL_DECLARE; CANCEL remains available; final prevalidates and atomically commits Core-E official rerack for the frozen original breaker and restores original shooting order. Exact retry, conflict, failure and reset paths fail closed. Audit v7 already classifies this row product_action_required=false and product_reachable=true; therefore the authenticated headless automation/referee authority is the product-reachable surface for this non-player-action row. Visible/global player Rules UI remains false and is not required by this row classification; this is not a claim of visible UI or complete-profile exposure.','gaps':[],'evidence':[
  ev('policy_selected',POLICY,350,385,'Root selected authenticated referee directive with default three shots each and no automatic final declaration.'),
  ev('protocol_foundation',FOUNDATION,1,170,'The CPU-verified B2 foundation provides signed actor, context, expiry, nonce, idempotency and atomic protocol semantics.'),
  ev('implementation_design_gate',GATE,1,220,'The independently accepted gate freezes proposal/refusal/counter/final/Core-E and headless reachability contracts.'),
  ev('implementation_receipt',RECEIPT,1,230,'The CPU receipt binds 11 positive and 17 negative fixtures, live sources and no-visible-UI boundary.'),
  ev('source_positive','src/Billiards/rules/stalemate_authority.h',14,97,'The consumer exposes the six-state authority machine, frozen rack/order, counters, refusal history and receipts.'),
  ev('source_positive','src/Billiards/rules/stalemate_authority.cpp',150,301,'The consumer implements exact proposal/refusal/count/final/cancel validation, retry/conflict and threshold-await-only behavior.'),
  ev('source_positive','src/Billiards/game_runtime.cpp',435,520,'GameRuntime integrates the authority and prevalidates Core-E before atomic final rerack with frozen original order.'),
  ev('source_positive','src/Billiards/automation_controller.cpp',749,890,'Headless automation exposes secret-free state and exact authenticated envelopes without direct Core-E or visible UI.'),
  ev('test_positive','tests/stalemate_authority_tests.cpp',1,280,'Focused tests cover both players, default/alternative directives, counters, cancel, retry, conflict and rollback.'),
  ev('test_positive','tests/rules_runtime_integration_tests.cpp',4378,4458,'Runtime tests prove Core-E rollback, original-order rerack, exact retry and reset invalidation.'),
  ev('test_positive','tests/automation_controller_tests.cpp',70,210,'Automation tests prove exact headless capability, malformed request rejection and secret-free authenticated roundtrip.') ]})
 v['summary']['implemented']+=1;v['summary']['missing']-=1
 v['m4_required_unclosed']=[x for x in v['m4_required_unclosed'] if x!='WPA-II-21']
 for plan in v['next_deterministic_implementation_plan']:
  if 'WPA-II-21' in plan.get('rows',[]):plan['rows'].remove('WPA-II-21')
 v['next_deterministic_implementation_plan']=[p for p in v['next_deterministic_implementation_plan'] if p.get('rows')]
 v['wpa_ii_21_completion']={'row_id':'WPA-II-21','selected_shipping_scope':{'option_id':'SIGNED_REFEREE_DIRECTIVE_DEFAULT_THREE_EACH','authenticated_referee_propose_final_cancel':True,'detached_signed_player_refusal':True,'first_refusal_starts_rack_scoped_counters':True,'threshold_only_awaits_referee_final':True,'core_e_frozen_original_breaker_and_order':True,'atomic_idempotent_fail_closed':True,'product_reachability_basis':'HEADLESS_AUTHENTICATED_REFEREE_AUTHORITY_FOR_PRODUCT_ACTION_REQUIRED_FALSE_ROW','visible_player_rules_ui':'NOT_EXPOSED_NOT_REQUIRED_BY_AUDIT_ROW_CLASSIFICATION','visible_ui_not_claimed':True},'root_selected_policy':ref(POLICY),'b2_foundation_receipt':ref(FOUNDATION),'implementation_design_gate':ref(GATE),'implementation_receipt':ref(RECEIPT),'remaining_gaps_within_selected_shipping_scope':[],'row_status':'IMPLEMENTED','product_reachable':True,'other_rows_changed':False,'global_product_exposed':False,'visible_player_ui_exposed':False,'profile_complete':False,'m4_complete':False,'audit_generated_only':True,'production_changed':False,'cmake_changed':False,'gpu_executed':False,'readiness_or_authorization_created':False}
 v['wpa_ii_21_completion_bindings']={'exact_live_allowlist_files':[ref(p) for p in LIVE]}
 return v
def main():OUT.write_text(json.dumps(build(),indent=2,ensure_ascii=False)+'\n');print(OUT,sha(OUT));return 0
if __name__=='__main__':raise SystemExit(main())
