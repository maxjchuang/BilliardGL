from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_phase5_wpa_heyball_as_built_audit_v7 import validate
ROOT=Path(__file__).resolve().parents[2];A=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v7.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(A.read_text())
 def reject(c,fn):v=copy.deepcopy(c.v);fn(v);c.assertTrue(validate(v,False))
 @staticmethod
 def row(v,i):return next(x for x in v['matrix'] if x['id']==i)
 def test_live(c):c.assertEqual([],validate(c.v,True))
 def test_schema(c):c.reject(lambda v:v.__setitem__('schema_version',6));c.reject(lambda v:v.__setitem__('audit_status','NONFINAL_ROOT_REVIEW_REQUIRED_WPA_II_09_ONLY_SUCCESSOR'))
 def test_predecessor(c):c.reject(lambda v:v['frozen_v6_predecessor'].__setitem__('sha256','0'*64))
 def test_target(c):c.reject(lambda v:c.row(v,'WPA-II-09').__setitem__('status','PARTIAL'));c.reject(lambda v:c.row(v,'WPA-II-09').__setitem__('product_reachable',False));c.reject(lambda v:c.row(v,'WPA-II-09')['gaps'].append('tip'))
 def test_target_immutable(c):c.reject(lambda v:c.row(v,'WPA-II-09').__setitem__('requirement','selected scope only'));c.reject(lambda v:c.row(v,'WPA-II-09').__setitem__('m4_required',False))
 def test_other_39_rows(c):c.reject(lambda v:c.row(v,'WPA-II-08').__setitem__('assessment','rewrite'));c.reject(lambda v:c.row(v,'WPA-II-10-4').__setitem__('status','PARTIAL'))
 def test_summary(c):c.reject(lambda v:v['summary'].__setitem__('implemented',12));c.reject(lambda v:v['summary'].__setitem__('partial',8))
 def test_m4(c):c.reject(lambda v:v['m4_required_unclosed'].append('WPA-II-09'));c.reject(lambda v:v['m4_required_unclosed'].remove('WPA-II-06-C'))
 def test_scope(c):
  for k in ('coordinate_preview_confirm_repeated','strict_positive_authoritative_impulse_consumes_once','zero_rejected_unsupported_miscue_failed_launch_preserve','atomic_idempotent_fail_closed'):c.reject(lambda v,k=k:v['wpa_ii_09_completion']['selected_shipping_scope'].__setitem__(k,False))
 def test_defer_not_hidden(c):c.reject(lambda v:v['wpa_ii_09_completion']['selected_shipping_scope'].__setitem__('physical_cue_part_reposition','IMPLEMENTED'));c.reject(lambda v:v['wpa_ii_09_completion']['selected_shipping_scope'].__setitem__('defer_is_not_physical_implementation_claim',False))
 def test_refs(c):c.reject(lambda v:v['wpa_ii_09_completion']['root_selected_policy'].__setitem__('sha256','0'*64));c.reject(lambda v:v['wpa_ii_09_completion']['implementation_receipt'].__setitem__('sha256','0'*64))
 def test_evidence(c):c.reject(lambda v:c.row(v,'WPA-II-09')['evidence'].pop());c.reject(lambda v:c.row(v,'WPA-II-09')['evidence'][0].__setitem__('sha256','0'*64))
 def test_bindings(c):c.reject(lambda v:v['wpa_ii_09_completion_bindings']['production_and_test_files'].pop());c.reject(lambda v:v['wpa_ii_09_completion_bindings']['production_and_test_files'][0].__setitem__('path','../bad'))
 def test_false_completion(c):
  for k in ('global_product_exposed','profile_complete','m4_complete','production_changed','cmake_changed','gpu_executed','readiness_or_authorization_created'):c.reject(lambda v,k=k:v['wpa_ii_09_completion'].__setitem__(k,True))
 def test_profile(c):c.reject(lambda v:v.__setitem__('profile_complete',True));c.reject(lambda v:v.__setitem__('product_exposed',True))
 def test_order_ids(c):c.reject(lambda v:v['matrix'].reverse());c.reject(lambda v:v['matrix'][0].__setitem__('id','WPA-II-09'))
 def test_bad(c):
  for v in (None,[],1,'x'):c.assertTrue(validate(v,False))
if __name__=='__main__':unittest.main()
