from __future__ import annotations
import copy,json,unittest
from pathlib import Path
from scripts.validate_phase5_wpa_heyball_as_built_audit_v9 import validate
ROOT=Path(__file__).resolve().parents[2];P=ROOT/'docs/rules/phase5-wpa-heyball-as-built-reconciliation-audit-v9.json'
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(P.read_text())
 def reject(c,fn):v=copy.deepcopy(c.v);fn(v);c.assertTrue(validate(v,False))
 def row(c,v,i):return next(x for x in v['matrix'] if x['id']==i)
 def test_live(c):c.assertEqual([],validate(c.v,True))
 def test_identity(c):c.reject(lambda v:v.__setitem__('audit_status','NONFINAL'));c.reject(lambda v:v.__setitem__('schema_version',10))
 def test_predecessor(c):c.reject(lambda v:v['frozen_v8_predecessor'].__setitem__('sha256','0'*64))
 def test_target(c):c.reject(lambda v:c.row(v,'WPA-II-19').__setitem__('status','PARTIAL'));c.reject(lambda v:c.row(v,'WPA-II-19').__setitem__('product_reachable',False));c.reject(lambda v:c.row(v,'WPA-II-19')['gaps'].append('visible UI'))
 def test_target_immutable(c):c.reject(lambda v:c.row(v,'WPA-II-19').__setitem__('product_action_required',True));c.reject(lambda v:c.row(v,'WPA-II-19')['source']['pages'].append(12))
 def test_other_39(c):c.reject(lambda v:c.row(v,'WPA-II-21').__setitem__('assessment','drift'));c.reject(lambda v:c.row(v,'WPA-II-15-1').__setitem__('status','IMPLEMENTED'))
 def test_summary(c):c.reject(lambda v:v['summary'].__setitem__('implemented',14));c.reject(lambda v:v['summary'].__setitem__('missing',12))
 def test_inventory_plan(c):c.reject(lambda v:v['m4_required_unclosed'].pop());c.reject(lambda v:v['next_deterministic_implementation_plan'].append({'rows':['WPA-II-19']}))
 def test_evidence(c):c.reject(lambda v:c.row(v,'WPA-II-19')['evidence'].pop());c.reject(lambda v:c.row(v,'WPA-II-19')['evidence'][0].__setitem__('sha256','0'*64))
 def test_scope(c):c.reject(lambda v:v['wpa_ii_19_completion']['selected_shipping_scope'].__setitem__('product_reachability_basis','VISIBLE_UI'));c.reject(lambda v:v['wpa_ii_19_completion']['selected_shipping_scope'].__setitem__('visible_ui_not_claimed',False))
 def test_protocol_semantics(c):
  for k in ('authenticated_referee_or_player_propose','same_actor_final','assignment_or_sequence_confusion_only','accepted_positive_shoot_deadline','ordinary_wrong_first_routes_wpa_ii_17','section_17_bih_preserved_until_successful_final','core_e_frozen_original_breaker_and_order','atomic_idempotent_fail_closed'):c.reject(lambda v,k=k:v['wpa_ii_19_completion']['selected_shipping_scope'].__setitem__(k,False))
 def test_refs(c):c.reject(lambda v:v['wpa_ii_19_completion']['implementation_receipt'].__setitem__('sha256','0'*64));c.reject(lambda v:v['wpa_ii_19_completion']['b2_foundation_receipt'].__setitem__('path','../bad'))
 def test_bindings(c):c.reject(lambda v:v['wpa_ii_19_completion_bindings']['exact_live_allowlist_files'].pop());c.reject(lambda v:v['wpa_ii_19_completion_bindings']['exact_live_allowlist_files'][0].__setitem__('sha256','0'*64))
 def test_no_false_completion(c):
  for k in ('global_product_exposed','visible_player_ui_exposed','visible_referee_ui_exposed','profile_complete','m4_complete','production_changed','cmake_changed','gpu_executed','readiness_or_authorization_created'):c.reject(lambda v,k=k:v['wpa_ii_19_completion'].__setitem__(k,True))
 def test_profile_boundary(c):c.reject(lambda v:v.__setitem__('product_exposed',True));c.reject(lambda v:v.__setitem__('profile_complete',True))
 def test_bad(c):
  for v in (None,[],1,'x'):c.assertTrue(validate(v,False))
  c.reject(lambda v:v.__setitem__('matrix',None));c.reject(lambda v:v.__setitem__('wpa_ii_19_completion',[]))
if __name__=='__main__':unittest.main()
