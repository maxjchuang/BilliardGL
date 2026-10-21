import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
VALIDATOR=ROOT/'scripts/validate_wpa_ii_15_2_signed_frozen_object_straight_cushion_declaration_lifecycle_implementation.py'
RECEIPT=ROOT/'docs/rules/wpa-ii-15-2-signed-frozen-object-straight-cushion-declaration-lifecycle-implementation-receipt-v1.json'
SPEC=importlib.util.spec_from_file_location('impl',VALIDATOR);MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)
class ReceiptTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.value=json.loads(RECEIPT.read_text())
 def bad(self,fn):
  v=copy.deepcopy(self.value);fn(v);self.assertTrue(MODULE.validate(v,False))
 def test_valid(self):self.assertEqual(MODULE.validate(self.value,True),[])
 def test_status(self):self.bad(lambda v:v.update(status='IMPLEMENTED'))
 def test_gate(self):self.bad(lambda v:v['authority_bindings']['implementation_design_gate'].update(sha256='0'*64))
 def test_policy(self):self.bad(lambda v:v['authority_bindings']['root_selected_policy'].update(selected_option_id='OTHER'))
 def test_red(self):self.bad(lambda v:v['tests_first_record'].update(red_observed=False))
 def test_states(self):self.bad(lambda v:v['implemented_contract']['states'].pop())
 def test_cushions(self):self.bad(lambda v:v['implemented_contract']['straight_cushions'].append('JAW'))
 def test_geometry(self):self.bad(lambda v:v['implemented_contract'].update(geometry_or_contact_telemetry_may_declare=True))
 def test_result(self):self.bad(lambda v:v['implemented_contract'].update(declaration_synthesizes_leave_recontact_cushion_pocket_or_legal_result=True))
 def test_commands(self):self.bad(lambda v:v['automation_headless']['commands'].pop())
 def test_readback(self):self.bad(lambda v:v['automation_headless'].update(readback_result_classified=True))
 def test_positive(self):self.bad(lambda v:v['fixture_matrix']['positive'].pop())
 def test_negative(self):self.bad(lambda v:v['fixture_matrix']['negative'].pop())
 def test_source(self):self.bad(lambda v:v['source_bindings'].pop())
 def test_test(self):self.bad(lambda v:v['test_bindings'].pop())
 def test_binary(self):self.bad(lambda v:v['cpu_verification']['binary_bindings'].pop())
 def test_d5(self):self.bad(lambda v:v['completion_boundary'].update(d5_leave_recontact_and_alternatives_complete=True))
 def test_audit(self):self.bad(lambda v:v['completion_boundary'].update(wpa_ii_15_2_audit_row_promoted=True))
if __name__=='__main__':unittest.main()
