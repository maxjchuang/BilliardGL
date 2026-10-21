import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
V=ROOT/'scripts/validate_wpa_ii_12_7_signed_push_review_declaration_lifecycle_implementation.py';R=ROOT/'docs/rules/wpa-ii-12-7-signed-push-review-declaration-lifecycle-implementation-receipt-v1.json'
S=importlib.util.spec_from_file_location('impl',V);M=importlib.util.module_from_spec(S);S.loader.exec_module(M)
class Tests(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(R.read_text())
 def bad(self,f):v=copy.deepcopy(self.v);f(v);self.assertTrue(M.validate(v,False))
 def test_valid(self):self.assertEqual(M.validate(self.v,True),[])
 def test_status(self):self.bad(lambda v:v.update(status='IMPLEMENTED'))
 def test_gate(self):self.bad(lambda v:v['authority_bindings']['implementation_design_gate'].update(sha256='0'*64))
 def test_policy(self):self.bad(lambda v:v['authority_bindings']['root_selected_policy'].update(selected_option_id='OTHER'))
 def test_red(self):self.bad(lambda v:v['tests_first'].update(red_observed=False))
 def test_review(self):self.bad(lambda v:v['contract'].update(review_needed_only=False))
 def test_measurement(self):self.bad(lambda v:v['contract'].update(measurement_complete=True))
 def test_push(self):self.bad(lambda v:v['contract'].update(push_classified=True))
 def test_foul(self):self.bad(lambda v:v['contract'].update(foul_classified=True))
 def test_threshold(self):self.bad(lambda v:v['contract'].update(thresholds_selected=True))
 def test_commands(self):self.bad(lambda v:v['automation']['commands'].pop())
 def test_positive(self):self.bad(lambda v:v['fixtures']['positive'].pop())
 def test_negative(self):self.bad(lambda v:v['fixtures']['negative'].pop())
 def test_source(self):self.bad(lambda v:v['source_bindings'].pop())
 def test_test(self):self.bad(lambda v:v['test_bindings'].pop())
 def test_binary(self):self.bad(lambda v:v['cpu']['binaries'].pop())
 def test_d5(self):self.bad(lambda v:v['completion'].update(d5_complete=True))
 def test_audit(self):self.bad(lambda v:v['completion'].update(audit_promoted=True))
 def test_m4(self):self.bad(lambda v:v['completion'].update(m4_complete=True))
if __name__=='__main__':unittest.main()
