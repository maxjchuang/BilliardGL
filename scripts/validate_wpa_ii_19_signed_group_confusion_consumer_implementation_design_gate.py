#!/usr/bin/env python3
"""Fail-closed validator for the design-only WPA-II-19 C2 consumer gate."""
from __future__ import annotations
import argparse, hashlib, json
from pathlib import Path
from typing import Any

ROOT = Path(__file__).resolve().parents[1]
DEFAULT = ROOT / "docs/rules/wpa-ii-19-signed-group-confusion-consumer-implementation-design-gate-v1.json"
KEYS = {"schema", "status", "authority_bindings", "consumer_identity_and_selected_policy", "window_and_snapshot_contract", "exact_payload_schemas", "state_machine_and_stop_contract", "rack_bih_and_order_contract", "atomicity_idempotency_and_reset", "automation_headless_contract", "implementation_allowlist", "required_fixtures", "source_bindings", "test_source_bindings", "completion_boundary"}
DIGESTS = {
 "authority_bindings":"c682889551d999141f44932d1f2bdcaeac83105747645feab364f5dbafd5d1f2", "consumer_identity_and_selected_policy":"4468798d6f41ff41fc58df3b71125c66dae01daa5797fd52425eade3b769abc4", "window_and_snapshot_contract":"5d92f88ab9382d0ab4b26d295677408b6d516d43ca3069f8a4a6a2bbfbf2b0de", "exact_payload_schemas":"5998c42e85a4d1429e384e75a7383aa9afe8a9e8aea466e8b47e6c393872bdaf", "state_machine_and_stop_contract":"8103d08000fe88d2ece71848b9af95c96d6080e2af66649e801e59c9a87caf63", "rack_bih_and_order_contract":"99d16e9d95f02b790466bb8eaa3d199a2d8360bcc6ddcf430ac722f316e58da1", "atomicity_idempotency_and_reset":"c5922361dde8a10d6f6b05077968e76f2e552179c86a0c1d1fdcc5065abec49f", "automation_headless_contract":"0fd10a6293329bf3e84f85a216e52b792b1d58657b24065c4da07cbfeb3fe092", "implementation_allowlist":"96787643f443dd32fccff08f5037faeb2f576e2d5845c9ae665395eae5784e1b", "required_fixtures":"175533be0654873f307fe6a7c7a931edd206b1b15cbd7473c07b98333be419ea", "source_bindings":"d551612aa0ee1f3723791c6575337050d0e692698c7ea98725e2bbe67043d503", "test_source_bindings":"7e16f082e3c2846defde0db0885f9545f4f1ecfd86f996362f213a76855190da", "completion_boundary":"8d3b6615637b3ccaed22e044b1b70c1140bfff47c1048215129a835cdcf46bf7"}
PROD = ["src/Billiards/rules/group_confusion_authority.h", "src/Billiards/rules/group_confusion_authority.cpp", "src/Billiards/game_runtime.h", "src/Billiards/game_runtime.cpp", "src/Billiards/automation_controller.cpp"]
TEST = ["tests/group_confusion_authority_tests.cpp", "tests/rules_runtime_integration_tests.cpp", "tests/automation_controller_tests.cpp"]

def sha(data: bytes) -> str: return hashlib.sha256(data).hexdigest()
def digest(value: Any) -> str: return sha(json.dumps(value, sort_keys=True, separators=(",", ":")).encode())
def resolve(raw: Any):
 if not isinstance(raw, str): return None
 p=Path(raw)
 if p.is_absolute() or ".." in p.parts: return None
 q=(ROOT/p).resolve()
 return q if q.is_relative_to(ROOT.resolve()) else None
def binding_ok(value: Any) -> bool:
 if not isinstance(value, dict) or set(value)!={"path","sha256"}: return False
 p=resolve(value.get("path")); return p is not None and p.is_file() and sha(p.read_bytes())==value.get("sha256")

def validate(v: Any, check_files: bool=True) -> list[str]:
 if not isinstance(v, dict): return ["root_not_object"]
 e=[]
 if set(v)!=KEYS: e.append("top_level_keys")
 if v.get("schema")!="wpa_ii_19_signed_group_confusion_consumer_implementation_design_gate_v1": e.append("schema")
 if v.get("status")!="DESIGN_COMPLETE_AWAITING_ROOT_EXACT_ALLOWLIST_APPROVAL_NO_PRODUCTION_MUTATION": e.append("status")
 for k,h in DIGESTS.items():
  if digest(v.get(k))!=h: e.append("contract_digest:"+k)
 a=v.get("authority_bindings",{}); a=a if isinstance(a,dict) else {}
 expected={"root_selected_policy":"fff40b72776b4381eae82044421432c139606d300f7214fb87e63e36eeaa770f", "b2_foundation_receipt":"1f654b7e407d34b362430fb83b53f00dfd3598742c5553c815231517727f914b", "b2_priority_register":"8a778fc249a2ba952ff6acca60c5d24cad6ca2135f606bb54e3e0b887d092f55", "original_design_gap_gate":"8bce41f79ba5c690d9846dc0d36c015c5fec1ae580a5ef8c91855029d2f74788", "wpa_ii_21_d8_predecessor_receipt":"0829217fd580d0302ee96d6b4068535f03632420d53f0ae6ca1bb4f2b86644e2", "audit_v8":"ddeabaf87e43647588b321e599fbdf56a3429eb1ad990425cdb9bcf5a9339c94"}
 for k,h in expected.items():
  b=a.get(k,{}) if isinstance(a,dict) else {}
  if b.get("sha256")!=h: e.append("authority:"+k)
  elif check_files and not binding_ok({"path":b.get("path"),"sha256":b.get("sha256")}): e.append("authority_drift:"+k)
 if a.get("root_c2_selection_semantic_sha256")!="42b99af76b1a9d3f9e2b4d92237719f0e8221d0b72665d541a2bac62fe4c9d2f": e.append("root_selection")
 c=v.get("consumer_identity_and_selected_policy",{}); c=c if isinstance(c,dict) else {}
 if c.get("consumer_id")!="WPA-II-19_SIGNED_GROUP_CONFUSION_CONSUMER" or c.get("classification")!="ASSIGNMENT_OR_SEQUENCE_CONFUSION_ONLY": e.append("identity")
 if c.get("accepted_proposal_actor_roles")!=["REFEREE","PLAYER_0","PLAYER_1"] or c.get("final_actor_role")!="SAME_AUTHENTICATED_ROLE_AND_SUBJECT_AS_PROPOSAL": e.append("actors")
 for k in ("cancel_supported","ordinary_wrong_first_contact_alone_opens_or_commits_section_19"):
  if c.get(k) is not False: e.append("policy_false:"+k)
 if c.get("deadline")!="BEFORE_NEXT_ACCEPTED_SHOOT" or c.get("rerack_breaker")!="FROZEN_ORIGINAL_RACK_BREAKER" or c.get("shooting_order")!="PRESERVE_ORIGINAL_RACK_ORDER": e.append("selected_policy")
 w=v.get("window_and_snapshot_contract",{}); w=w if isinstance(w,dict) else {}
 if len(w.get("window_reference",[]))!=12 or len(w.get("does_not_close_window",[]))!=9 or w.get("accepted_launch_later_physics_failure_reopens_previous_window") is not False or w.get("automatic_classification_from_first_contact_or_group_assignment") is not False or w.get("all_snapshot_fields_enter_authority_state_hash") is not True: e.append("window")
 p=v.get("exact_payload_schemas",{}); p=p if isinstance(p,dict) else {}; proposal=p.get("proposal",{}) if isinstance(p,dict) else {}; final=p.get("final",{}) if isinstance(p,dict) else {}
 if proposal.get("exact_keys_in_canonical_order")!=["assignment_snapshot_sha256","claimed_confusion_kind","completed_shot_identity","realization_attested","window_generation"] or proposal.get("ordinary_wrong_first_contact_only_payload_allowed") is not False: e.append("proposal_payload")
 if len(final.get("exact_keys_in_canonical_order",[]))!=7 or final.get("same_authenticated_actor_required") is not True or p.get("cancel",{}).get("supported") is not False: e.append("final_payload")
 sm=v.get("state_machine_and_stop_contract",{}); sm=sm if isinstance(sm,dict) else {}
 if len(sm.get("states",[]))!=6 or sm.get("partial_commit_allowed") is not False or "does not rerack or clear" not in str(sm.get("proposal")): e.append("state_machine")
 rack=v.get("rack_bih_and_order_contract",{}); rack=rack if isinstance(rack,dict) else {}; core=rack.get("core_e_binding",{}) if isinstance(rack,dict) else {}
 if core.get("direct_caller_exposure_allowed") is not False or rack.get("rerack_uses_current_player") is not False or rack.get("foundation_action_is_consumer_action") is not False or rack.get("numbered_seed_order_is_normative") is not False: e.append("rack_boundary")
 if check_files:
  for k in ("header","source"):
   if not binding_ok(core.get(k)): e.append("core_e_drift")
 atom=v.get("atomicity_idempotency_and_reset",{}); atom=atom if isinstance(atom,dict) else {}
 if len(atom.get("prevalidate",[]))!=11 or "zero" not in str(atom.get("exact_retry")) or "rolls back" not in str(atom.get("failed_core_e_plan_or_parent_commit")) or atom.get("plain_turn_change_or_legal_pocket_resets_window") is not False: e.append("atomicity")
 auto=v.get("automation_headless_contract",{}); auto=auto if isinstance(auto,dict) else {}
 if auto.get("commands")!=["rules.group_confusion_propose","rules.group_confusion_final","get_group_confusion_authority_state"] or auto.get("headless_only") is not True or auto.get("modern_product_session_or_input_adapter_exposed") is not False or auto.get("global_rules_decision_ui_product_exposed") is not False or auto.get("visible_signed_realization_ui_requires_separate_gate") is not True: e.append("automation")
 allow=v.get("implementation_allowlist",{}); allow=allow if isinstance(allow,dict) else {}
 if allow.get("production_files")!=PROD or allow.get("test_files")!=TEST or allow.get("build_files")!=["CMakeLists.txt"]: e.append("allowlist")
 if allow.get("referee_protocol_rack_executor_chinese_rules_shotfacts_physics_edit_allowed") is not False or allow.get("modern_product_ui_renderer_input_edit_allowed") is not False: e.append("forbidden_allowlist")
 fx=v.get("required_fixtures",{}); fx=fx if isinstance(fx,dict) else {}
 if len(fx.get("positive",[]))!=10 or len(fx.get("negative",[]))!=18 or fx.get("headless_cpu_only") is not True: e.append("fixtures")
 src=v.get("source_bindings"); tst=v.get("test_source_bindings")
 expected_src=["CMakeLists.txt","src/Billiards/rules/referee_authority_protocol.h","src/Billiards/rules/referee_authority_protocol.cpp","src/Billiards/game_runtime.h","src/Billiards/game_runtime.cpp","src/Billiards/automation_controller.cpp","src/Billiards/rules/rack_executor.h","src/Billiards/rules/rack_executor.cpp"]
 expected_tst=["tests/referee_authority_protocol_tests.cpp","tests/rules_runtime_integration_tests.cpp","tests/automation_controller_tests.cpp"]
 if not isinstance(src,list) or [x.get("path") for x in src if isinstance(x,dict)]!=expected_src: e.append("sources")
 if not isinstance(tst,list) or [x.get("path") for x in tst if isinstance(x,dict)]!=expected_tst: e.append("tests")
 if check_files:
  for rows,label in ((src,"source"),(tst,"test")):
   if isinstance(rows,list):
    for row in rows:
     if not binding_ok(row): e.append(label+"_drift")
 b=v.get("completion_boundary",{}); b=b if isinstance(b,dict) else {}
 if b.get("root_revalidation_required_before_tests_first_patch") is not True: e.append("root_gate")
 for k in ("implementation_authorized_by_this_gate","wpa_ii_19_implemented","audit_row_promoted","b2_complete","m4_complete","production_modified","cmake_modified","visible_product_ui_or_global_exposure","gpu_readiness_or_authorization"):
  if b.get(k) is not False: e.append("false_completion:"+k)
 return e

def main() -> int:
 p=argparse.ArgumentParser(); p.add_argument("gate",nargs="?",default=str(DEFAULT)); p.add_argument("--no-file-checks",action="store_true"); a=p.parse_args()
 try: v=json.loads(Path(a.gate).read_text())
 except (OSError,json.JSONDecodeError) as ex: print("INVALID:",ex); return 1
 errors=validate(v,not a.no_file_checks)
 if errors: print("INVALID:",", ".join(errors)); return 1
 print("PASS_WPA_II_19_SIGNED_GROUP_CONFUSION_CONSUMER_IMPLEMENTATION_DESIGN_GATE"); return 0
if __name__=="__main__": raise SystemExit(main())
