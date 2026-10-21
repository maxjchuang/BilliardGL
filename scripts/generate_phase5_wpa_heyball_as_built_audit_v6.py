#!/usr/bin/env python3
"""Generate the read-only WPA-II-08 completion audit successor."""
from __future__ import annotations
from collections import Counter
import hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
V5=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json'
OUT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v6.json'
def sha(p):return hashlib.sha256(Path(p).read_bytes()).hexdigest()
def ev(kind,path,start,end,finding):return {'kind':kind,'path':path,'sha256':sha(ROOT/path),'lines':{'start':start,'end':end},'finding':finding}
def build():
 v=json.loads(V5.read_text());v['schema']='phase5_wpa_heyball_as_built_reconciliation_audit_v6';v['schema_version']=6;v['audit_id']='phase5_wpa_heyball_2025_08_16_as_built_reconciliation_v6';v['audit_scope']='wpa_ii_08_complete_clause_successor';v['audit_status']='FINAL_ROOT_ACCEPTED_WPA_II_08_ONLY_SUCCESSOR';v['frozen_v5_predecessor']={'path':str(V5.relative_to(ROOT)),'sha256':sha(V5)}
 rows={r['id']:r for r in v['matrix']};r=rows['WPA-II-08'];r.update({'status':'IMPLEMENTED','product_reachable':True,'assessment':'WPA-II §8 is complete and product reachable: Core-G enforces BehindHeadString placement geometry; the narrow product seam commits only the current actor/offer; the all-behind parent shares the adjudicator legal-first-contact predicate, treats an on-line ball as directly playable, freezes unique/tied candidates, revalidates revision/hash/layout, and atomically uses the Core-E foot-spot search; the next accepted shot consumes authoritative cue-center continuous crossing evidence strictly ordered before first contact. Exact retries are zero-delta and conflicts, stale state, malformed geometry, no-solution and reset fail closed.','gaps':[],'evidence':[
  ev('policy_selected','docs/rules/m4-wpa-policy-bundle-root-selected-v1.json',81,86,'Root selected CENTER_CROSSING_EXACT_CONTINUOUS_ORDER for WPA-II-08.'),
  ev('implementation_receipt','docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-receipt-v1.json',1,260,'Authoritative continuous center-crossing telemetry, completeness, strict total order, rollback and 8+9 fixtures are CPU verified.'),
  ev('implementation_receipt','docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-receipt-v1.json',1,170,'All-behind candidate/tie/Core-E parent and narrow product subflow are CPU verified with atomic/idempotent boundaries.'),
  ev('source_positive','src/Billiards/rules/ball_in_hand_authority.h',8,190,'Core-G placement plus all-behind request/tie schemas freeze actor, revision, action and selected ball.'),
  ev('source_positive','src/Billiards/game_runtime.cpp',1620,2125,'Runtime enforces placement geometry, legal-ball sampling, exact tie, state-hash revalidation, Core-E plan rebuild and replacement child.'),
  ev('source_positive','src/Billiards/render/modern/modern_product_session.cpp',515,855,'Shipping session exposes only current-offer placement and all-behind actions; foundation/global UI stay hidden.'),
  ev('test_positive','tests/chinese_eight_ball_rules_tests.cpp',695,770,'Rules CPU tests prove strict continuous crossing order, on-line/foot-side direct contact and incomplete fail-closed behavior.'),
  ev('test_positive','tests/rules_runtime_integration_tests.cpp',2409,2840,'Runtime CPU tests cover placement, unique/tie re-spot, actor/hash/nonfinite/reset/retry/rollback and restriction lifecycle.'),
  ev('test_positive','tests/rules_decision_session_integration_tests.cpp',904,1320,'Product CPU tests cover exact BehindHeadString placement and narrow all-behind commit/retry/conflict with hidden global UI.') ]})
 v['summary']['implemented']+=1;v['summary']['partial']-=1;v['m4_required_unclosed']=[x for x in v['m4_required_unclosed'] if x!='WPA-II-08']
 for plan in v['next_deterministic_implementation_plan']:
  if 'WPA-II-08' in plan.get('rows',[]):plan['rows'].remove('WPA-II-08')
 v['next_deterministic_implementation_plan']=[p for p in v['next_deterministic_implementation_plan'] if p.get('rows')]
 v['wpa_ii_08_completion']={'row_id':'WPA-II-08','official_semantics':{'behind_head_string_placement':True,'all_behind_respot_tie_foot_search':True,'center_crossing_strictly_before_first_contact':True,'narrow_product_reachability':True,'atomic_idempotent_fail_closed':True},'root_selected_policy':{'path':'docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','sha256':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f'},'telemetry_receipt':{'path':'docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-receipt-v1.json','sha256':'86379eecd2437a6e17643d0964f4460b857615f0eddbc50ab7dbeaefe3afa878'},'all_behind_receipt':{'path':'docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-receipt-v1.json','sha256':'0dfa4e71448b26840e9688c4065ffb1180ab102d4e152c0e2003bfdf9c6e2a3f'},'remaining_gaps':[],'row_status':'IMPLEMENTED','product_reachable':True,'other_rows_changed':False,'global_product_exposed':False,'profile_complete':False,'m4_complete':False,'audit_generated_only':True,'production_changed':False,'cmake_changed':False,'gpu_executed':False,'readiness_or_authorization_created':False}
 v['wpa_ii_08_completion_bindings']={'production_and_test_files':[{'path':p,'sha256':sha(ROOT/p)} for p in ['src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/rules/chinese_eight_ball_rules.h','src/Billiards/rules/chinese_eight_ball_rules.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp','tests/chinese_eight_ball_rules_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']]}
 return v
def main():OUT.write_text(json.dumps(build(),indent=2,ensure_ascii=False)+'\n');print(OUT,sha(OUT));return 0
if __name__=='__main__':raise SystemExit(main())
