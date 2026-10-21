#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-15-2-signed-frozen-object-straight-cushion-declaration-lifecycle-implementation-receipt-v1.json'
PROD=['CMakeLists.txt','src/Billiards/rules/frozen_cushion_declaration.h','src/Billiards/rules/frozen_cushion_declaration.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/frozen_cushion_declaration_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
COMMANDS=['rules.frozen_object_cushion_propose','rules.frozen_object_cushion_final','rules.frozen_object_cushion_cancel','get_frozen_object_cushion_declaration_state']
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
 e=[];keys={'schema','status','authority_bindings','tests_first_record','implemented_contract','automation_headless','fixture_matrix','source_bindings','test_bindings','cpu_verification','completion_boundary'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='wpa_ii_15_2_signed_frozen_object_straight_cushion_declaration_lifecycle_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='PARTIAL_LIFECYCLE_CPU_VERIFIED_HEADLESS_D5_AND_AUDIT_REMAIN_OPEN':e.append('status')
 a=v.get('authority_bindings',{});a=a if isinstance(a,dict) else {}
 expected={'implementation_design_gate':'81a5fbfb2326481872c61a09f9be92045b63c342c18b119206349201117c4a5f','root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b'}
 for key,digest in expected.items():
  row=a.get(key,{})
  if not isinstance(row,dict) or row.get('sha256')!=digest:e.append('authority:'+key)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+key)
 p=a.get('root_selected_policy',{})
 if p.get('row_id')!='WPA-II-15-2' or p.get('selected_option_id')!='SIGNED_DECLARATION_STRAIGHT_CUSHION_CAPTURE_ONLY':e.append('policy')
 red=v.get('tests_first_record',{})
 if red.get('red_observed') is not True or red.get('red_exit_code')!=1 or 'no such file' not in str(red.get('red_failure')) or red.get('green_after_minimal_production_patch') is not True:e.append('red_green')
 c=v.get('implemented_contract',{})
 if c.get('states')!=['INACTIVE','PROPOSAL_OPEN','DECLARATION_ACTIVE','DECLARATION_CONSUMED','CANCELLED','INVALIDATED'] or c.get('straight_cushions')!=['HEAD','FOOT','LEFT_LONG','RIGHT_LONG']:e.append('states')
 for key in ('exact_retry_zero_delta',):
  if c.get(key) is not True:e.append('true:'+key)
 for key in ('jaw_or_pocket_boundary_accepted','geometry_or_contact_telemetry_may_declare','proposal_is_declaration','consumed_declaration_reactivates','second_shot_reuses','declaration_synthesizes_leave_recontact_cushion_pocket_or_legal_result','visible_or_global_rules_ui_exposed'):
  if c.get(key) is not False:e.append('false:'+key)
 au=v.get('automation_headless',{})
 if au.get('capability')!='wpa_ii_15_2_signed_frozen_cushion_declaration_v1' or au.get('commands')!=COMMANDS or au.get('request_exact_field_count')!=17 or au.get('readback_result_classified') is not False or au.get('readback_omits')!=['credential_id','authentication_proof','payload_canonical'] or au.get('headless_only') is not True:e.append('automation')
 fx=v.get('fixture_matrix',{});pos=fx.get('positive',[]) if isinstance(fx,dict) else [];neg=fx.get('negative',[]) if isinstance(fx,dict) else []
 if len(pos)!=10 or len(neg)!=18 or len(set(pos+neg))!=28:e.append('fixtures')
 s=v.get('source_bindings');t=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD:e.append('sources')
 if not isinstance(t,list) or [x.get('path') for x in t if isinstance(x,dict)]!=TEST:e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(t,'test')):
   if isinstance(rows,list):
    for row in rows:
     if not bind(row):e.append(label+'_drift')
 cpu=v.get('cpu_verification',{});bins=cpu.get('binary_bindings',[]) if isinstance(cpu,dict) else []
 if cpu.get('passed_count')!=10 or len(cpu.get('passed_targets',[]))!=10 or len(bins)!=10 or any(not isinstance(x,dict) or x.get('exit_code')!=0 for x in bins) or cpu.get('all_exit_code')!=0 or cpu.get('gpu_cocoa_opengl_executed') is not False:e.append('cpu')
 elif check_files:
  for row in bins:
   if not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('binary_drift')
 cb=v.get('completion_boundary',{})
 if cb.get('signed_declaration_partial_lifecycle_implemented') is not True:e.append('partial')
 for key in ('d5_leave_recontact_and_alternatives_complete','wpa_ii_15_2_audit_row_promoted','b2_complete','m4_complete','physics_telemetry_shotfacts_rules_topology_or_cue_contact_modified','visible_product_ui_implemented','global_rules_ui_exposed','gpu_executed','readiness_or_authorization_created'):
  if cb.get(key) is not False:e.append('false_claim:'+key)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_15_2_SIGNED_FROZEN_CUSHION_PARTIAL_LIFECYCLE_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
