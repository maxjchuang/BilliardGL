#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-d5-a-stroke-observation-canonical-tip-interval-foundation-implementation-receipt-v1.json'
PROD=['src/Billiards/cue_contact.h','src/Billiards/cue_contact.cpp','src/Billiards/coupled_cue_contact.h','src/Billiards/coupled_cue_contact.cpp','src/Billiards/physics_telemetry.h','src/Billiards/physics_telemetry.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp']
TEST=['tests/cue_contact_tests.cpp','tests/coupled_cue_contact_tests.cpp','tests/physics_telemetry_tests.cpp','tests/rules_runtime_integration_tests.cpp']
POS=['ACCEPTED_STROKE_STARTS_BEFORE_FIRST_CONTACT','ONE_COMPLETE_ANALYTIC_INTERVAL','ONE_COMPLETE_COUPLED_INTERVAL','SEPARATE_RECONTACT_CREATES_SECOND_ORDERED_INTERVAL','CONTIGUOUS_SAME_CONTACT_SAMPLES_MERGE','COMPLETE_NO_CONTACT_STROKE_EMPTY_INTERVALS','AUTHORITATIVE_END_AFTER_RELEASE','EXACT_RETRY_SAME_DIGEST_ZERO_DUPLICATES']
NEG=['REJECTED_ZERO_UNSUPPORTED_MISCUE_LAUNCH_NO_OBSERVATION','START_AFTER_CONTACT_INCOMPLETE','END_BEFORE_RELEASE_INCOMPLETE','DROPPED_INTERVAL_INCOMPLETE','OVERLAPPING_OR_NONCANONICAL_INTERVAL_REJECTED','NONFINITE_TIME_POINT_NORMAL_VELOCITY_OR_IMPULSE_REJECTED','MIXED_OR_UNKNOWN_ORIGIN_REJECTED','MICROTRACE_ONLY_NOT_FULL_STROKE_COMPLETE','FAILED_SOLVER_STEP_ROLLS_BACK_UNCOMMITTED_EDGES','RESET_RERACK_TURN_OR_STATE_CONFLICT_INVALIDATES','DUPLICATE_INTERVAL_DEDUPED','CONFLICTING_RETRY_ZERO_DELTA','INTERVAL_COUNT_DOES_NOT_SET_DOUBLE_HIT_PUSH_OR_FOUL','NO_SHOTFACT_RULE_UI_OR_GPU_SIDE_EFFECT']
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
 e=[];keys={'schema','status','authority_bindings','tests_first','contract','fixtures','source_bindings','test_bindings','build_binding','cpu','completion'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='m4_d5_a_stroke_observation_canonical_tip_interval_foundation_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='POLICY_INDEPENDENT_FOUNDATION_CPU_VERIFIED_D5_CLASSIFICATION_AUDIT_OPEN':e.append('status')
 a=v.get('authority_bindings',{});expected={'implementation_design_gate':'542e0bfd05759dce5497938f96d657964474ac30f96bbfbde2b9efdc0bdc3c01','root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','d5_d6_feasibility':'86d4957620d9e2c9033708ef3dbd5329610cc9cc9c44a134369079cffa375f77'}
 for key,digest in expected.items():
  row=a.get(key,{}) if isinstance(a,dict) else {}
  if row.get('sha256')!=digest:e.append('authority:'+key)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+key)
 if a.get('d5_d6_feasibility',{}).get('batch')!='D5-A':e.append('batch')
 red=v.get('tests_first',{})
 if red.get('red_observed') is not True or red.get('red_exit_code')!=1 or 'CanonicalCueTipStrokeObservation' not in str(red.get('red_failure')) or red.get('green') is not True:e.append('red')
 c=v.get('contract',{})
 for key in ('accepted_positive_launch_stages_before_first_contact','authoritative_motion_end_required','successful_physics_tick_required_before_runtime_publication','analytic_and_coupled_origins_recorded','ordered_half_open_intervals','contiguous_same_origin_samples_merge','separated_recontact_creates_interval','empty_complete_observation_allowed','origin_and_completeness_attested','immutable_digest_and_exact_retry','failed_launch_step_reset_rerack_rollback'):
  if c.get(key) is not True:e.append('true:'+key)
 for key in ('double_hit_classified','push_classified','foul_classified','threshold_selected','shotfacts_rules_ui_consumer_added'):
  if c.get(key) is not False:e.append('false:'+key)
 fx=v.get('fixtures',{})
 if fx.get('positive')!=POS or fx.get('negative')!=NEG or len(set(POS+NEG))!=22:e.append('fixtures')
 s=v.get('source_bindings');t=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD:e.append('sources')
 if not isinstance(t,list) or [x.get('path') for x in t if isinstance(x,dict)]!=TEST:e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(t,'test')):
   if isinstance(rows,list):
    for row in rows:
     if not bind(row):e.append(label+'_drift')
 build=v.get('build_binding',{})
 if build.get('path')!='CMakeLists.txt' or build.get('sha256')!='c54bbd13cb2550fe4d4355c3b662339e0be23823363abc23ee6810f53404a801' or build.get('modified_for_d5_a') is not False:e.append('build')
 elif check_files and not bind({'path':build['path'],'sha256':build['sha256']}):e.append('cmake_drift')
 cpu=v.get('cpu',{});bins=cpu.get('binaries',[]) if isinstance(cpu,dict) else []
 block=cpu.get('link_blocked_external',{}) if isinstance(cpu,dict) else {}
 if cpu.get('build_root')!='build-wpa-ii-17-subset-green-v1' or cpu.get('passed_count')!=17 or cpu.get('failed_count')!=0 or cpu.get('link_blocked_external_count')!=1 or len(bins)!=17 or any(not isinstance(x,dict) or x.get('exit_code')!=0 for x in bins) or cpu.get('gpu') is not False:e.append('cpu')
 if block.get('target')!='BilliardsShotFactCollectorTests' or 'outside D5-A exact allowlist' not in str(block.get('reason')):e.append('external_block')
 if check_files:
  for row in bins:
   if not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('binary_drift')
 done=v.get('completion',{})
 if done.get('d5_a_foundation_implemented') is not True or done.get('unblocks_d5_b_design') is not True:e.append('completion_true')
 for key in ('d5_complete','classification_or_audit_progress','wpa_rows_closed','rules_shotfacts_ui_modified','gpu_readiness_or_authorization','m4_complete'):
  if done.get(key) is not False:e.append('false_claim:'+key)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_A_STROKE_OBSERVATION_CANONICAL_TIP_INTERVAL_FOUNDATION_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
