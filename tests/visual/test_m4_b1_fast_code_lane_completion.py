from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_m4_b1_fast_code_lane_completion import validate
ROOT=Path(__file__).resolve().parents[2];R=ROOT/'docs/rules/m4-b1-fast-code-lane-completion-receipt-v1.json';S=ROOT/'docs/rules/m4-remaining-gap-dependency-critical-path-register-v2-b1-successor.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.r=json.loads(R.read_text());c.s=json.loads(S.read_text())
 def reject(c,fn):r=copy.deepcopy(c.r);s=copy.deepcopy(c.s);fn(r,s);c.assertTrue(validate(r,s,False))
 def test_live(c):c.assertEqual([],validate(c.r,c.s,True))
 def test_identity(c):c.reject(lambda r,s:r.__setitem__('status','M4_COMPLETE'));c.reject(lambda r,s:s.__setitem__('status','B2_AUTHORIZED'))
 def test_row_loss_order(c):c.reject(lambda r,s:r['rows'].pop());c.reject(lambda r,s:r['rows'].reverse())
 def test_row_status(c):c.reject(lambda r,s:r['rows'][0].__setitem__('audit_status','IMPLEMENTED'));c.reject(lambda r,s:r['rows'][4].__setitem__('product_reachable',False))
 def test_receipt_inventory(c):c.reject(lambda r,s:r['rows'][0]['receipts'].pop());c.reject(lambda r,s:r['rows'][5]['receipts'][0].__setitem__('sha256','0'*64))
 def test_batch_not_m4(c):c.reject(lambda r,s:r['batch_assessment'].__setitem__('batch_completed_is_not_m4_completion',False));c.reject(lambda r,s:r['claims'].__setitem__('m4_complete',True))
 def test_all_six_not_false(c):c.reject(lambda r,s:r['claims'].__setitem__('b1_batch_completed',False));c.reject(lambda r,s:r['rows'][2].__setitem__('batch_code_complete',False))
 def test_predecessor(c):c.reject(lambda r,s:s['predecessor'].__setitem__('sha256','0'*64));c.reject(lambda r,s:s['predecessor'].__setitem__('modified',True))
 def test_d3_not_fully_satisfied(c):c.reject(lambda r,s:next(x for x in s['dependency_state_successor'] if x['id']=='D3-BREAK-CLASSIFICATION').__setitem__('successor_state','SATISFIED'));c.reject(lambda r,s:s['claims'].__setitem__('d3_fully_satisfied',True))
 def test_d4_satisfied(c):c.reject(lambda r,s:next(x for x in s['dependency_state_successor'] if x['id']=='D4-PLACEMENT-LIFECYCLE').__setitem__('successor_state','PARTIAL'));c.reject(lambda r,s:s['claims'].__setitem__('d4_satisfied',False))
 def test_b2_waits(c):c.reject(lambda r,s:next(x for x in s['dependency_state_successor'] if x['id']=='D2-REFEREE-AUTHORITY-PROTOCOL').__setitem__('successor_state','SATISFIED'));c.reject(lambda r,s:s['batch_state_successor'][2].__setitem__('may_start_from_this_successor',True))
 def test_b3_not_complete(c):c.reject(lambda r,s:s['claims'].__setitem__('b3_complete_or_authorized',True));c.reject(lambda r,s:s['dependency_state_successor'][5].__setitem__('successor_state','SATISFIED'))
 def test_no_external_work(c):
  for k in ('production_modified','cmake_modified','gpu_readiness_or_authorization'):c.reject(lambda r,s,k=k:s['claims'].__setitem__(k,True))
 def test_bindings(c):c.reject(lambda r,s:s['new_bindings']['audit_v7'].__setitem__('sha256','0'*64));c.reject(lambda r,s:r['bindings']['root_selected_policy'].__setitem__('sha256','0'*64))
 def test_bad(c):
  for x in (None,[],1,'x'):c.assertTrue(validate(x,c.s,False));c.assertTrue(validate(c.r,x,False))
if __name__=='__main__':unittest.main()
