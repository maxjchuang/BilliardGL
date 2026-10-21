from __future__ import annotations
import copy,importlib.util,json,subprocess,sys,tempfile,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[2]
SCRIPT=ROOT/'scripts/validate_m4_d5_b_cue_object_frozen_cushion_policy_independent_telemetry_implementation_design_gate.py'
GATE=ROOT/'docs/rules/m4-d5-b-cue-object-frozen-cushion-policy-independent-telemetry-implementation-design-gate-v1.json'
spec=importlib.util.spec_from_file_location('validator',SCRIPT);mod=importlib.util.module_from_spec(spec);spec.loader.exec_module(mod)
class TestD5BDesignGate(unittest.TestCase):
 @classmethod
 def setUpClass(cls):cls.base=json.loads(GATE.read_text())
 def reject(self,mutator):
  value=copy.deepcopy(self.base);mutator(value);self.assertTrue(mod.validate(value,False))
 def test_live_validator(self):self.assertEqual(mod.validate(copy.deepcopy(self.base),True),[])
 def test_cli(self):self.assertEqual(subprocess.run([sys.executable,str(SCRIPT)],cwd=ROOT,capture_output=True,text=True).returncode,0)
 def test_rejects_status(self):self.reject(lambda v:v.__setitem__('status','IMPLEMENTED'))
 def test_rejects_d5a_binding(self):self.reject(lambda v:v['authority_bindings']['d5_a_receipt'].__setitem__('sha256','0'*64))
 def test_rejects_wrong_batch(self):self.reject(lambda v:v['authority_bindings']['d5_d6_feasibility'].__setitem__('batch','D5-C'))
 def test_rejects_policy_binding(self):self.reject(lambda v:v['authority_bindings']['root_selected_policy'].__setitem__('sha256','0'*64))
 def test_rejects_declaration_binding(self):self.reject(lambda v:v['authority_bindings']['frozen_cushion_declaration_receipt'].__setitem__('sha256','0'*64))
 def test_rejects_existing_fact_claim(self):self.reject(lambda v:v['current_mapping'].__setitem__('cue_object_contact_before_tip_separation_fact_available',True))
 def test_rejects_time_domain(self):self.reject(lambda v:v['canonical_time_and_identity_contract'].__setitem__('time_domain','physics tick local only'))
 def test_rejects_order(self):self.reject(lambda v:v['canonical_time_and_identity_contract']['required_order_values'].reverse())
 def test_rejects_equal_foul(self):self.reject(lambda v:v['canonical_time_and_identity_contract'].__setitem__('equal_is_measurement_not_foul',False))
 def test_rejects_tolerance(self):self.reject(lambda v:v['canonical_time_and_identity_contract'].__setitem__('tie_tolerance_selected',True))
 def test_rejects_close_policy(self):self.reject(lambda v:v['cue_object_order_observation'].__setitem__('does_not_select_very_close_or_light_graze_policy',False))
 def test_rejects_geometry_declaration(self):self.reject(lambda v:v['frozen_cushion_lifecycle_observation'].__setitem__('prerequisite','geometry starts observation'))
 def test_rejects_jaw(self):self.reject(lambda v:v['frozen_cushion_lifecycle_observation'].__setitem__('jaw_counts_as_straight_cushion',True))
 def test_rejects_cue_pocket(self):self.reject(lambda v:v['frozen_cushion_lifecycle_observation'].__setitem__('cue_ball_pocket_is_not_object_pocket_alternative',False))
 def test_rejects_dedupe_family(self):self.reject(lambda v:v['atomicity_dedupe_and_rollback']['dedupe_keys'].pop('departure'))
 def test_rejects_double_hit_claim(self):self.reject(lambda v:v['classification_boundary'].__setitem__('double_hit_classified',True))
 def test_rejects_frozen_result(self):self.reject(lambda v:v['classification_boundary'].__setitem__('frozen_cushion_legal_or_foul_classified',True))
 def test_rejects_fixture_order(self):self.reject(lambda v:v['required_fixtures']['positive'].reverse())
 def test_rejects_missing_negative(self):self.reject(lambda v:v['required_fixtures']['negative'].pop())
 def test_rejects_prod_allowlist(self):self.reject(lambda v:v['implementation_allowlist']['production_files'].append('src/Billiards/rules/chinese_eight_ball_rules.cpp'))
 def test_rejects_test_allowlist(self):self.reject(lambda v:v['implementation_allowlist']['test_files'].pop())
 def test_rejects_forbidden_order(self):self.reject(lambda v:v['implementation_allowlist']['explicitly_forbidden_files'].reverse())
 def test_rejects_binding_duplicate(self):self.reject(lambda v:v['live_source_bindings'].__setitem__(1,copy.deepcopy(v['live_source_bindings'][0])))
 def test_rejects_implementation_claim(self):self.reject(lambda v:v['completion_boundary'].__setitem__('d5_b_implemented',True))
 def test_rejects_d5_complete(self):self.reject(lambda v:v['completion_boundary'].__setitem__('d5_complete',True))
 def test_rejects_audit(self):self.reject(lambda v:v['completion_boundary'].__setitem__('classification_or_audit_progress',True))
 def test_rejects_gpu(self):self.reject(lambda v:v['completion_boundary'].__setitem__('gpu_readiness_or_authorization',True))
 def test_bad_json(self):
  with tempfile.TemporaryDirectory() as td:
   p=Path(td)/'bad.json';p.write_text('{');self.assertNotEqual(subprocess.run([sys.executable,str(SCRIPT),str(p)],cwd=ROOT,capture_output=True).returncode,0)
if __name__=='__main__':unittest.main()
