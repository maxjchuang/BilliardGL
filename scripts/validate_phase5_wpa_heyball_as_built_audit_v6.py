#!/usr/bin/env python3
"""Validate the read-only WPA-II-08 completion audit successor."""
from __future__ import annotations
from collections import Counter
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v6.json'
V5=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json'
V5_SHA='2811958f1205d9998b40060b83c577f91359d2164e1ac0eaa9ccc4cac2ad787a'
SUMMARY={'total':40,'implemented':12,'partial':8,'missing':13,'procedural_only':7}
COMPLETION_PATHS=['src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/rules/chinese_eight_ball_rules.h','src/Billiards/rules/chinese_eight_ball_rules.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp','tests/chinese_eight_ball_rules_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']
EVIDENCE_PATHS=['docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-receipt-v1.json','docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-receipt-v1.json','src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.cpp','tests/chinese_eight_ball_rules_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']
def sha(p:Path)->str:return hashlib.sha256(p.read_bytes()).hexdigest()
def digest(v:Any)->str:return hashlib.sha256(json.dumps(v,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def safe(s:Any):
 if not isinstance(s,str) or Path(s).is_absolute() or '..' in Path(s).parts:return None
 p=(ROOT/s).resolve();return p if p.is_relative_to(ROOT.resolve()) else None
def validate(v:Any,check_files=True)->list[str]:
 if not isinstance(v,dict):return ['audit_not_object']
 e=[]
 if v.get('schema')!='phase5_wpa_heyball_as_built_reconciliation_audit_v6' or v.get('schema_version')!=6 or v.get('audit_scope')!='wpa_ii_08_complete_clause_successor' or v.get('audit_status')!='FINAL_ROOT_ACCEPTED_WPA_II_08_ONLY_SUCCESSOR':e.append('schema_scope')
 if v.get('frozen_v5_predecessor')!={'path':'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v5.json','sha256':V5_SHA}:e.append('predecessor')
 matrix=v.get('matrix')
 if not isinstance(matrix,list) or len(matrix)!=40:return e+['matrix']
 ids=[r.get('id') for r in matrix if isinstance(r,dict)]
 if len(ids)!=40 or len(set(ids))!=40:e.append('ids')
 try:old=json.loads(V5.read_text())
 except Exception:old=None;e.append('predecessor_load')
 old_rows={r['id']:r for r in old['matrix']} if isinstance(old,dict) else {}
 rows={r['id']:r for r in matrix if isinstance(r,dict)}
 target=rows.get('WPA-II-08')
 if not isinstance(target,dict) or target.get('status')!='IMPLEMENTED' or target.get('product_reachable') is not True or target.get('gaps')!=[] or target.get('m4_required') is not True or target.get('product_action_required') is not True:e.append('target')
 if isinstance(target,dict):
  if [x.get('path') for x in target.get('evidence',[]) if isinstance(x,dict)]!=EVIDENCE_PATHS:e.append('target_evidence')
  if digest(target.get('evidence'))!='35244d74cc36492437a6c8a090804587ba128c29587097167e0ab6f6e3cc2d1c':e.append('target_evidence_digest')
 for row_id,row in rows.items():
  if row_id=='WPA-II-08':
   for key in ('id','source','requirement','m4_required','policy_decision_required','product_action_required','core_increments'):
    if row.get(key)!=old_rows.get(row_id,{}).get(key):e.append('target_immutable:'+key)
  elif row!=old_rows.get(row_id):e.append('other_row_drift:'+row_id)
 counts=Counter(r.get('status') for r in matrix)
 computed={'total':40,'implemented':counts['IMPLEMENTED'],'partial':counts['PARTIAL'],'missing':counts['MISSING'],'procedural_only':counts['PROCEDURAL_ONLY']}
 if v.get('summary')!=SUMMARY or computed!=SUMMARY:e.append('summary')
 expected=[r['id'] for r in matrix if r.get('m4_required') and r.get('status')!='IMPLEMENTED']
 if v.get('m4_required_unclosed')!=expected or 'WPA-II-08' in expected:e.append('m4_inventory')
 completion=v.get('wpa_ii_08_completion')
 semantics=completion.get('official_semantics') if isinstance(completion,dict) else None
 if not isinstance(semantics,dict) or set(semantics)!={'behind_head_string_placement','all_behind_respot_tie_foot_search','center_crossing_strictly_before_first_contact','narrow_product_reachability','atomic_idempotent_fail_closed'} or not all(x is True for x in semantics.values()):e.append('semantics')
 if not isinstance(completion,dict) or completion.get('remaining_gaps')!=[] or completion.get('row_status')!='IMPLEMENTED' or completion.get('product_reachable') is not True or completion.get('other_rows_changed') is not False:e.append('completion')
 for k in ('global_product_exposed','profile_complete','m4_complete','production_changed','cmake_changed','gpu_executed','readiness_or_authorization_created'):
  if not isinstance(completion,dict) or completion.get(k) is not False:e.append('completion_false:'+k)
 policy=completion.get('root_selected_policy') if isinstance(completion,dict) else None;telemetry=completion.get('telemetry_receipt') if isinstance(completion,dict) else None;allb=completion.get('all_behind_receipt') if isinstance(completion,dict) else None
 if policy!={'path':'docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','sha256':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f'}:e.append('policy')
 if telemetry!={'path':'docs/rules/wpa-ii-08-exact-continuous-crossing-telemetry-implementation-receipt-v1.json','sha256':'86379eecd2437a6e17643d0964f4460b857615f0eddbc50ab7dbeaefe3afa878'}:e.append('telemetry')
 if allb!={'path':'docs/rules/wpa-ii-08-all-behind-respot-product-subflow-implementation-receipt-v1.json','sha256':'0dfa4e71448b26840e9688c4065ffb1180ab102d4e152c0e2003bfdf9c6e2a3f'}:e.append('all_behind')
 bindings=v.get('wpa_ii_08_completion_bindings',{}).get('production_and_test_files')
 if not isinstance(bindings,list) or [x.get('path') for x in bindings if isinstance(x,dict)]!=COMPLETION_PATHS:e.append('bindings')
 if v.get('profile_complete') is not False or v.get('product_exposed') is not False or v.get('complete_profile_product_exposed') is not False:e.append('profile_boundary')
 if check_files:
  if not V5.is_file() or sha(V5)!=V5_SHA:e.append('predecessor_drift')
  for collection,label in ((target.get('evidence',[]) if isinstance(target,dict) else [],'evidence'),(bindings if isinstance(bindings,list) else [],'binding')):
   for item in collection:
    p=safe(item.get('path')) if isinstance(item,dict) else None
    if p is None or not p.is_file() or sha(p)!=item.get('sha256'):e.append(label+'_drift')
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--audit',type=Path,default=DEFAULT);ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(a.audit.read_text())
 except Exception as x:print(json.dumps({'status':'FAIL','errors':[str(x)]}));return 1
 e=validate(v,not a.no_file_checks);print(json.dumps({'status':'PASS' if not e else 'FAIL','errors':e},sort_keys=True))
 if not e:print('PASS_PHASE5_WPA_HEYBALL_AS_BUILT_AUDIT_V6_WPA_II_08_COMPLETE')
 return bool(e)
if __name__=='__main__':raise SystemExit(main())
