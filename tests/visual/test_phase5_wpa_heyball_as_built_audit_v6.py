from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_phase5_wpa_heyball_as_built_audit_v6 import validate
ROOT=Path(__file__).resolve().parents[2];AUDIT=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v6.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(AUDIT.read_text())
 def reject(c,fn):v=copy.deepcopy(c.v);fn(v);c.assertTrue(validate(v,False))
 @staticmethod
 def row(v,i):return next(x for x in v['matrix'] if x['id']==i)
 def test_live(c):c.assertEqual([],validate(c.v,True))
 def test_schema(c):c.reject(lambda v:v.__setitem__('schema_version',5));c.reject(lambda v:v.__setitem__('audit_scope','full'));c.reject(lambda v:v.__setitem__('audit_status','NONFINAL'))
 def test_predecessor(c):c.reject(lambda v:v['frozen_v5_predecessor'].__setitem__('sha256','0'*64))
 def test_target(c):c.reject(lambda v:c.row(v,'WPA-II-08').__setitem__('status','PARTIAL'));c.reject(lambda v:c.row(v,'WPA-II-08').__setitem__('product_reachable',False));c.reject(lambda v:c.row(v,'WPA-II-08')['gaps'].append('gap'))
 def test_target_semantics_immutable(c):c.reject(lambda v:c.row(v,'WPA-II-08').__setitem__('requirement','rewritten'));c.reject(lambda v:c.row(v,'WPA-II-08').__setitem__('m4_required',False))
 def test_other_rows(c):c.reject(lambda v:c.row(v,'WPA-II-09').__setitem__('status','IMPLEMENTED'));c.reject(lambda v:c.row(v,'WPA-II-06-C').__setitem__('assessment','rewrite'))
 def test_summary(c):c.reject(lambda v:v['summary'].__setitem__('implemented',13));c.reject(lambda v:v['summary'].__setitem__('partial',9))
 def test_m4(c):c.reject(lambda v:v['m4_required_unclosed'].append('WPA-II-08'));c.reject(lambda v:v['m4_required_unclosed'].remove('WPA-II-09'))
 def test_semantics(c):
  for k in c.v['wpa_ii_08_completion']['official_semantics']:c.reject(lambda v,k=k:v['wpa_ii_08_completion']['official_semantics'].__setitem__(k,False))
 def test_receipts(c):c.reject(lambda v:v['wpa_ii_08_completion']['telemetry_receipt'].__setitem__('sha256','0'*64));c.reject(lambda v:v['wpa_ii_08_completion']['all_behind_receipt'].__setitem__('sha256','0'*64));c.reject(lambda v:v['wpa_ii_08_completion']['root_selected_policy'].__setitem__('sha256','0'*64))
 def test_remaining(c):c.reject(lambda v:v['wpa_ii_08_completion']['remaining_gaps'].append('unknown'));c.reject(lambda v:v['wpa_ii_08_completion'].__setitem__('other_rows_changed',True))
 def test_false_global(c):
  for k in ('global_product_exposed','profile_complete','m4_complete','production_changed','cmake_changed','gpu_executed','readiness_or_authorization_created'):c.reject(lambda v,k=k:v['wpa_ii_08_completion'].__setitem__(k,True))
 def test_profile(c):c.reject(lambda v:v.__setitem__('profile_complete',True));c.reject(lambda v:v.__setitem__('product_exposed',True))
 def test_evidence(c):c.reject(lambda v:c.row(v,'WPA-II-08')['evidence'].pop());c.reject(lambda v:c.row(v,'WPA-II-08')['evidence'][0].__setitem__('sha256','0'*64))
 def test_bindings(c):c.reject(lambda v:v['wpa_ii_08_completion_bindings']['production_and_test_files'].pop());c.reject(lambda v:v['wpa_ii_08_completion_bindings']['production_and_test_files'][0].__setitem__('path','../bad'))
 def test_order_ids(c):c.reject(lambda v:v['matrix'].reverse());c.reject(lambda v:v['matrix'][0].__setitem__('id','WPA-II-08'))
 def test_bad(c):
  for v in (None,[],1,'x'):c.assertTrue(validate(v,False))
if __name__=='__main__':unittest.main()
