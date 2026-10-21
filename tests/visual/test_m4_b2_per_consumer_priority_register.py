from __future__ import annotations
import copy,importlib.util,json,sys,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2];V=ROOT/'scripts/validate_m4_b2_per_consumer_priority_register.py';R=ROOT/'docs/rules/m4-b2-per-consumer-priority-register-v1.json'
S=importlib.util.spec_from_file_location('b2_priority',V);assert S and S.loader
M=importlib.util.module_from_spec(S);sys.modules[S.name]=M;S.loader.exec_module(M)
class RegisterTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.v=json.loads(R.read_text())
 def reject(self,fn):
  v=copy.deepcopy(self.v);fn(v);self.assertTrue(M.validate(v,False))
 def test_live(self):self.assertEqual([],M.validate(self.v,True))
 def test_schema_status_extra(self):self.reject(lambda v:v.__setitem__('schema','v2'));self.reject(lambda v:v.__setitem__('status','APPROVED'));self.reject(lambda v:v.__setitem__('implemented',True))
 def test_all_inputs_bound(self):
  for k in self.v['input_bindings']:self.reject(lambda v,k=k:v['input_bindings'][k].__setitem__('sha256','0'*64))
 def test_exact_order(self):self.reject(lambda v:v['ordering_contract']['stable_priority_order'].reverse());self.reject(lambda v:v['rows'].reverse());self.reject(lambda v:v['rows'][0].__setitem__('priority',2))
 def test_root_selected_options_are_exact(self):
  for i in range(8):self.reject(lambda v,i=i:v['rows'][i].__setitem__('selected_option_id','OTHER'))
 def test_d5_dependencies(self):
  for i in (3,4,5,6):self.reject(lambda v,i=i:v['rows'][i]['dependencies'].__setitem__('requires_D5_for_consumer',False))
  for i in (0,1,2,7):self.reject(lambda v,i=i:v['rows'][i]['dependencies'].__setitem__('requires_D5_for_consumer',True))
 def test_d6_dependencies(self):
  for i in (6,7):self.reject(lambda v,i=i:v['rows'][i]['dependencies'].__setitem__('requires_D6_for_consumer',False))
  self.reject(lambda v:v['rows'][0]['dependencies'].__setitem__('requires_D6_for_consumer',True))
 def test_stalemate_then_group_confusion(self):self.reject(lambda v:v['ordering_contract'].__setitem__('first_recommended_consumer','WPA-II-19'));self.reject(lambda v:v['rows'][0].__setitem__('recommended_next',False));self.reject(lambda v:v['rows'][1].__setitem__('recommended_next',True))
 def test_no_consumer_authorization(self):
  for i in range(8):self.reject(lambda v,i=i:v['rows'][i].__setitem__('consumer_implementation_authorized',True))
 def test_blocking_honesty(self):self.reject(lambda v:v['rows'][4].__setitem__('m4_value','CLOSED'));self.reject(lambda v:v['rows'][6].__setitem__('m4_value','COMPLETE'));self.reject(lambda v:v['rows'][7].__setitem__('m4_value','IMPLEMENTED'))
 def test_batches_stay_recommendations(self):self.reject(lambda v:v['execution_batches_recommended_not_selected'][0].__setitem__('state','ROOT_SELECTED'));self.reject(lambda v:v['execution_batches_recommended_not_selected'].pop())
 def test_no_false_completion(self):
  for k in ('root_selected_next_consumer','consumer_implementation_authorized','specific_consumer_registered','production_or_cmake_modified','audit_row_promoted','b2_complete','m4_complete','global_player_rules_ui_exposed','gpu_readiness_or_authorization'):self.reject(lambda v,k=k:v['claims'].__setitem__(k,True))
 def test_malformed(self):
  for v in (None,[],1,'x'):self.assertTrue(M.validate(v,False))
  self.reject(lambda v:v.__setitem__('rows',None));self.reject(lambda v:v.__setitem__('input_bindings',[]));self.reject(lambda v:v.__setitem__('claims','done'))
if __name__=='__main__':unittest.main()
