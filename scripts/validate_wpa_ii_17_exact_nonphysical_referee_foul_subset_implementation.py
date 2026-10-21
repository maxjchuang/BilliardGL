#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-17-exact-nonphysical-referee-foul-subset-implementation-receipt-v1.json'
PROD=['CMakeLists.txt','src/Billiards/rules/nonphysical_general_foul_authority.h','src/Billiards/rules/nonphysical_general_foul_authority.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/nonphysical_general_foul_authority_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
KINDS=['BOTH_FEET_OFF_GROUND_DURING_SHOT','ILLEGAL_BODY_CLOTHING_OR_EQUIPMENT_BALL_TOUCH','ONE_HAND_SHOT','AUXILIARY_EQUIPMENT_PLACED_ON_TABLE_TO_SHOOT','MOBILE_DEVICE_DISTURBANCE_GENERAL_FOUL']
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
 e=[];keys={'schema','status','authority_bindings','tests_first','exact_contract','automation','fixtures','source_bindings','test_bindings','cpu','completion'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='wpa_ii_17_exact_nonphysical_referee_foul_subset_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='PARTIAL_EXACT_NONPHYSICAL_SUBSET_CPU_VERIFIED_D5_D6_FULL_ROW_OPEN':e.append('status')
 a=v.get('authority_bindings',{});expected={'implementation_design_gate':'69b04abc73dddc2d9b3b84022ab689842c0028f181dd3bb9b3258792ce0503da','root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b','cue_scratch_receipt':'38d4c067d0f3c77603ef5149ea5160e859f8fc8d02c52d04a116761b8106aef5'}
 for k,d in expected.items():
  row=a.get(k,{}) if isinstance(a,dict) else {}
  if row.get('sha256')!=d:e.append('authority:'+k)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+k)
 if a.get('cue_scratch_receipt',{}).get('modified') is not False:e.append('scratch')
 red=v.get('tests_first',{})
 if red.get('red_observed') is not True or red.get('red_exit_code')!=1 or 'no such file' not in str(red.get('red_failure')) or red.get('green') is not True:e.append('red')
 c=v.get('exact_contract',{})
 if c.get('event_kinds')!=KINDS:e.append('kinds')
 for k in ('final_atomic_ordinary_foul_anywhere_bih','core_g_offer_staged','exact_retry_zero_delta'):
  if c.get(k) is not True:e.append('true:'+k)
 for k in ('proposal_is_foul','mobile_repeat_loss_of_rack','physics_or_shotfacts_synthesized'):
  if c.get(k) is not False:e.append('false:'+k)
 au=v.get('automation',{});commands=['rules.nonphysical_general_foul_propose','rules.nonphysical_general_foul_final','rules.nonphysical_general_foul_cancel','get_nonphysical_general_foul_state']
 if au.get('capability')!='wpa_ii_17_exact_nonphysical_referee_foul_subset_v1' or au.get('commands')!=commands or au.get('headless') is not True or au.get('secret_free') is not True or au.get('full_row_complete_readback') is not False:e.append('automation')
 fx=v.get('fixtures',{});pos=fx.get('positive',[]) if isinstance(fx,dict) else [];neg=fx.get('negative',[]) if isinstance(fx,dict) else []
 if len(pos)!=10 or len(neg)!=20 or len(set(pos+neg))!=30:e.append('fixtures')
 s=v.get('source_bindings');t=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD:e.append('sources')
 if not isinstance(t,list) or [x.get('path') for x in t if isinstance(x,dict)]!=TEST:e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(t,'test')):
   if isinstance(rows,list):
    for row in rows:
     if not bind(row):e.append(label+'_drift')
 cpu=v.get('cpu',{});bins=cpu.get('binaries',[]) if isinstance(cpu,dict) else []
 if cpu.get('build_root')!='build-wpa-ii-17-subset-green-v1' or cpu.get('passed_count')!=13 or len(bins)!=13 or any(not isinstance(x,dict) or x.get('exit_code')!=0 for x in bins) or cpu.get('gpu') is not False:e.append('cpu')
 elif check_files:
  for row in bins:
   if not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('binary_drift')
 cb=v.get('completion',{})
 if cb.get('exact_nonphysical_subset_implemented') is not True:e.append('partial')
 for k in ('d5_complete','d6_complete','off_table_policy_resolved','full_wpa_ii_17_implemented','audit_promoted','m4_complete','forbidden_sources_modified','mobile_repeat_loss_of_rack_implemented','visible_or_global_ui','gpu_readiness_or_authorization'):
  if cb.get(k) is not False:e.append('false_claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_17_EXACT_NONPHYSICAL_REFEREE_FOUL_SUBSET_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
