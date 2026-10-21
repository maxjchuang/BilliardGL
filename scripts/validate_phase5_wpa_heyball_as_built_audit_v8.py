#!/usr/bin/env python3
from __future__ import annotations
from collections import Counter
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v8.json';V7=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v7.json';V7_SHA='d1654e8a82fcfb9c9621b90b36cb4d3eec14b5765025c8c49337b45891d33e15'
SUMMARY={'total':40,'implemented':14,'partial':7,'missing':12,'procedural_only':7}
LIVE=['CMakeLists.txt','src/Billiards/rules/stalemate_authority.h','src/Billiards/rules/stalemate_authority.cpp','src/Billiards/rules/referee_authority_protocol.h','src/Billiards/rules/referee_authority_protocol.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp','tests/stalemate_authority_tests.cpp','tests/referee_authority_protocol_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
EVIDENCE=['docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json','docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-design-gate-v1.json','docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-receipt-v1.json','src/Billiards/rules/stalemate_authority.h','src/Billiards/rules/stalemate_authority.cpp','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp','tests/stalemate_authority_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
REFS={'root_selected_policy':('docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f'),'b2_foundation_receipt':('docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json','1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b'),'implementation_design_gate':('docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-design-gate-v1.json','f4820aa0a63fde6857a60208cc1e190f572bb57c6a5bc02a516e5d034b28b1f0'),'implementation_receipt':('docs/rules/wpa-ii-21-stalemate-referee-consumer-implementation-receipt-v1.json','0829217fd580d0302ee96d6b4068535f03632420d53f0ae6ca1bb4f2b86644e2')}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dig(v):return hashlib.sha256(json.dumps(v,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def safe(s):
 if not isinstance(s,str) or Path(s).is_absolute() or '..' in Path(s).parts:return None
 p=(ROOT/s).resolve();return p if p.is_relative_to(ROOT.resolve()) else None
def validate(v:Any,check_files=True):
 if not isinstance(v,dict):return ['audit_not_object']
 e=[]
 if v.get('schema')!='phase5_wpa_heyball_as_built_reconciliation_audit_v8' or v.get('schema_version')!=8 or v.get('audit_scope')!='wpa_ii_21_headless_authenticated_referee_authority_completion_successor' or v.get('audit_status')!='FINAL_ROOT_ACCEPTED_WPA_II_21_ONLY_SUCCESSOR':e.append('schema_scope')
 if v.get('frozen_v7_predecessor')!={'path':'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v7.json','sha256':V7_SHA}:e.append('predecessor')
 matrix=v.get('matrix')
 if not isinstance(matrix,list) or len(matrix)!=40:return e+['matrix']
 ids=[r.get('id') for r in matrix if isinstance(r,dict)]
 if len(ids)!=40 or len(set(ids))!=40:e.append('ids')
 try:old=json.loads(V7.read_text())
 except Exception:old=None;e.append('predecessor_load')
 oldrows={r['id']:r for r in old['matrix']} if isinstance(old,dict) else {};rows={r['id']:r for r in matrix if isinstance(r,dict)};target=rows.get('WPA-II-21')
 if not isinstance(target,dict) or target.get('status')!='IMPLEMENTED' or target.get('product_reachable') is not True or target.get('gaps')!=[] or target.get('m4_required') is not True or target.get('product_action_required') is not False:e.append('target')
 if isinstance(target,dict):
  if [x.get('path') for x in target.get('evidence',[]) if isinstance(x,dict)]!=EVIDENCE:e.append('evidence')
  if dig(target.get('evidence'))!='30439e457a6b6d4ca6ad498c0b79399788fb8322165f299f7e275f3e772cd027':e.append('evidence_digest')
  text=str(target.get('assessment'))
  for token in ('root-selected','authenticated headless','threshold only awaits','original breaker','product_action_required=false','Visible/global player Rules UI remains false','not a claim'):
   if token not in text:e.append('assessment:'+token)
 for row_id,row in rows.items():
  if row_id=='WPA-II-21':
   for k in ('id','source','requirement','m4_required','policy_decision_required','product_action_required','core_increments'):
    if row.get(k)!=oldrows.get(row_id,{}).get(k):e.append('target_immutable:'+k)
  elif row!=oldrows.get(row_id):e.append('other_row_drift:'+row_id)
 counts=Counter(r.get('status') for r in matrix);computed={'total':40,'implemented':counts['IMPLEMENTED'],'partial':counts['PARTIAL'],'missing':counts['MISSING'],'procedural_only':counts['PROCEDURAL_ONLY']}
 if v.get('summary')!=SUMMARY or computed!=SUMMARY:e.append('summary')
 expected=[r['id'] for r in matrix if r.get('m4_required') and r.get('status')!='IMPLEMENTED']
 if v.get('m4_required_unclosed')!=expected or 'WPA-II-21' in expected:e.append('m4_inventory')
 oldplans=[p for p in old.get('next_deterministic_implementation_plan',[]) if 'WPA-II-21' not in p.get('rows',[])] if isinstance(old,dict) else []
 if v.get('next_deterministic_implementation_plan')!=oldplans:e.append('plan')
 c=v.get('wpa_ii_21_completion',{});c=c if isinstance(c,dict) else {};scope=c.get('selected_shipping_scope',{});scope=scope if isinstance(scope,dict) else {}
 if scope.get('option_id')!='SIGNED_REFEREE_DIRECTIVE_DEFAULT_THREE_EACH' or scope.get('product_reachability_basis')!='HEADLESS_AUTHENTICATED_REFEREE_AUTHORITY_FOR_PRODUCT_ACTION_REQUIRED_FALSE_ROW' or scope.get('visible_player_rules_ui')!='NOT_EXPOSED_NOT_REQUIRED_BY_AUDIT_ROW_CLASSIFICATION' or scope.get('visible_ui_not_claimed') is not True:e.append('scope')
 for k in ('authenticated_referee_propose_final_cancel','detached_signed_player_refusal','first_refusal_starts_rack_scoped_counters','threshold_only_awaits_referee_final','core_e_frozen_original_breaker_and_order','atomic_idempotent_fail_closed'):
  if scope.get(k) is not True:e.append('scope_true:'+k)
 for k,(path,h) in REFS.items():
  if c.get(k)!={'path':path,'sha256':h}:e.append('ref:'+k)
 if c.get('remaining_gaps_within_selected_shipping_scope')!=[] or c.get('row_status')!='IMPLEMENTED' or c.get('product_reachable') is not True or c.get('other_rows_changed') is not False:e.append('completion')
 for k in ('global_product_exposed','visible_player_ui_exposed','profile_complete','m4_complete','production_changed','cmake_changed','gpu_executed','readiness_or_authorization_created'):
  if c.get(k) is not False:e.append('completion_false:'+k)
 if c.get('audit_generated_only') is not True:e.append('audit_only')
 b=v.get('wpa_ii_21_completion_bindings',{});b=b.get('exact_live_allowlist_files') if isinstance(b,dict) else None
 if not isinstance(b,list) or [x.get('path') for x in b if isinstance(x,dict)]!=LIVE or dig(b)!='90483a56029dfa77899cb948701332a9d4bc4a929e20a17d0f847e71ba49e1db':e.append('bindings')
 if v.get('profile_complete') is not False or v.get('product_exposed') is not False or v.get('complete_profile_product_exposed') is not False:e.append('profile_boundary')
 if check_files:
  if not V7.is_file() or sha(V7)!=V7_SHA:e.append('predecessor_drift')
  for collection,label in ((target.get('evidence',[]) if isinstance(target,dict) else [],'evidence'),(b if isinstance(b,list) else [],'binding')):
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
