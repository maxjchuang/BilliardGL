#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1];DEFAULT=ROOT/'docs/rules/m4-d5-a-stroke-observation-canonical-tip-interval-foundation-implementation-design-gate-v1.json'
PROD=['src/Billiards/cue_contact.h','src/Billiards/cue_contact.cpp','src/Billiards/coupled_cue_contact.h','src/Billiards/coupled_cue_contact.cpp','src/Billiards/physics_telemetry.h','src/Billiards/physics_telemetry.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp'];TEST=['tests/cue_contact_tests.cpp','tests/coupled_cue_contact_tests.cpp','tests/physics_telemetry_tests.cpp','tests/rules_runtime_integration_tests.cpp'];LIVE=['CMakeLists.txt']+PROD
def sha(b):return hashlib.sha256(b).hexdigest()
def bind(r):
 p=Path(r.get('path',''))
 if p.is_absolute() or '..' in p.parts:return False
 q=ROOT/p;return q.is_file() and sha(q.read_bytes())==r.get('sha256')
def validate(v,files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if v.get('schema')!='m4_d5_a_stroke_observation_canonical_tip_interval_foundation_implementation_design_gate_v1' or v.get('status')!='FINAL_DESIGN_ONLY_ROOT_REVALIDATION_REQUIRED_NO_PRODUCTION_CHANGE':e.append('header')
 a=v.get('authority_bindings',{});ex={'root_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','d5_d6_feasibility':'86d4957620d9e2c9033708ef3dbd5329610cc9cc9c44a134369079cffa375f77','cue_contact_gap_gate':'1442e4204c4acb0718ca0b87c858c8222e03ac634f5b5b7def3d9a6bdae64033','b2_successor':'663a2a28faf93bd98de89e60498af1c955c55c36033849a0da12af965a478e69'}
 for k,d in ex.items():
  r=a.get(k,{})
  if r.get('sha256')!=d:e.append('authority:'+k)
  elif files and not bind(r):e.append('drift:'+k)
 c=v.get('foundation_contract',{})
 if c.get('schema_id')!='canonical_cue_tip_stroke_observation_v1' or c.get('policy_independent_measurement_only') is not True or len(c.get('stroke_identity',[]))!=7 or len(c.get('origin_values',[]))!=3 or len(c.get('interval_fields',[]))!=15 or len(c.get('completeness_fields',[]))!=7:e.append('contract')
 g=v.get('current_gap_and_minimum_extension',{})
 if g.get('cue_tip_after_release_observed') is not False or g.get('full_stroke_second_contact_observable') is not False or g.get('existing_microsteps_may_seed_interval_samples_but_do_not_prove_full_stroke') is not True or len(g.get('minimum_extension',[]))!=5:e.append('gap')
 atom=v.get('atomicity_dedupe_and_rollback',{})
 if len(atom.get('dedupe_key',[]))!=5 or 'zero telemetry/runtime delta' not in str(atom.get('conflicting_retry')):e.append('atomic')
 b=v.get('classification_boundary',{})
 for k in ('double_hit_classified','push_classified','foul_classified','close_case_policy_selected','duration_or_distance_threshold_selected','frozen_rule_classified','shotfacts_or_rules_consumer_added'):
  if b.get(k) is not False:e.append('classification:'+k)
 if b.get('interval_count_is_not_a_foul_until_separate_consumer_gate') is not True:e.append('interval_boundary')
 f=v.get('required_fixtures',{})
 if len(f.get('positive',[]))!=8 or len(f.get('negative',[]))!=14 or len(set(f.get('positive',[])+f.get('negative',[])))!=22:e.append('fixtures')
 al=v.get('implementation_allowlist',{})
 if al.get('production_files')!=PROD or al.get('test_files')!=TEST or al.get('build_files')!=['CMakeLists.txt'] or al.get('rules_shotfacts_ui_renderer_edit_allowed') is not False:e.append('allowlist')
 live=v.get('live_source_bindings',[])
 if [r.get('path') for r in live]!=LIVE:e.append('live')
 elif files:
  for r in live:
   if not bind(r):e.append('live_drift')
 x=v.get('completion_boundary',{})
 if x.get('root_revalidation_required') is not True:e.append('root_gate')
 for k in ('implementation_authorized_by_this_gate','d5_a_implemented','d5_complete','classification_or_audit_progress','production_or_cmake_modified','rules_shotfacts_ui_modified','gpu_readiness_or_authorization','m4_complete'):
  if x.get(k) is not False:e.append('claim:'+k)
 return e
def main():
 p=argparse.ArgumentParser();p.add_argument('path',nargs='?',default=str(DEFAULT));p.add_argument('--no-file-checks',action='store_true');x=p.parse_args()
 try:v=json.loads(Path(x.path).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_A_STROKE_OBSERVATION_FOUNDATION_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
