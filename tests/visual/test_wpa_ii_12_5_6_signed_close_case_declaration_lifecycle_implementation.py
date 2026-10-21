import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
V=ROOT/'scripts/validate_wpa_ii_12_5_6_signed_close_case_declaration_lifecycle_implementation.py';R=ROOT/'docs/rules/wpa-ii-12-5-6-signed-close-case-declaration-lifecycle-implementation-receipt-v1.json'
S=importlib.util.spec_from_file_location('impl',V);M=importlib.util.module_from_spec(S);S.loader.exec_module(M)
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(R.read_text())
 def bad(self,f):v=copy.deepcopy(self.v);f(v);self.assertTrue(M.validate(v,False))
 def test_valid(self):self.assertEqual(M.validate(self.v,True),[])
 def test_status(self):self.bad(lambda v:v.update(status='IMPLEMENTED'))
 def test_gate(self):self.bad(lambda v:v['authority_bindings']['implementation_design_gate'].update(sha256='0'*64))
 def test_red(self):self.bad(lambda v:v['tests_first_record'].update(red_observed=False))
 def test_states(self):self.bad(lambda v:v['implemented_contract']['states'].pop())
 def test_review(self):self.bad(lambda v:v['implemented_contract'].update(review_needed_only=False))
 def test_double(self):self.bad(lambda v:v['implemented_contract'].update(double_hit_classified=True))
 def test_foul(self):self.bad(lambda v:v['implemented_contract'].update(foul_classified=True))
 def test_geometry(self):self.bad(lambda v:v['implemented_contract'].update(geometry_or_microtrace_attests=True))
 def test_threshold(self):self.bad(lambda v:v['implemented_contract'].update(thresholds_selected=True))
 def test_commands(self):self.bad(lambda v:v['automation_headless']['commands'].pop())
 def test_positive(self):self.bad(lambda v:v['fixture_matrix']['positive'].pop())
 def test_negative(self):self.bad(lambda v:v['fixture_matrix']['negative'].pop())
 def test_source(self):self.bad(lambda v:v['source_bindings'].pop())
 def test_test(self):self.bad(lambda v:v['test_bindings'].pop())
 def test_binary(self):self.bad(lambda v:v['cpu_verification']['binary_bindings'].pop())
 def test_d5(self):self.bad(lambda v:v['completion_boundary'].update(d5_complete=True))
 def test_audit(self):self.bad(lambda v:v['completion_boundary'].update(wpa_ii_12_5_6_audit_row_promoted=True))
 def test_physics(self):self.bad(lambda v:v['completion_boundary'].update(physics_telemetry_shotfacts_rules_topology_or_cue_contact_modified=True))
if __name__=='__main__':unittest.main()
