#!/usr/bin/env python3
"""Validate the read-only NONFINAL WPA-II-09 completion successor."""
from __future__ import annotations
from collections import Counter
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v7.json';V6=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v6.json';V6_SHA='8f6abc94c3cb0c94518b47bd9bde2208744af77678200387064a196f33e85f56'
SUMMARY={'total':40,'implemented':13,'partial':7,'missing':13,'procedural_only':7}
PATHS=['src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp','tests/game_runtime_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']
EVIDENCE=['docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-design-gate-v1.json','docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-receipt-v1.json','src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/game_runtime.cpp','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.cpp','src/Billiards/render/modern/modern_product_session.cpp','tests/game_runtime_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dig(v):return hashlib.sha256(json.dumps(v,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def safe(s):
 if not isinstance(s,str) or Path(s).is_absolute() or '..' in Path(s).parts:return None
 p=(ROOT/s).resolve();return p if p.is_relative_to(ROOT.resolve()) else None
def validate(v:Any,check_files=True):
 if not isinstance(v,dict):return ['audit_not_object']
 e=[]
 if v.get('schema')!='phase5_wpa_heyball_as_built_reconciliation_audit_v7' or v.get('schema_version')!=7 or v.get('audit_scope')!='wpa_ii_09_root_selected_coordinate_only_completion_successor' or v.get('audit_status')!='FINAL_ROOT_ACCEPTED_WPA_II_09_ONLY_SUCCESSOR':e.append('schema_scope')
 if v.get('frozen_v6_predecessor')!={'path':'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v6.json','sha256':V6_SHA}:e.append('predecessor')
 matrix=v.get('matrix')
 if not isinstance(matrix,list) or len(matrix)!=40:return e+['matrix']
 ids=[r.get('id') for r in matrix if isinstance(r,dict)]
 if len(ids)!=40 or len(set(ids))!=40:e.append('ids')
 try:old=json.loads(V6.read_text())
 except Exception:old=None;e.append('predecessor_load')
 oldrows={r['id']:r for r in old['matrix']} if isinstance(old,dict) else {};rows={r['id']:r for r in matrix if isinstance(r,dict)};target=rows.get('WPA-II-09')
 if not isinstance(target,dict) or target.get('status')!='IMPLEMENTED' or target.get('product_reachable') is not True or target.get('gaps')!=[] or target.get('m4_required') is not True or target.get('product_action_required') is not True:e.append('target')
 if isinstance(target,dict):
  if [x.get('path') for x in target.get('evidence',[]) if isinstance(x,dict)]!=EVIDENCE:e.append('evidence')
  if dig(target.get('evidence'))!='d15257d467817b70c0661cb1d69cdc9870fc14d628292fc302cfc8b63d229202':e.append('evidence_digest')
  text=str(target.get('assessment'))
  for token in ('root-selected','coordinate-placement','strictly-positive','DISABLED_UNSUPPORTED','not a claim'):
   if token not in text:e.append('assessment:'+token)
 for row_id,row in rows.items():
  if row_id=='WPA-II-09':
   for k in ('id','source','requirement','m4_required','policy_decision_required','product_action_required','core_increments'):
    if row.get(k)!=oldrows.get(row_id,{}).get(k):e.append('target_immutable:'+k)
  elif row!=oldrows.get(row_id):e.append('other_row_drift:'+row_id)
 counts=Counter(r.get('status') for r in matrix);computed={'total':40,'implemented':counts['IMPLEMENTED'],'partial':counts['PARTIAL'],'missing':counts['MISSING'],'procedural_only':counts['PROCEDURAL_ONLY']}
 if v.get('summary')!=SUMMARY or computed!=SUMMARY:e.append('summary')
 expected=[r['id'] for r in matrix if r.get('m4_required') and r.get('status')!='IMPLEMENTED']
 if v.get('m4_required_unclosed')!=expected or 'WPA-II-09' in expected:e.append('m4_inventory')
 c=v.get('wpa_ii_09_completion',{});c=c if isinstance(c,dict) else {};scope=c.get('selected_shipping_scope',{});scope=scope if isinstance(scope,dict) else {}
 if scope.get('option_id')!='COORDINATE_PLACEMENT_ONLY_POSITIVE_IMPULSE_CONSUMES' or scope.get('physical_cue_part_reposition')!='DISABLED_UNSUPPORTED_SELECTED_SCOPE_DEFER' or scope.get('forward_stroke_classifier')!='DISABLED_UNSUPPORTED_SELECTED_SCOPE_DEFER' or scope.get('defer_is_not_physical_implementation_claim') is not True:e.append('selected_scope')
 for k in ('coordinate_preview_confirm_repeated','strict_positive_authoritative_impulse_consumes_once','zero_rejected_unsupported_miscue_failed_launch_preserve','atomic_idempotent_fail_closed'):
  if scope.get(k) is not True:e.append('scope_true:'+k)
 if c.get('remaining_gaps_within_selected_shipping_scope')!=[] or c.get('row_status')!='IMPLEMENTED' or c.get('product_reachable') is not True or c.get('other_rows_changed') is not False:e.append('completion')
 for k in ('global_product_exposed','profile_complete','m4_complete','production_changed','cmake_changed','gpu_executed','readiness_or_authorization_created'):
  if c.get(k) is not False:e.append('completion_false:'+k)
 expected_refs=(('root_selected_policy','fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f'),('implementation_design_gate','412e89c780f6e7f14719a05957f7f774c86c240fddd8b687a3c041ab50cfe93f'),('implementation_receipt','6c7d365d14013c6aa2937f7df81f9d9b5ce4b85fe087fa4dc1b4e068c7d78a70'))
 for k,h in expected_refs:
  if not isinstance(c.get(k),dict) or c[k].get('sha256')!=h:e.append('ref:'+k)
 bindings=v.get('wpa_ii_09_completion_bindings',{});bindings=bindings.get('production_and_test_files') if isinstance(bindings,dict) else None
 if not isinstance(bindings,list) or [x.get('path') for x in bindings if isinstance(x,dict)]!=PATHS:e.append('bindings')
 if v.get('profile_complete') is not False or v.get('product_exposed') is not False or v.get('complete_profile_product_exposed') is not False:e.append('profile_boundary')
 if check_files:
  if not V6.is_file() or sha(V6)!=V6_SHA:e.append('predecessor_drift')
  for collection,label in ((target.get('evidence',[]) if isinstance(target,dict) else [],'evidence'),(bindings if isinstance(bindings,list) else [],'binding')):
   for item in collection:
    p=safe(item.get('path')) if isinstance(item,dict) else None
    if p is None or not p.is_file() or sha(p)!=item.get('sha256'):e.append(label+'_drift')
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('--audit',type=Path,default=DEFAULT);ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(a.audit.read_text())
 except Exception as x:print(json.dumps({'status':'FAIL','errors':[str(x)]}));return 1
 e=validate(v,not a.no_file_checks);print(json.dumps({'status':'PASS' if not e else 'FAIL','errors':e},sort_keys=True));return bool(e)
if __name__=='__main__':raise SystemExit(main())
