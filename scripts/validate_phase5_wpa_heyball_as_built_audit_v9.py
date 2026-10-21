#!/usr/bin/env python3
from __future__ import annotations
from collections import Counter
import argparse,hashlib,json
from pathlib import Path
from typing import Any
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v9.json';V8=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v8.json';V8_SHA='ddeabaf87e43647588b321e599fbdf56a3429eb1ad990425cdb9bcf5a9339c94'
SUMMARY={'total':40,'implemented':15,'partial':7,'missing':11,'procedural_only':7}
LIVE=['CMakeLists.txt','src/Billiards/rules/group_confusion_authority.h','src/Billiards/rules/group_confusion_authority.cpp','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp','tests/group_confusion_authority_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
EVIDENCE=['docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json','docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-design-gate-v1.json','docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-receipt-v1.json','src/Billiards/rules/group_confusion_authority.h','src/Billiards/rules/group_confusion_authority.cpp','src/Billiards/game_runtime.cpp','src/Billiards/game_runtime.cpp','src/Billiards/game_runtime.cpp','src/Billiards/automation_controller.cpp','tests/group_confusion_authority_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/automation_controller_tests.cpp']
REFS={'root_selected_policy':('docs/rules/m4-wpa-policy-bundle-root-selected-v1.json','fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f'),'b2_foundation_receipt':('docs/rules/m4-b2-authenticated-referee-authority-protocol-foundation-implementation-receipt-v1.json','1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b'),'implementation_design_gate':('docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-design-gate-v1.json','40bfaf8be6c878db0b8711be22245f6f2955ac241433d024d8bdbaf41af05a91'),'implementation_receipt':('docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-receipt-v1.json','162478839f14fb121738b98f6688f690d1e4683b9132ccd67ee3a3f12e07ab18')}
def sha(p):return hashlib.sha256(p.read_bytes()).hexdigest()
def dig(v):return hashlib.sha256(json.dumps(v,sort_keys=True,separators=(',',':')).encode()).hexdigest()
def safe(s):
 if not isinstance(s,str) or Path(s).is_absolute() or '..' in Path(s).parts:return None
 p=(ROOT/s).resolve();return p if p.is_relative_to(ROOT.resolve()) else None
def validate(v:Any,check_files=True):
 if not isinstance(v,dict):return ['audit_not_object']
 e=[]
 if v.get('schema')!='phase5_wpa_heyball_as_built_reconciliation_audit_v9' or v.get('schema_version')!=9 or v.get('audit_scope')!='wpa_ii_19_headless_authenticated_realization_only_successor' or v.get('audit_status')!='FINAL_ROOT_ACCEPTED_WPA_II_19_ONLY_SUCCESSOR':e.append('schema_scope')
 if v.get('frozen_v8_predecessor')!={'path':'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v8.json','sha256':V8_SHA}:e.append('predecessor')
 matrix=v.get('matrix')
 if not isinstance(matrix,list) or len(matrix)!=40:return e+['matrix']
 ids=[r.get('id') for r in matrix if isinstance(r,dict)]
 if len(ids)!=40 or len(set(ids))!=40:e.append('ids')
 try:old=json.loads(V8.read_text())
 except Exception:old=None;e.append('predecessor_load')
 oldrows={r['id']:r for r in old['matrix']} if isinstance(old,dict) else {};rows={r['id']:r for r in matrix if isinstance(r,dict)};target=rows.get('WPA-II-19')
 if not isinstance(target,dict) or target.get('status')!='IMPLEMENTED' or target.get('product_reachable') is not True or target.get('gaps')!=[] or target.get('m4_required') is not False or target.get('product_action_required') is not False:e.append('target')
 if isinstance(target,dict):
  if [x.get('path') for x in target.get('evidence',[]) if isinstance(x,dict)]!=EVIDENCE or dig(target.get('evidence'))!='f4b387477d9268b72f932a23647a4c8388733205333093891e2fd165e5986e29':e.append('evidence')
  text=str(target.get('assessment'))
  for token in ('root-selected','headless authenticated realization','Ordinary WrongFirstContact remains WPA-II §17','superseded only by successful final rerack','product_action_required=false','Visible/global player Rules UI','not claimed'):
   if token not in text:e.append('assessment:'+token)
 for row_id,row in rows.items():
  if row_id=='WPA-II-19':
   for k in ('id','source','requirement','m4_required','policy_decision_required','product_action_required','core_increments'):
    if row.get(k)!=oldrows.get(row_id,{}).get(k):e.append('target_immutable:'+k)
  elif row!=oldrows.get(row_id):e.append('other_row_drift:'+row_id)
 counts=Counter(r.get('status') for r in matrix);computed={'total':40,'implemented':counts['IMPLEMENTED'],'partial':counts['PARTIAL'],'missing':counts['MISSING'],'procedural_only':counts['PROCEDURAL_ONLY']}
 if v.get('summary')!=SUMMARY or computed!=SUMMARY:e.append('summary')
 for key in ('m4_required_unclosed','next_deterministic_implementation_plan'):
  if v.get(key)!=old.get(key):e.append(key)
 c=v.get('wpa_ii_19_completion',{});c=c if isinstance(c,dict) else {};scope=c.get('selected_shipping_scope',{});scope=scope if isinstance(scope,dict) else {}
 if scope.get('option_id')!='SIGNED_CONFUSION_DECLARATION_ASSIGNMENT_ERROR_ONLY' or scope.get('product_reachability_basis')!='HEADLESS_AUTHENTICATED_REALIZATION_FOR_PRODUCT_ACTION_REQUIRED_FALSE_ROW' or scope.get('visible_player_rules_ui')!='NOT_EXPOSED' or scope.get('visible_referee_ui')!='NOT_EXPOSED' or scope.get('visible_ui_not_claimed') is not True:e.append('scope')
 for k in ('authenticated_referee_or_player_propose','same_actor_final','assignment_or_sequence_confusion_only','accepted_positive_shoot_deadline','ordinary_wrong_first_routes_wpa_ii_17','section_17_bih_preserved_until_successful_final','core_e_frozen_original_breaker_and_order','atomic_idempotent_fail_closed'):
  if scope.get(k) is not True:e.append('scope_true:'+k)
 for k,(path,h) in REFS.items():
  if c.get(k)!={'path':path,'sha256':h}:e.append('ref:'+k)
 if c.get('remaining_gaps_within_selected_shipping_scope')!=[] or c.get('row_status')!='IMPLEMENTED' or c.get('product_reachable') is not True or c.get('other_rows_changed') is not False:e.append('completion')
 for k in ('global_product_exposed','visible_player_ui_exposed','visible_referee_ui_exposed','profile_complete','m4_complete','production_changed','cmake_changed','gpu_executed','readiness_or_authorization_created'):
  if c.get(k) is not False:e.append('completion_false:'+k)
 if c.get('audit_generated_only') is not True:e.append('audit_only')
 b=v.get('wpa_ii_19_completion_bindings',{});b=b.get('exact_live_allowlist_files') if isinstance(b,dict) else None
 if not isinstance(b,list) or [x.get('path') for x in b if isinstance(x,dict)]!=LIVE or dig(b)!='51c2846a232b8b93ab43646e637c6820e695333ce9cfcfaba1087df342e60663':e.append('bindings')
 if v.get('profile_complete') is not False or v.get('product_exposed') is not False or v.get('complete_profile_product_exposed') is not False:e.append('profile_boundary')
 if check_files:
  if not V8.is_file() or sha(V8)!=V8_SHA:e.append('predecessor_drift')
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
