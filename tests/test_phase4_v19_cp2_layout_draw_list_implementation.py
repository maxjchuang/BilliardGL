import copy, importlib.util, json, pathlib, unittest

ROOT=pathlib.Path(__file__).resolve().parents[1]
SPEC=importlib.util.spec_from_file_location("validator",ROOT/"scripts/validate_phase4_v19_cp2_layout_draw_list_implementation.py")
V=importlib.util.module_from_spec(SPEC); SPEC.loader.exec_module(V)
BASE=json.loads(V.RECEIPT.read_text()) if V.RECEIPT.exists() else {}

class ReceiptTests(unittest.TestCase):
 def test_canonical(self): self.assertEqual(V.validate(replay=True),[])
 def test_fail_closed_mutations(self):
  mutations=[]
  def add(name,fn):
   d=copy.deepcopy(BASE); fn(d); mutations.append((name,d))
  add("schema",lambda d:d.update(schema="bad")); add("status",lambda d:d.update(status="bad"))
  for key in ["transparent_cutout_profiles","named_exclusions","fail_closed_mutations","cp1_inventory_rows","default_inset_on","default_world_off","bilingual_toggle_label","focused_only_focus_primitive","trajectory_semantic_members","trajectory_probes","trajectory_denominator_contribution","cp3_started","gpu_run","readiness_created","authorization_created"]:
   add("contract_"+key,lambda d,k=key:d["verified_contract"].__setitem__(k,"mutated"))
  for role in list(V.PINNED)[:8]:
   add("binding_"+role,lambda d,r=role:next(x for x in d["bindings"] if x["role"]==r).__setitem__("sha256","0"*64))
  add("drop_binding",lambda d:d["bindings"].pop())
  add("cp1_inventory_count",lambda d:d["cp1_canonical_inventory"][0].__setitem__("probeDenominator",30))
  add("cp1_inventory_digest",lambda d:d["cp1_canonical_inventory"][1].__setitem__("orderedProbeDigest","0"*64))
  self.assertGreaterEqual(len(mutations),27)
  for name,doc in mutations:
   with self.subTest(name=name): self.assertTrue(V.validate_document(doc))

if __name__=="__main__": unittest.main()
