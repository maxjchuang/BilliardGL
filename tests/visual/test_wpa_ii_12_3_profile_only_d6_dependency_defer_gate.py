import copy,importlib.util,json,pathlib,unittest
ROOT=pathlib.Path(__file__).resolve().parents[2];V=ROOT/'scripts/validate_wpa_ii_12_3_profile_only_d6_dependency_defer_gate.py';G=ROOT/'docs/rules/wpa-ii-12-3-profile-only-d6-dependency-defer-gate-v1.json'
S=importlib.util.spec_from_file_location('g',V);M=importlib.util.module_from_spec(S);S.loader.exec_module(M)
class T(unittest.TestCase):
 @classmethod
 def setUpClass(c):c.v=json.loads(G.read_text())
 def bad(self,f):v=copy.deepcopy(self.v);f(v);self.assertTrue(M.validate(v,False))
 def test_valid(self):self.assertEqual(M.validate(self.v,True),[])
 def test_status(self):self.bad(lambda v:v.update(status='IMPLEMENTED'))
 def test_policy(self):self.bad(lambda v:v['authority_bindings']['root_selected_policy'].update(sha256='0'*64))
 def test_d6(self):self.bad(lambda v:v['assessment'].update(d6_required=False))
 def test_fact(self):self.bad(lambda v:v['assessment'].update(profile_declaration_alone_is_fact=True))
 def test_foul(self):self.bad(lambda v:v['assessment'].update(profile_declaration_alone_is_foul=True))
 def test_partial(self):self.bad(lambda v:v['assessment'].update(partial_profile_consumer_recommended=True))
 def test_shortcut(self):self.bad(lambda v:v['deferred_contract'].update(coordinate_or_injected_fact_shortcut_forbidden=False))
 def test_pos(self):self.bad(lambda v:v['required_future_fixtures']['positive'].pop())
 def test_neg(self):self.bad(lambda v:v['required_future_fixtures']['negative'].pop())
 def test_allow(self):self.bad(lambda v:v['implementation_allowlist']['production_files'].append('x'))
 def test_impl(self):self.bad(lambda v:v['completion_boundary'].update(profile_consumer_implemented=True))
 def test_promote(self):self.bad(lambda v:v['completion_boundary'].update(wpa_ii_12_3_promoted=True))
 def test_m4(self):self.bad(lambda v:v['completion_boundary'].update(m4_complete=True))
if __name__=='__main__':unittest.main()
