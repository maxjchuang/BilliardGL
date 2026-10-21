#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-12-7-signed-push-review-declaration-lifecycle-implementation-receipt-v1.json'
PROD=['CMakeLists.txt','src/Billiards/rules/push_review_declaration.h','src/Billiards/rules/push_review_declaration.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp']
TEST=['tests/push_review_declaration_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
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
 e=[];keys={'schema','status','authority_bindings','tests_first','contract','automation','fixtures','source_bindings','test_bindings','cpu','completion'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='wpa_ii_12_7_signed_push_review_declaration_lifecycle_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='PARTIAL_PUSH_REVIEW_CPU_VERIFIED_D5_AUDIT_OPEN':e.append('status')
 a=v.get('authority_bindings',{});expected={'implementation_design_gate':'7dfb149547c8f831c362aadc4d9a30c5f2c6de4676364b23d9ec32046d3f477e','root_selected_policy':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','b2_foundation_receipt':'1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b'}
 for key,digest in expected.items():
  row=a.get(key,{}) if isinstance(a,dict) else {}
  if row.get('sha256')!=digest:e.append('authority:'+key)
  elif check_files and not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('authority_drift:'+key)
 policy=a.get('root_selected_policy',{}) if isinstance(a,dict) else {}
 if policy.get('row_id')!='WPA-II-12-7' or policy.get('selected_option_id')!='MEASUREMENTS_PLUS_SIGNED_REFEREE_PUSH_DECLARATION' or policy.get('row_implementation_allowed') is not False:e.append('policy_boundary')
 red=v.get('tests_first',{})
 if red.get('red_observed') is not True or red.get('red_exit_code')!=1 or 'no such file' not in str(red.get('red_failure')) or red.get('green') is not True:e.append('red')
 c=v.get('contract',{})
 for key in ('review_needed_only','consume_once','exact_retry'):
  if c.get(key) is not True:e.append('true:'+key)
 for key in ('measurement_complete','push_classified','foul_classified','thresholds_selected','microtrace_as_measurement'):
  if c.get(key) is not False:e.append('false:'+key)
 au=v.get('automation',{});commands=['rules.push_review_propose','rules.push_review_final','rules.push_review_cancel','get_push_review_state']
 if au.get('capability')!='wpa_ii_12_7_signed_push_review_declaration_v1' or au.get('commands')!=commands or au.get('headless') is not True or au.get('secret_free') is not True:e.append('automation')
 fx=v.get('fixtures',{});pos=fx.get('positive',[]) if isinstance(fx,dict) else [];neg=fx.get('negative',[]) if isinstance(fx,dict) else []
 if len(pos)!=10 or len(neg)!=19 or len(set(pos+neg))!=29:e.append('fixtures')
 s=v.get('source_bindings');t=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD:e.append('sources')
 if not isinstance(t,list) or [x.get('path') for x in t if isinstance(x,dict)]!=TEST:e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(t,'test')):
   if isinstance(rows,list):
    for row in rows:
     if not bind(row):e.append(label+'_drift')
 cpu=v.get('cpu',{});bins=cpu.get('binaries',[]) if isinstance(cpu,dict) else []
 if cpu.get('build_root')!='build-wpa-ii-12-7-green-v1' or cpu.get('passed_count')!=12 or len(bins)!=12 or any(not isinstance(x,dict) or x.get('exit_code')!=0 for x in bins) or cpu.get('gpu') is not False:e.append('cpu')
 elif check_files:
  for row in bins:
   if not bind({'path':row.get('path'),'sha256':row.get('sha256')}):e.append('binary_drift')
 cb=v.get('completion',{})
 if cb.get('partial_lifecycle') is not True:e.append('partial')
 for key in ('d5_complete','audit_promoted','m4_complete','forbidden_sources_modified','ui_gpu_readiness'):
  if cb.get(key) is not False:e.append('false_claim:'+key)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');args=ap.parse_args()
 try:v=json.loads(Path(args.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not args.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_12_7_SIGNED_PUSH_REVIEW_PARTIAL_LIFECYCLE_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
