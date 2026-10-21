#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-d5-c-machine-double-hit-classification-implementation-design-gate-v1.json'
PROD=['src/Billiards/rules/double_hit_classifier.h','src/Billiards/rules/double_hit_classifier.cpp','src/Billiards/rules/chinese_eight_ball_rules.h','src/Billiards/rules/chinese_eight_ball_rules.cpp','src/Billiards/rules/shot_fact_collector.h','src/Billiards/rules/shot_fact_collector.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp']
TEST=['tests/double_hit_classifier_tests.cpp','tests/shot_fact_collector_tests.cpp','tests/chinese_eight_ball_rules_tests.cpp','tests/rules_runtime_integration_tests.cpp']
POS=['TWO_COMPLETE_CANONICAL_INTERVALS_AUTOMATIC_DOUBLE_HIT_FOUL','THREE_COMPLETE_CANONICAL_INTERVALS_SAME_RESULT','AUTOMATIC_DOUBLE_HIT_ADDS_EXACT_FOUL_REASON','AUTOMATIC_DOUBLE_HIT_GRANTS_OPPONENT_BALL_IN_HAND','SINGLE_COMPLETE_INTERVAL_UNSIGNED_NO_DOUBLE_HIT','ZERO_INTERVAL_COMPLETE_NO_DOUBLE_HIT','SIGNED_CLOSE_CASE_SINGLE_INTERVAL_RAW_BEFORE_REVIEW_ONLY','RAW_EQUAL_REVIEW_ONLY','EXACT_CLASSIFIER_AND_ADJUDICATION_RETRY_ZERO_DELTA','IMMUTABLE_RESULT_BINDS_SOURCE_AND_POLICY_DIGESTS']
NEG=['INCOMPLETE_D5_A_NO_CLASSIFICATION','INCOMPLETE_D5_B_NO_CLASSIFICATION','MISMATCHED_SHOT_IDENTITY_REJECTED','MISMATCHED_ACTOR_RACK_REVISION_OR_STATE_HASH_REJECTED','MISSING_OR_CHANGED_SOURCE_DIGEST_REJECTED','FAILED_OR_GAPPED_PHYSICS_NEVER_FOUL','ONE_INTERVAL_NEVER_AUTOMATIC_FOUL','RAW_BEFORE_WITHOUT_SIGNED_CLOSE_CASE_NEVER_AUTOMATIC_FOUL','RAW_EQUAL_NEVER_REWRITTEN_AS_BEFORE','RAW_AFTER_NEVER_AUTOMATIC_FOUL','SIGNED_REVIEW_NEEDED_NEVER_FOUL','SIGNED_LIGHT_GRAZE_NEVER_MACHINE_NO_FOUL_RESULT_WITHOUT_REFEREE_FINAL','NULL_GAP_PREVENTS_MACHINE_VERY_CLOSE_BRANCH','NULL_GRAZE_PREVENTS_MACHINE_LIGHT_GRAZE_BRANCH','NO_NUMERIC_TOLERANCE_APPLIED_TO_RAW_ORDER','FROZEN_DECLARATION_DOES_NOT_OVERRIDE_MULTI_INTERVAL_FOUL','PUSH_CLASSIFICATION_REMAINS_ABSENT','STALE_WRONG_OR_UNCONSUMED_CLOSE_CASE_IGNORED','CONFLICTING_RETRY_ZERO_DELTA','FAILED_ADJUDICATION_ROLLS_BACK','RESET_RERACK_TURN_OR_NEXT_SHOT_INVALIDATES','NO_VISIBLE_GLOBAL_UI_GPU_OR_AUDIT_SIDE_EFFECT']
def sha(b):return hashlib.sha256(b).hexdigest()
def bind(r):
 if not isinstance(r,dict) or set(r)!={'path','sha256'}:return False
 p=Path(r['path'])
 if p.is_absolute() or '..' in p.parts:return False
 q=(ROOT/p).resolve();return q.is_relative_to(ROOT.resolve()) and q.is_file() and sha(q.read_bytes())==r['sha256']
def validate(v,files=True):
 if not isinstance(v,dict):return ['root']
 e=[];keys={'schema','status','authority_bindings','selected_policy_constants','classifier_contract','rules_consumption_contract','atomicity_and_idempotency','required_fixtures','implementation_allowlist','live_source_bindings','completion_boundary'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='m4_d5_c_machine_double_hit_classification_implementation_design_gate_v1' or v.get('status')!='FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_CHANGE':e.append('identity')
 exp={'root_selected_d5_c_policy':'c3529ff92ccb1540513f9be3fbbf725832dfd5c81ce78394d53a9b8b8c994ef6','candidate_gate':'cf89e3491e7eac9d41704c73784e625125292bf28526c7044c303be6fd172be9','d5_a_receipt':'7a6f7c3c798654d846d6a645451ddfa559a156a32ec869a3d4df4d5a7b2b0bd6','d5_b_receipt':'5d690f3bfbdbee149cae833eec0221e0543216dd963f6e0a634cc3b5b743f8fe','signed_close_case_receipt':'eb0ac7392ca9bfb9f84c768c22cc90bf120da32d8edb8a7d32674b2383fa51f1'}
 a=v.get('authority_bindings',{})
 for k,d in exp.items():
  r=a.get(k,{})
  if r.get('sha256')!=d:e.append('authority:'+k)
  elif files and not bind({'path':r.get('path'),'sha256':r.get('sha256')}):e.append('drift:'+k)
 if a.get('root_selected_d5_c_policy',{}).get('selected_bundle')!='D5C-A_REFEREE_CLOSE_CASES_EXACT_ORDER':e.append('selected')
 c=v.get('selected_policy_constants',{})
 if c!={'very_close_surface_gap_cm':None,'light_graze_normal_impulse_ratio':None,'tip_separation_tie_tolerance_seconds':0.0,'raw_exact_order':True,'machine_close_or_graze_threshold_branch':False}:e.append('constants')
 x=v.get('classifier_contract',{})
 if x.get('schema_id')!='canonical_double_hit_classification_v1' or len(x.get('inputs',[]))!=3 or len(x.get('required_identity_match',[]))!=5 or x.get('automatic_foul_result')!='DOUBLE_HIT_FOUL' or x.get('raw_equal')!='SIGNED_REFEREE_REVIEW_REQUIRED' or x.get('invalid_incomplete_mismatch_or_conflict')!='INCOMPLETE_NO_CLASSIFICATION' or x.get('interval_count_precedes_close_case_branch') is not True or x.get('signed_review_never_overrides_proven_multiple_intervals') is not True or len(x.get('result_fields',[]))!=11:e.append('classifier')
 rules=v.get('rules_consumption_contract',{})
 for k in ('classification_occurs_after_d5_a_and_d5_b_finish','shotfacts_may_carry_only_complete_immutable_classifier_result','double_hit_foul_grants_existing_general_foul_ball_in_hand','review_required_does_not_mutate_match_state_or_adjudication','no_classification_does_not_mutate_match_state_or_adjudication','adjudicator_may_not_recompute_interval_count_or_order','signed_close_case_review_state_remains_headless_and_separate','foul_must_be_bound_to_current_completed_shot_before_next_shot'):
  if rules.get(k) is not True:e.append('rules:'+k)
 if rules.get('new_foul_reason')!='DoubleHit':e.append('reason')
 atomic=v.get('atomicity_and_idempotency',{})
 if len(atomic)!=7 or any(y is not True for y in atomic.values()):e.append('atomic')
 f=v.get('required_fixtures',{})
 if f.get('positive')!=POS or f.get('negative')!=NEG or len(set(POS+NEG))!=32:e.append('fixtures')
 allow=v.get('implementation_allowlist',{})
 if allow.get('production_files')!=PROD or allow.get('test_files')!=TEST or allow.get('build_files')!=['CMakeLists.txt'] or len(allow.get('explicitly_forbidden_files',[]))!=8:e.append('allowlist')
 live=v.get('live_source_bindings',[])
 if not isinstance(live,list) or len(live)!=15:e.append('live')
 elif files:
  for r in live:
   if not bind(r):e.append('live_drift')
 done=v.get('completion_boundary',{})
 if done.get('root_revalidation_required') is not True:e.append('revalidation')
 for k in ('implementation_authorized_by_this_gate','d5_c_implemented','d5_complete','wpa_ii_12_5_6_or_15_1_promoted','push_or_frozen_exception_implemented','visible_or_global_ui_modified','gpu_readiness_or_authorization','m4_complete'):
  if done.get(k) is not False:e.append('false:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('gate',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');x=ap.parse_args()
 try:v=json.loads(Path(x.gate).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_C_MACHINE_DOUBLE_HIT_CLASSIFICATION_IMPLEMENTATION_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
