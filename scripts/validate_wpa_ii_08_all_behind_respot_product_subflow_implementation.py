#!/usr/bin/env python3
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from typing import Any

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-receipt-v1.json'
KEYS={'schema','status','authority','implementation_contract','fixture_matrix','source_bindings','implementation_aggregate_sha256','test_bindings','cpu_verification','cmake_binding','claims'}
DIGESTS={'authority':'a87ebe8767aa223d22c596d9d1e3bc8df51224a4f970f82fc2335bef71c6b490','implementation_contract':'6de78df75397d4ac9eb3f280e54c855ff398db1a1dd9cca55dbef58e12a6e20d','fixture_matrix':'27932f64b0815d2237754c248cd09b3d58884d5dc3620ad518c67c0bd4b58648','source_bindings':'dcf866ee544b8f882cfae96d077fc160d3a43153bf5cd29fcab685747080b154','test_bindings':'97f518285efb089396a0b0b3fc2255a72e079bb1c8e19d11d9f194fca9265d63','cpu_verification':'65a05e1c8a7a033b7df11361326faa7c819d4e60b67f74139e883aaad1faf4d0','cmake_binding':'afd6dc405a748b90032257d361c54cb750582c54ea39ff2a33ba41fc19a52579','claims':'5200d9b71c249228c179ebc7f70b413b5843fb065fc7b90076256778c559ec98'}
PROD=['src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/rules/chinese_eight_ball_rules.h','src/Billiards/rules/chinese_eight_ball_rules.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp']
TESTS=['tests/chinese_eight_ball_rules_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']
POS=['OPEN_TABLE_ALL_BEHIND_UNIQUE_CLOSEST','ASSIGNED_GROUP_ALL_BEHIND_UNIQUE_CLOSEST','GROUP_CLEARED_EIGHT_ONLY_CLOSEST','EXACT_TIE_STAGES_ASCENDING_PLAYER_CHOICE','SELECTED_TIE_BALL_RESPOTS_AT_FOOT_SPOT','OBSTRUCTED_FOOT_SPOT_USES_SECTION7_SEARCH','SUCCESS_PRESERVES_ACTOR_AND_REPLACES_ONE_CORE_G_CHILD','EXACT_RETRY_RETURNS_ORIGINAL_ACK_ZERO_DELTA']
NEG=['NO_LEGAL_OBJECT_REJECTS','LEGAL_OBJECT_ON_LINE_SUPPRESSES_OFFER','LEGAL_OBJECT_FOOT_SIDE_SUPPRESSES_OFFER','OPPONENT_GROUP_AND_EIGHT_EXCLUDED_UNTIL_GROUP_CLEARED','WRONG_ACTOR_ACTION_FLOW_DECISION_REVISION_OR_HASH_ROLLS_BACK','NONFINITE_OBJECT_GEOMETRY_ROLLS_BACK','STALE_LAYOUT_BETWEEN_STAGE_AND_COMMIT_ROLLS_BACK','FOUNDATION_NO_SOLUTION_PRESERVES_OLD_CORE_G_CHILD','FOUNDATION_OR_SIBLING_ACTION_CANNOT_ENTER_PRODUCT_SEAM','CONFLICTING_RETRY_ROLLS_BACK','RESET_INVALIDATES_PARENT_TIE_AND_PRODUCT_RETRY']
def sha(b:bytes)->str:return hashlib.sha256(b).hexdigest()
def dig(v:Any)->str:return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def resolve(s:Any):
 if not isinstance(s,str):return None
 p=Path(s)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def bind(b:Any)->bool:
 if not isinstance(b,dict) or set(b)!={'path','sha256'}:return False
 p=resolve(b.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==b.get('sha256')
def validate(v:Any,check_files=True)->list[str]:
 if not isinstance(v,dict):return ['root_not_object']
 e=[]
 if set(v)!=KEYS:e.append('keys')
 if v.get('schema')!='wpa_ii_08_all_behind_respot_product_subflow_implementation_receipt_v1':e.append('schema')
 if v.get('status')!='IMPLEMENTED_CPU_VERIFIED_NARROW_PRODUCT_SUBFLOW_NO_AUTO_M4_COMPLETION':e.append('status')
 for k,d in DIGESTS.items():
  if dig(v.get(k))!=d:e.append('digest:'+k)
 a=v.get('authority');g=a.get('implementation_design_gate') if isinstance(a,dict) else None;t=a.get('telemetry_receipt') if isinstance(a,dict) else None
 if not isinstance(g,dict) or g.get('sha256')!='e998d1e9110982ef17a011d1f667b1062b63a2d58f4ce36b7f5df1ad19e1139c' or g.get('root_authorized_exact_7_plus_3_allowlist') is not True:e.append('gate')
 if not isinstance(t,dict) or t.get('sha256')!='86379eecd2437a6e17643d0964f4460b857615f0eddbc50ab7dbeaefe3afa878':e.append('telemetry')
 c=v.get('implementation_contract')
 for k in ('foundation_direct_product_access','global_rules_ui_exposed'):
  if not isinstance(c,dict) or c.get(k) is not False:e.append('contract_false:'+k)
 f=v.get('fixture_matrix')
 if not isinstance(f,dict) or f.get('positive')!=POS or f.get('negative')!=NEG or f.get('telemetry_positive_regression')!=8 or f.get('telemetry_negative_regression')!=9:e.append('fixtures')
 s=v.get('source_bindings');ts=v.get('test_bindings')
 if not isinstance(s,list) or [x.get('path') for x in s if isinstance(x,dict)]!=PROD:e.append('sources')
 elif v.get('implementation_aggregate_sha256')!=dig(s):e.append('aggregate')
 if not isinstance(ts,list) or [x.get('path') for x in ts if isinstance(x,dict)]!=TESTS:e.append('tests')
 if check_files:
  for rows,label in ((s,'source'),(ts,'test')):
   if isinstance(rows,list):
    for x in rows:
     if not bind(x):e.append(label+'_drift')
  for x,label in ((g,'gate'),(t,'telemetry')):
   if not isinstance(x,dict) or not bind({'path':x.get('path'),'sha256':x.get('sha256')}):e.append(label+'_drift')
 cpu=v.get('cpu_verification');req=cpu.get('required') if isinstance(cpu,dict) else None
 if not isinstance(req,list) or len(req)!=3 or any(x.get('exit_code')!=0 for x in req) or cpu.get('all_exit_code')!=0 or cpu.get('collector_cmake_link_gap_unchanged_and_workaround_reused') is not True or cpu.get('gpu_cocoa_opengl_executed') is not False:e.append('cpu')
 elif check_files:
  for x in req:
   if not bind({'path':x.get('path'),'sha256':x.get('sha256')}):e.append('binary_drift')
 cm=v.get('cmake_binding')
 if not isinstance(cm,dict) or cm.get('modified') is not False:e.append('cmake')
 elif check_files and not bind({'path':cm.get('path'),'sha256':cm.get('sha256')}):e.append('cmake_drift')
 cl=v.get('claims')
 for k in ('all_behind_parent_respot_product_subflow_implemented','telemetry_8_positive_9_negative_regressed','narrow_product_reachable'):
  if not isinstance(cl,dict) or cl.get(k) is not True:e.append('claim_true:'+k)
 for k in ('wpa_ii_08_row_auto_closed','m4_complete','audit_modified','foundation_product_exposed','global_rules_ui_exposed','new_production_file','rack_executor_modified','cmake_modified','visual_ui_modified','gpu_executed','readiness_or_authorization_created'):
  if not isinstance(cl,dict) or cl.get(k) is not False:e.append('false_completion:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('receipt',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(Path(a.receipt).read_text())
 except Exception as x:print('INVALID:',x);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_08_ALL_BEHIND_RESPOT_PRODUCT_SUBFLOW_IMPLEMENTATION');return 0
if __name__=='__main__':raise SystemExit(main())
