#!/usr/bin/env python3
from __future__ import annotations
import argparse,hashlib,json
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
DEFAULT=ROOT/'docs/rules/m4-d5-c-machine-double-hit-classification-policy-root-selected-v1.json'
def sha(b):return hashlib.sha256(b).hexdigest()
def bind(row):
 if not isinstance(row,dict) or set(row)!={'path','sha256'}:return False
 p=Path(row['path'])
 if p.is_absolute() or '..' in p.parts:return False
 q=(ROOT/p).resolve()
 return q.is_relative_to(ROOT.resolve()) and q.is_file() and sha(q.read_bytes())==row['sha256']
def validate(v,files=True):
 if not isinstance(v,dict):return ['root']
 e=[];keys={'schema','selection_id','status','selected_by','selected_on','input_bindings','root_selection_directive','selected_bundle','selection_semantics','implementation_permissions','claims'}
 if set(v)!=keys:e.append('keys')
 if v.get('schema')!='m4_d5_c_machine_double_hit_classification_policy_root_selected_v1' or v.get('selection_id')!='m4-d5-c-root-selected-2026-08-12-v1':e.append('identity')
 if v.get('status')!='ROOT_SELECTED_POLICY_ONLY_IMPLEMENTATION_NOT_AUTHORIZED' or v.get('selected_by')!='ROOT_PM' or v.get('selected_on')!='2026-08-12':e.append('selection')
 exp={'candidate_gate':'cf89e3491e7eac9d41704c73784e625125292bf28526c7044c303be6fd172be9','root_policy_bundle':'fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f','d5_a_receipt':'7a6f7c3c798654d846d6a645451ddfa559a156a32ec869a3d4df4d5a7b2b0bd6','d5_b_receipt':'5d690f3bfbdbee149cae833eec0221e0543216dd963f6e0a634cc3b5b743f8fe'}
 a=v.get('input_bindings',{})
 if set(a)!=set(exp):e.append('bindings')
 else:
  for k,d in exp.items():
   if a[k].get('sha256')!=d:e.append('binding:'+k)
   elif files and not bind(a[k]):e.append('drift:'+k)
 directive=v.get('root_selection_directive',{})
 if directive.get('semantic_sha256')!='be012b449b6c8fc302d679db09eb362ae8f8025f7f187522071e73dec78e98ea' or sha(str(directive.get('semantic_text','')).encode())!=directive.get('semantic_sha256'):e.append('directive')
 b=v.get('selected_bundle',{});c=b.get('constants',{})
 if b.get('id')!='D5C-A_REFEREE_CLOSE_CASES_EXACT_ORDER' or b.get('candidate_order')!=1 or b.get('root_selected') is not True or c!={'very_close_surface_gap_cm':None,'light_graze_normal_impulse_ratio':None,'tip_separation_tie_tolerance_seconds':0.0}:e.append('bundle')
 expected={'automatic_predicate':'complete canonical cue-tip interval count > 1','automatic_result':'DOUBLE_HIT_FOUL','single_interval_signed_close_case_and_raw_before':'SIGNED_REFEREE_REVIEW_REQUIRED','raw_equal':'SIGNED_REFEREE_REVIEW_REQUIRED','light_graze':'SIGNED_REFEREE_REVIEW_REQUIRED','incomplete_evidence':'INCOMPLETE_NO_CLASSIFICATION','single_interval_without_signed_close_case':'NO_DOUBLE_HIT_OBSERVED'}
 for k,x in expected.items():
  if b.get(k)!=x:e.append('predicate:'+k)
 s=v.get('selection_semantics',{})
 if s.get('raw_exact_order_preserved') is not True:e.append('raw')
 for k in ('gap_or_graze_numeric_machine_branch_enabled','equal_rewritten_as_before','signed_referee_review_is_automatic_foul','review_needed_is_foul','selection_is_implementation','selection_closes_wpa_ii_12_5_6_or_15_1','selection_closes_d5_or_m4'):
  if s.get(k) is not False:e.append('semantic:'+k)
 p=v.get('implementation_permissions',{})
 if p.get('independent_implementation_design_gate_may_be_prepared') is not True:e.append('gate_permission')
 for k in ('production_or_cmake_change_allowed_before_gate_revalidation','tests_first_implementation_authorized','rules_or_shotfacts_consumer_authorized','visible_or_global_ui_allowed','gpu_readiness_or_authorization_allowed'):
  if p.get(k) is not False:e.append('overauth:'+k)
 claims=v.get('claims',{})
 if claims.get('policy_selected') is not True:e.append('policy_claim')
 for k in ('implementation_started','machine_double_hit_classification_implemented','audit_row_promoted','d5_complete','m4_complete','production_or_cmake_modified','gpu_executed_or_authorized'):
  if claims.get(k) is not False:e.append('false_claim:'+k)
 return e
def main():
 ap=argparse.ArgumentParser();ap.add_argument('artifact',nargs='?',default=str(DEFAULT));ap.add_argument('--no-file-checks',action='store_true');x=ap.parse_args()
 try:v=json.loads(Path(x.artifact).read_text())
 except Exception as z:print('INVALID:',z);return 1
 e=validate(v,not x.no_file_checks)
 if e:print('INVALID:',', '.join(e));return 1
 print('PASS_M4_D5_C_MACHINE_DOUBLE_HIT_CLASSIFICATION_POLICY_ROOT_SELECTED');return 0
if __name__=='__main__':raise SystemExit(main())
