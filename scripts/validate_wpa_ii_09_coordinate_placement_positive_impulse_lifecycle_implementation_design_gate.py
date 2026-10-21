#!/usr/bin/env python3
"""Validate the design-only selected WPA-II-09 lifecycle gate."""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from typing import Any

ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/wpa-ii-09-coordinate-placement-positive-impulse-lifecycle-implementation-design-gate-v1.json'
KEYS={'schema','status','authority_bindings','feasibility_finding','coordinate_commit_contract','positive_impulse_consumption_contract','unsupported_boundary','implementation_allowlist','required_fixtures','source_bindings','test_source_bindings','cmake_binding','completion_boundary','claims'}
DIGESTS={
'authority_bindings':'87e35000d3eee1f8c457e7a0d1a6bd4d2f835a9042d9c64513c9e2d7df195cb9','feasibility_finding':'089a7d1817504a45c4333bda86bb38dadc6f4c1f80e3bacb54c7c35414782dca','coordinate_commit_contract':'49b18ca5ed7919827e1d038a904e70cf1ee0332dff0f5bb553ec6d3ab6155aee','positive_impulse_consumption_contract':'eae28f34e7cba76e135ab97a056b336aa95d15936b5a756b841c851e41e44bc8','unsupported_boundary':'4cfc9ab0ff8a106fd5f30c8d4684968260c895bce3ffeb41dc54e6b72624b9b3','implementation_allowlist':'4d72e84835200a225369450ac71eb4a78d0062ae4dd839da12b1716c69712739','required_fixtures':'5328e6a5037308ad8b17d8c7a60181b7901b1dfe70422cdf5642d4b2a463a95b','source_bindings':'1175c8502461b134e7dd11abadf87f3f86255afead5aede6cc8ebe241e45963b','test_source_bindings':'db3b02e3109f12816731acdf817af1b0a14fc500e19dcbf0787832c5cf598ba1','cmake_binding':'f04aa7dc2eb97f1c05ef1a3abc306bceb9d8f5d17a762472fc5218b0d131a784','completion_boundary':'dda4f4d0bd0c87cfbdc658599777c86c2668b61b70a5a3617239d674b5d56d00','claims':'ca94ccc864a65308ef69aab217d7b178162ea05b80ff45f1f7c87d7c7697cf8e'}
PROD=['src/Billiards/rules/ball_in_hand_authority.h','src/Billiards/game_runtime.h','src/Billiards/game_runtime.cpp','src/Billiards/render/modern/modern_product_session.h','src/Billiards/render/modern/modern_product_session.cpp']
TEST=['tests/game_runtime_tests.cpp','tests/rules_runtime_integration_tests.cpp','tests/rules_decision_session_integration_tests.cpp']

def sha(b:bytes)->str:return hashlib.sha256(b).hexdigest()
def digest(v:Any)->str:return sha(json.dumps(v,sort_keys=True,separators=(',',':')).encode())
def resolve(raw:Any):
 if not isinstance(raw,str):return None
 p=Path(raw)
 if p.is_absolute() or '..' in p.parts:return None
 q=(ROOT/p).resolve();return q if q.is_relative_to(ROOT.resolve()) else None
def binding_ok(v:Any)->bool:
 if not isinstance(v,dict) or set(v)!={'path','sha256'}:return False
 p=resolve(v.get('path'));return p is not None and p.is_file() and sha(p.read_bytes())==v.get('sha256')

def validate(v:Any,check_files:bool=True)->list[str]:
 if not isinstance(v,dict):return ['root_not_object']
 e=[]
 if set(v)!=KEYS:e.append('top_level_keys')
 if v.get('schema')!='wpa_ii_09_coordinate_placement_positive_impulse_lifecycle_implementation_design_gate_v1':e.append('schema')
 if v.get('status')!='DESIGN_COMPLETE_AWAITING_ROOT_IMPLEMENTATION_APPROVAL_NO_PRODUCTION_MUTATION':e.append('status')
 for k,h in DIGESTS.items():
  if digest(v.get(k))!=h:e.append('contract_digest:'+k)
 a=v.get('authority_bindings',{});s=a.get('root_selected_policy',{}) if isinstance(a,dict) else {};o=a.get('original_design_gap_gate',{}) if isinstance(a,dict) else {};u=a.get('current_audit',{}) if isinstance(a,dict) else {}
 if s.get('selected_option_id')!='COORDINATE_PLACEMENT_ONLY_POSITIVE_IMPULSE_CONSUMES' or s.get('sha256')!='fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f':e.append('selection')
 if o.get('sha256')!='848d45fc1d8edd8daec8f55c8a4857a9f9f57d148f64bb6db28a0caf052b615e':e.append('original_gate')
 if u.get('sha256')!='8f6abc94c3cb0c94518b47bd9bde2208744af77678200387064a196f33e85f56' or u.get('must_not_be_promoted_by_gate') is not True:e.append('audit')
 f=v.get('feasibility_finding',{}); f=f if isinstance(f,dict) else {}; pred='application.action.ok && application.contact.applied && isfinite(application.contact.normalImpulseNs) && application.contact.normalImpulseNs > 0.0'
 if f.get('selected_positive_predicate')!=pred or f.get('new_policy_blocker') is not False or f.get('new_solver_or_cue_contact_telemetry_required') is not False:e.append('feasibility')
 c=v.get('coordinate_commit_contract',{});c=c if isinstance(c,dict) else {};p=v.get('positive_impulse_consumption_contract',{});p=p if isinstance(p,dict) else {}
 if 'stage exactly one fresh replacement' not in str(c.get('commit')) or c.get('partial_commit_allowed') is not False:e.append('coordinate_contract')
 if p.get('consume_if_and_only_if')!='the selected positive predicate succeeds on the candidate CueShotApplication' or 'does not undo' not in str(p.get('later_physics_step_failure')) or p.get('partial_commit_allowed') is not False:e.append('impulse_contract')
 x=v.get('unsupported_boundary',{});x=x if isinstance(x,dict) else {}
 if x.get('physical_cue_part_reposition')!='DISABLED_UNSUPPORTED' or x.get('forward_stroke_classifier')!='DISABLED_UNSUPPORTED':e.append('unsupported')
 for k in ('coordinate_preview_or_confirm_is_tip_touch','new_tip_contact_or_forward_stroke_claim','global_rules_decision_ui_product_exposed','foundation_or_sibling_rules_action_product_exposed'):
  if x.get(k) is not False:e.append('unsupported_false:'+k)
 al=v.get('implementation_allowlist',{});al=al if isinstance(al,dict) else {}
 if al.get('production_files')!=PROD or al.get('test_files')!=TEST:e.append('allowlist')
 for k in ('new_production_file_allowed','cue_contact_or_physics_edit_allowed','cmake_change_allowed','visual_ui_or_renderer_allowed'):
  if al.get(k) is not False:e.append('allowlist_false:'+k)
 fx=v.get('required_fixtures',{});fx=fx if isinstance(fx,dict) else {}
 if len(fx.get('positive',[]))!=7 or len(fx.get('negative',[]))!=9 or len(fx.get('regressions',[]))!=4 or fx.get('headless_cpu_only') is not True:e.append('fixtures')
 src=v.get('source_bindings');tst=v.get('test_source_bindings')
 if not isinstance(src,list) or [r.get('path') for r in src if isinstance(r,dict)]!=PROD:e.append('sources')
 if not isinstance(tst,list) or [r.get('path') for r in tst if isinstance(r,dict)]!=TEST:e.append('tests')
 if check_files:
  for rows,label in ((src,'source'),(tst,'test')):
   if isinstance(rows,list):
    for r in rows:
     if not binding_ok(r):e.append(label+'_drift')
  for b,label in ((s,'selection'),(o,'original'),(u,'audit')):
   if not binding_ok({'path':b.get('path'),'sha256':b.get('sha256')}):e.append(label+'_drift')
 cm=v.get('cmake_binding',{});cm=cm if isinstance(cm,dict) else {}
 if cm.get('modified_in_gate') is not False:e.append('cmake')
 elif check_files and not binding_ok({'path':cm.get('path'),'sha256':cm.get('sha256')}):e.append('cmake_drift')
 b=v.get('completion_boundary',{});b=b if isinstance(b,dict) else {};cl=v.get('claims',{});cl=cl if isinstance(cl,dict) else {}
 if b.get('implementation_authorized_by_this_gate') is not False or b.get('root_revalidation_required_before_tests_first_patch') is not True or b.get('gpu_ta_readiness_or_authorization') is not False:e.append('boundary')
 if cl.get('design_complete') is not True or cl.get('feasible_without_new_physics') is not True:e.append('design_claim')
 for k in ('implementation_complete','wpa_ii_09_complete','production_modified','cmake_modified','visual_ui_modified','gpu_executed'):
  if cl.get(k) is not False:e.append('false_completion:'+k)
 return e

def main()->int:
 ap=argparse.ArgumentParser();ap.add_argument('gate',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');a=ap.parse_args()
 try:v=json.loads(Path(a.gate).read_text())
 except (OSError,json.JSONDecodeError) as ex:print('INVALID:',ex);return 1
 e=validate(v,not a.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_WPA_II_09_COORDINATE_PLACEMENT_POSITIVE_IMPULSE_LIFECYCLE_IMPLEMENTATION_DESIGN_GATE');return 0
if __name__=='__main__':raise SystemExit(main())
