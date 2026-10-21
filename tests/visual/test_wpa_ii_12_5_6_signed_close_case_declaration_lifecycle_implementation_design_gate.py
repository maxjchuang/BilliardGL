import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2]
VALIDATOR=ROOT/'scripts/validate_wpa_ii_12_5_6_signed_close_case_declaration_lifecycle_implementation_design_gate.py'
GATE=ROOT/'docs/rules/wpa-ii-12-5-6-signed-close-case-declaration-lifecycle-implementation-design-gate-v1.json'
SPEC=importlib.util.spec_from_file_location('gate',VALIDATOR);MODULE=importlib.util.module_from_spec(SPEC);SPEC.loader.exec_module(MODULE)
class GateTests(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.value=json.loads(GATE.read_text())
 def bad(self,fn):
  v=copy.deepcopy(self.value);fn(v);self.assertTrue(MODULE.validate(v,False))
 def test_valid(self):self.assertEqual(MODULE.validate(self.value,True),[])
 def test_status(self):self.bad(lambda v:v.update(status='IMPLEMENTED'))
 def test_policy(self):self.bad(lambda v:v['authority_bindings']['root_selected_policy'].update(selected_option_id='OTHER'))
 def test_priority(self):self.bad(lambda v:v['authority_bindings']['b2_priority_register'].update(sha256='0'*64))
 def test_threshold(self):self.bad(lambda v:v['official_and_selected_boundary']['selected_machine_constants'].update(very_close_distance_threshold=0.1))
 def test_graze(self):self.bad(lambda v:v['official_and_selected_boundary']['selected_machine_constants'].update(light_graze_threshold=0.1))
 def test_proposal(self):self.bad(lambda v:v['declaration_protocol_contract'].update(proposal_not_attestation=False))
 def test_final_result(self):self.bad(lambda v:v['declaration_protocol_contract'].update(final_does_not_attest_double_hit_or_foul=False))
 def test_geometry(self):self.bad(lambda v:v['declaration_protocol_contract'].update(geometry_microtrace_or_single_launch_contact_may_attest=True))
 def test_payload(self):self.bad(lambda v:v['exact_payload_schemas']['forbidden_payload_fields'].pop())
 def test_d5(self):self.bad(lambda v:v['d5_and_classification_blocker'].update(d5_complete=True))
 def test_physics_fact(self):self.bad(lambda v:v['d5_and_classification_blocker'].update(declaration_is_physics_fact=True))
 def test_foul(self):self.bad(lambda v:v['d5_and_classification_blocker'].update(declaration_is_double_hit_or_foul_result=True))
 def test_allowlist(self):self.bad(lambda v:v['implementation_allowlist']['production_files'].append('src/Billiards/physics.cpp'))
 def test_forbidden_telemetry(self):self.bad(lambda v:v['implementation_allowlist'].update(physics_telemetry_shotfacts_rules_topology_cue_contact_or_coupled_contact_edit_allowed=True))
 def test_positive(self):self.bad(lambda v:v['required_fixtures']['positive'].pop())
 def test_negative(self):self.bad(lambda v:v['required_fixtures']['negative'].pop())
 def test_source(self):self.bad(lambda v:v['source_bindings'].pop())
 def test_implementation_auth(self):self.bad(lambda v:v['completion_boundary'].update(implementation_authorized_by_this_gate=True))
 def test_audit(self):self.bad(lambda v:v['completion_boundary'].update(audit_row_promoted=True))
if __name__=='__main__':unittest.main()
