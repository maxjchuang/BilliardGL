#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json,math
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-d5-c-machine-double-hit-classification-policy-candidate-gate-v1.json'
IDS=['D5C-A_REFEREE_CLOSE_CASES_EXACT_ORDER','D5C-B_CONSERVATIVE_MACHINE_CLOSE_GRAZE','D5C-C_BALANCED_MACHINE_CLOSE_GRAZE']
LIVE=['CMakeLists.txt','src/Billiards/physics_telemetry.h','src/Billiards/physics_telemetry.cpp','src/Billiards/rules/chinese_eight_ball_rules.h','src/Billiards/rules/chinese_eight_ball_rules.cpp','src/Billiards/rules/shot_fact_collector.h','src/Billiards/rules/shot_fact_collector.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp']
def sha(b):return hashlib.sha256(b).hexdigest()
def resolve(s):
 if not isinstance(s,str):return None
 p=Path(s)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def bind(row):
 if not isinstance(row,dict) or set(row)!={'path','sha256'}:return False
 p=resolve(row.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==row.get('sha256')
def validate(v,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[];keys={'schema','status','official_source','authority_bindings','current_capability','shared_machine_contract','bounded_policy_space','recommended_bundle_id','recommendation_is_not_selection','policy_bundles','mutual_exclusion','future_implementation_boundary','live_no_change_bindings','claims'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='m4_d5_c_machine_double_hit_classification_policy_candidate_gate_v1':e.append('schema')
 if v.get('status')!='FINAL_DESIGN_ONLY_ROOT_POLICY_SELECTION_REQUIRED_NO_PRODUCTION_AUTHORITY':e.append('status')
 official=v.get('official_source',{})
 if official.get('path')!='tmp/pdfs/250816-Rules-of-Heyball.pdf' or official.get('sha256')!='2b92dd5088ea630337326e26b7a2a7a4499a9801f2c5da7e5cc40421d5424a24' or official.get('pdf_page')!=9 or official.get('clauses')!=['12.5','12.6'] or official.get('visually_reviewed') is not True or len(official.get('not_numerically_determined',[]))!=3:e.append('official')
 elif check_files and not bind({'path':official['path'],'sha256':official['sha256']}):e.append('official_drift')
 a=v.get('authority_bindings',{});expected={'d5_a_receipt':'7a6f7c3c798654d846d6a645451ddfa559a156a32ec869a3d4df4d5a7b2b0bd6','d5_b_receipt':'5d690f3bfbdbee149cae833eec0221e0543216dd963f6e0a634cc3b5b743f8fe','d5_d6_feasibility':'86d4957620d9e2c9033708ef3dbd5329610cc9cc9c44a134369079cffa375f77','root_selected_policy_bundle':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','policy_register':'66e105e968fc49f46cfe7fd117e42b545c6789a87d401f1d657ee263103a31ce','cue_contact_gap_gate':'1442e4204c4acb0718ca0b87c858c8222e03ac634f5b5b7def3d9a6bdae64033','signed_close_case_receipt':'eb0ac7392ca9bfb9f84c768c22cc90bf120da32d8edb8a7d32674b2383fa51f1','audit_v9':'eefd6121b7df2b41e45b66f3c87e2732bbccdb3808d866a89d44e8a324b9cf01'}
 for key,digest in expected.items():
  row=a.get(key,{}) if isinstance(a,dict) else {}
  if row.get('sha256')!=digest:e.append('authority:'+key)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+key)
 if a.get('d5_d6_feasibility',{}).get('batch')!='D5-C' or a.get('root_selected_policy_bundle',{}).get('does_not_select_d5_c_numeric_inputs') is not True:e.append('selection_boundary')
 cap=v.get('current_capability',{})
 for key in ('complete_canonical_tip_intervals','cue_object_contact_vs_tip_separation_raw_total_order','raw_equal_preserved_without_classification','signed_close_case_review_lifecycle'):
  if cap.get(key) is not True:e.append('cap_true:'+key)
 for key in ('accepted_launch_initial_surface_gap_in_d5_b_observation','calibrated_light_graze_metric','machine_double_hit_classifier','shotfacts_or_rules_consumer'):
  if cap.get(key) is not False:e.append('cap_false:'+key)
 contract=v.get('shared_machine_contract',{})
 if contract.get('automatic_unambiguous_predicate')!='complete canonical cue-tip interval count > 1 is DOUBLE_HIT_FOUL' or contract.get('incomplete_evidence')!='INCOMPLETE_NO_CLASSIFICATION' or 'REVIEW_REQUIRED' not in contract.get('outputs',[]) or len(contract.get('outputs',[]))!=6:e.append('contract')
 space=v.get('bounded_policy_space',{})
 if space.get('very_close_surface_gap_cm')!={'minimum':0.0,'maximum':0.5,'null_means':'machine close-case branch disabled'} or space.get('light_graze_normal_impulse_ratio')!={'minimum_exclusive':0.0,'maximum':0.25,'null_means':'machine light-graze exception disabled'} or space.get('tip_separation_tie_tolerance_seconds')!={'minimum':0.0,'maximum':0.00001} or space.get('values_are_product_policy_not_official_constants') is not True:e.append('bounds')
 if v.get('recommended_bundle_id')!=IDS[0] or v.get('recommendation_is_not_selection') is not True:e.append('recommendation')
 bundles=v.get('policy_bundles',[])
 if not isinstance(bundles,list) or len(bundles)!=3 or [x.get('order') for x in bundles]!=[1,2,3] or [x.get('id') for x in bundles]!=IDS or len({x.get('id') for x in bundles})!=3:e.append('bundles')
 else:
  expected=[(None,None,0.0),(0.05,0.05,0.0000001),(0.15,0.1,0.000001)]
  for i,(bundle,constants) in enumerate(zip(bundles,expected)):
   c=bundle.get('constants',{})
   actual=(c.get('very_close_surface_gap_cm'),c.get('light_graze_normal_impulse_ratio'),c.get('tip_separation_tie_tolerance_seconds'))
   if bundle.get('selection_status')!='UNSELECTED' or bundle.get('recommended') is not (i==0) or actual!=constants:e.append('bundle:'+str(i))
 mutual=v.get('mutual_exclusion',{})
 if any(mutual.get(k) is not True for k in ('exactly_one_bundle_may_be_root_selected_later','bundle_constants_may_not_be_mixed','selection_must_create_a_new_versioned_root_selected_artifact','this_gate_selects_none')):e.append('mutual')
 future=v.get('future_implementation_boundary',{})
 if future.get('dormant_until_root_selection') is not True or future.get('tests_first_required') is not True or future.get('bundle_b_or_c_additional_design_gate_required') is not True or len(future.get('candidate_production_allowlist',[]))!=8 or len(future.get('candidate_test_allowlist',[]))!=4 or len(future.get('forbidden',[]))!=6:e.append('future')
 live=v.get('live_no_change_bindings')
 if not isinstance(live,list) or [x.get('path') for x in live if isinstance(x,dict)]!=LIVE:e.append('live')
 elif check_files:
  for row in live:
   if not bind(row):e.append('live_drift')
 claims=v.get('claims',{})
 for key in ('policy_selected','production_or_cmake_modified','d5_c_implementation_authorized','machine_double_hit_classification_implemented','wpa_ii_12_5_6_or_15_1_promoted','audit_or_m4_progress','gpu_readiness_or_authorization'):
  if claims.get(key) is not False:e.append('false_claim:'+key)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('gate',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.gate).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_C_MACHINE_DOUBLE_HIT_CLASSIFICATION_POLICY_CANDIDATE_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
