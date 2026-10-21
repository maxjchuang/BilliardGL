#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-d5-b-cue-object-frozen-cushion-policy-independent-telemetry-implementation-receipt-v1.json'
PROD=['src/Billiards/cue_contact.h','src/Billiards/physics_telemetry.h','src/Billiards/physics_telemetry.cpp','src/Billiards/physics.h','src/Billiards/physics.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/rules/frozen_cushion_declaration.h']
TEST=['tests/cue_contact_tests.cpp','tests/physics_tests.cpp','tests/physics_telemetry_tests.cpp','tests/rules_runtime_integration_tests.cpp']
FORBIDDEN=['src/Billiards/rules/chinese_eight_ball_rules.h','src/Billiards/rules/shot_fact_collector.h','src/Billiards/rules/shot_fact_collector.cpp']
POS=['CONTACT_STRICTLY_BEFORE_LAST_TIP_SEPARATION','CONTACT_STRICTLY_AFTER_LAST_TIP_SEPARATION','SIMULTANEOUS_CUE_OBJECT_CONTACTS_CANONICAL_ORDER','SIGNED_DECLARED_OBJECT_DEPARTS_NAMED_CUSHION','DEPARTURE_THEN_SAME_NAMED_CUSHION_RECONTACT','OTHER_DISTINCT_OBJECT_STRAIGHT_CUSHION_ALTERNATIVE','OBJECT_POCKET_CAPTURE_ALTERNATIVE','ALL_EVENTS_COMPLETE_ORIGIN_ATTESTED','EXACT_TICK_AND_FINAL_RETRY_SAME_DIGEST_ZERO_DUPLICATE']
NEG=['EQUAL_CONTACT_AND_TIP_SEPARATION_RECORDED_NOT_CLASSIFIED','MISSING_OR_INCOMPLETE_D5_A_STROKE_UNORDERED','FAILED_OR_GAPPED_PHYSICS_TICK_INCOMPLETE_ROLLBACK','NONFINITE_OR_OUT_OF_RANGE_TOI_REJECTED','MISSING_SOLVER_EVENT_OR_CONTACT_IDENTITY_REJECTED','GEOMETRY_ONLY_FROZEN_RELATION_CANNOT_START_LIFECYCLE','STALE_WRONG_OR_UNCONSUMED_DECLARATION_REJECTED','DECLARED_OBJECT_NEVER_LEAVES','RECONTACT_BEFORE_DEPARTURE_NOT_SATISFY_LIFECYCLE','WRONG_NAMED_CUSHION_RECONTACT_PRESERVED_AS_OTHER_EVENT_ONLY','JAW_CONTACT_NOT_STRAIGHT_CUSHION_ALTERNATIVE','CUE_BALL_POCKET_NOT_OBJECT_POCKET_ALTERNATIVE','POSITION_CORRECTION_WITHOUT_VELOCITY_IMPULSE_NOT_CUSHION_CONTACT','DUPLICATE_EVENT_DEDUPED','CONFLICTING_RETRY_ZERO_DELTA','RESET_RERACK_TURN_OR_STATE_CONFLICT_INVALIDATES','FACTS_DO_NOT_SET_DOUBLE_HIT_PUSH_FROZEN_FOUL_OR_LEGAL','NO_RULES_SHOTFACT_UI_GPU_OR_AUDIT_SIDE_EFFECT']
BIN=['BilliardsCueContactTests','BilliardsCoupledCueContactTests','BilliardsPhysicsTelemetryTests','BilliardsRulesRuntimeIntegrationTests','BilliardsGameRuntimeTests','BilliardsPhysicsTests','BilliardsPhysicsInstrumentationTests','BilliardsPhysicsScenarioTests','BilliardsAutomationControllerTests','BilliardsAutomationProtocolTests','BilliardsShotTests','BilliardsRulesTests','BilliardsRefereeAuthorityProtocolTests','BilliardsFrozenBallDeclarationTests','BilliardsFrozenCushionDeclarationTests','BilliardsDoubleHitCloseCaseDeclarationTests','BilliardsPushReviewDeclarationTests','BilliardsNonphysicalGeneralFoulAuthorityTests']
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
 e=[];keys={'schema','status','authority_bindings','tests_first','contract','fixtures','source_bindings','test_bindings','forbidden_live_bindings','build_binding','cpu','completion'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='m4_d5_b_cue_object_frozen_cushion_policy_independent_telemetry_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='POLICY_INDEPENDENT_D5_B_CPU_VERIFIED_CLASSIFICATION_AND_AUDIT_OPEN':e.append('status')
 a=v.get('authority_bindings',{});expected={'implementation_design_gate':'abdc09dae784e44b39c834b3ef93ccd20531fe9ebba6cfb628c0ebfdf8ee275f','d5_a_receipt':'7a6f7c3c798654d846d6a645451ddfa559a156a32ec869a3d4df4d5a7b2b0bd6','root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','d5_d6_feasibility':'86d4957620d9e2c9033708ef3dbd5329610cc9cc9c44a134369079cffa375f77'}
 for key,digest in expected.items():
  row=a.get(key,{}) if isinstance(a,dict) else {}
  if row.get('sha256')!=digest:e.append('authority:'+key)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+key)
 if a.get('d5_d6_feasibility',{}).get('batch')!='D5-B':e.append('batch')
 red=v.get('tests_first',{})
 if red.get('red_observed') is not True or red.get('red_exit_code')!=2 or 'CanonicalCueObjectOrderObservation' not in str(red.get('red_failure')) or red.get('green') is not True:e.append('red')
 c=v.get('contract',{})
 if c.get('schemas')!=['canonical_cue_object_tip_separation_order_v1','canonical_frozen_cushion_shot_observation_v1']:e.append('schemas')
 for key in ('shared_stroke_time_origin','strict_before_equal_after_total_order','raw_equal_not_classified','simultaneous_contacts_canonical_and_preserved','successful_contiguous_physics_ticks_only','whole_tick_toi_solver_and_contact_identity_attested','signed_committed_consumed_frozen_binding_only','continuous_declared_object_departure','same_named_recontact_requires_prior_departure','wrong_named_declared_object_contact_preserved_only','distinct_other_object_straight_cushion_alternative','object_pocket_capture_alternative','jaw_and_cue_ball_pocket_excluded','position_correction_without_velocity_impulse_excluded','exact_tick_and_final_retry_zero_duplicate','conflict_failed_gap_reset_and_rerack_fail_closed'):
  if c.get(key) is not True:e.append('true:'+key)
 for key in ('double_hit_classified','push_classified','frozen_cushion_legal_or_foul_classified','threshold_or_tolerance_selected','shotfacts_rules_automation_ui_renderer_consumer_added'):
  if c.get(key) is not False:e.append('false:'+key)
 fx=v.get('fixtures',{})
 if fx.get('positive')!=POS or fx.get('negative')!=NEG or len(set(POS+NEG))!=27:e.append('fixtures')
 for rows,paths,label in ((v.get('source_bindings'),PROD,'sources'),(v.get('test_bindings'),TEST,'tests'),(v.get('forbidden_live_bindings'),FORBIDDEN,'forbidden')):
  if not isinstance(rows,list) or [x.get('path') for x in rows if isinstance(x,dict)]!=paths:e.append(label)
  elif check_files:
   for row in rows:
    if not bind(row):e.append(label+'_drift')
 build=v.get('build_binding',{})
 if build.get('path')!='CMakeLists.txt' or build.get('sha256')!='c54bbd13cb2550fe4d4355c3b662339e0be23823363abc23ee6810f53404a801' or build.get('modified_for_d5_b') is not False:e.append('build')
 elif check_files and not bind({'path':build['path'],'sha256':build['sha256']}):e.append('cmake_drift')
 cpu=v.get('cpu',{});bins=cpu.get('binaries',[]) if isinstance(cpu,dict) else []
 if cpu.get('build_root')!='build-m4-d5-b-cpu-v1' or cpu.get('passed_count')!=18 or cpu.get('failed_count')!=0 or cpu.get('gpu') is not False or len(bins)!=18 or [Path(x.get('path','')).name for x in bins]!=BIN or any(x.get('exit_code')!=0 for x in bins):e.append('cpu')
 if check_files:
  for row in bins:
   if not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('binary_drift')
 done=v.get('completion',{})
 if done.get('d5_b_policy_independent_telemetry_implemented') is not True or done.get('unblocks_d5_c_and_d5_d_design') is not True:e.append('completion_true')
 for key in ('d5_complete','classification_or_audit_progress','wpa_rows_closed','rules_shotfacts_automation_ui_renderer_modified','gpu_readiness_or_authorization','m4_complete'):
  if done.get(key) is not False:e.append('false_claim:'+key)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_B_CUE_OBJECT_FROZEN_CUSHION_POLICY_INDEPENDENT_TELEMETRY_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
