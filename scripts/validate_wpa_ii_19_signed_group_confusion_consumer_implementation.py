#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-receipt-v1.json'
KEYS={'schema','status','authority_bindings','tests_first_record','implemented_contract','automation_headless','fixture_matrix','source_bindings','test_bindings','cpu_verification','completion_boundary'}
PROD=['CMakeLists.txt','src/Billiards/rules/group_confusion_authority.h','src/Billiards/rules/group_confusion_authority.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/group_confusion_authority_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
COMMANDS=['rules.group_confusion_propose','rules.group_confusion_final','get_group_confusion_authority_state']
DIGESTS={'tests_first_record':'741a88ca2ab8b602d34ced3b65385a50da6a08c84d207d0a178b7b979ba3691a','implemented_contract':'6b6d8e1157735fe4cb764d195b76d2819e5ef360672a988e87ed1a48721a2828','automation_headless':'5ef1d690f6ba83c86bbc749d9cb4529c4b4b72e8b9c2ef36b3bead72d4b0847e','fixture_matrix':'88c8e2c2a6f2e982899892afe29134971168d4c1874cd21d975571dfdc38ae1d','source_bindings':'b63096f38b8eab328224acc316d27f536da79ba1bf6d809705ecadb433e32afa','test_bindings':'009143b9c536585ff414b7a4123b98c09343451de0a538eb536917e217cbb7a4','cpu_verification':'afc684794dd155d6887b9ea687e2980a6d9c4da03a2231be6efe63387b9e275a','completion_boundary':'9766fcba965fc6cae6fd6868fbaa6dd4b0c30b252dcda8aa7afcca41a8fbbb80'}
def sha(b):return hashlib.sha256(b).hexdigest()
def dig(v):return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def resolve(s):
 if not isinstance(s,str):return None
 p=Path(s)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def bind(row):
 if not isinstance(row,dict) or set(row)!={'path','sha256'}:return False
 p=resolve(row.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==row.get('sha256')
def validate(v:Any,check_files=True):
 if not isinstance(v,dict):return ['root']
 e=[]
 if set(v)!=KEYS:e.append('keys')
 if v.get('schema')!='wpa_ii_19_signed_group_confusion_consumer_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='IMPLEMENTED_CPU_VERIFIED_HEADLESS_AUTHORITY_NO_VISIBLE_UI_NO_AUTO_M4_COMPLETION':e.append('status')
 for k,h in DIGESTS.items():
  if dig(v.get(k))!=h:e.append('digest:'+k)
 a=v.get('authority_bindings',{});a=a if isinstance(a,dict) else {};g=a.get('implementation_design_gate',{});p=a.get('root_selected_policy',{});f=a.get('b2_foundation_receipt',{})
 if g.get('sha256')!='40bfaf8be6c878db0b8711be22245f6f2955ac241433d024d8bdbaf41af05a91' or g.get('root_revalidation')!='PASS_16_OF_16_EXACT_5_PRODUCTION_3_TESTS_AND_SINGLE_CMAKE_TARGET_AUTHORIZED':e.append('gate')
 if p.get('selected_option_id')!='SIGNED_CONFUSION_DECLARATION_ASSIGNMENT_ERROR_ONLY' or p.get('row_id')!='WPA-II-19' or f.get('sha256')!='1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b':e.append('authority')
 if check_files:
  for x,label in ((g,'gate'),(p,'policy'),(f,'foundation')):
   if not bind({'path':x.get('path'),'sha256':x.get('sha256')}):e.append(label+'_drift')
 red=v.get('tests_first_record',{});red=red if isinstance(red,dict) else {}
 if red.get('red_observed') is not True or red.get('red_exit_code')!=1 or 'no such file' not in str(red.get('red_failure')) or red.get('green_after_minimal_production_patch') is not True:e.append('red_green')
 c=v.get('implemented_contract',{});c=c if isinstance(c,dict) else {}
 if c.get('states')!=['INACTIVE','TIMELY_WINDOW_OPEN','REALIZATION_PROPOSED_RACK_STOPPED','RERACK_COMMITTING','RERACK_COMMITTED','WINDOW_EXPIRED'] or c.get('classification')!='ASSIGNMENT_OR_SEQUENCE_CONFUSION_ONLY':e.append('states')
 for k in ('section_17_bih_preserved_on_failed_stale_or_conflicting_request','section_17_bih_superseded_only_by_successful_final_rerack','exact_retry_zero_delta','commit_failure_full_rollback','reset_and_other_official_rerack_invalidate'):
  if c.get(k) is not True:e.append('contract_true:'+k)
 for k in ('cancel_supported','ordinary_wrong_first_contact_auto_section_19','direct_core_e_command_exposed','signed_payload_promoted_to_physics_shotfacts_or_assignment','visible_or_global_rules_ui_exposed'):
  if c.get(k) is not False:e.append('contract_false:'+k)
 au=v.get('automation_headless',{});au=au if isinstance(au,dict) else {}
 if au.get('capability')!='wpa_ii_19_group_confusion_referee_consumer_v1' or au.get('commands')!=COMMANDS or au.get('cancel_command_exposed') is not False or au.get('request_exact_field_count')!=17 or au.get('readback_omits')!=['credential_id','authentication_proof','payload_canonical'] or au.get('headless_only') is not True:e.append('automation')
 fx=v.get('fixture_matrix',{});fx=fx if isinstance(fx,dict) else {}
 if len(fx.get('positive',[]))!=10 or len(fx.get('negative',[]))!=18 or len(set(fx.get('positive',[])+fx.get('negative',[])))!=28:e.append('fixtures')
 s=v.get('source_bindings');t=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD:e.append('sources')
 if not isinstance(t,list) or [x.get('path') for x in t if isinstance(x,dict)]!=TEST:e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(t,'test')):
   if isinstance(rows,list):
    for x in rows:
     if not bind(x):e.append(label+'_drift')
 cpu=v.get('cpu_verification',{});cpu=cpu if isinstance(cpu,dict) else {};b=cpu.get('required_binaries')
 if not isinstance(b,list) or len(b)!=5 or any(x.get('exit_code')!=0 for x in b) or len(cpu.get('wide_passed_targets',[]))!=5 or cpu.get('all_exit_code')!=0 or cpu.get('gpu_cocoa_opengl_executed') is not False:e.append('cpu')
 elif check_files:
  for x in b:
   if not bind({'path':x.get('path'),'sha256':x.get('sha256')}):e.append('binary_drift')
 cb=v.get('completion_boundary',{});cb=cb if isinstance(cb,dict) else {}
 if cb.get('wpa_ii_19_internal_headless_consumer_implemented') is not True:e.append('implemented')
 for k in ('visible_product_ui_implemented','audit_row_promoted','b2_complete','m4_complete','protocol_rack_executor_rules_shotfacts_or_physics_modified','global_rules_ui_exposed','gpu_executed','readiness_or_authorization_created'):
  if cb.get(k) is not False:e.append('false_claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(Path(a.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_19_SIGNED_GROUP_CONFUSION_CONSUMER_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
