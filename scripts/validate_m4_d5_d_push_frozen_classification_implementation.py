#!/usr/bin/env python3
import hashlib, json, pathlib, sys
ROOT=pathlib.Path(__file__).resolve().parents[1]
RECEIPT=ROOT/'docs/rules/m4-d5-d-push-frozen-classification-implementation-receipt-v1.json'
def sha(path): return hashlib.sha256(path.read_bytes()).hexdigest()
def require(ok,msg):
    if not ok: raise ValueError(msg)
def validate(data):
    require(data['schema']=='m4_d5_d_push_frozen_classification_implementation_receipt_v1','schema')
    require(data['status']=='ROOT_SELECTED_D5_D_CPU_VERIFIED_AUDIT_ROWS_OPEN','status')
    for binding in data['authority_bindings'].values():
        require(sha(ROOT/binding['path'])==binding['sha256'],'authority binding')
    require(data['authority_bindings']['root_selected_policy']['bundle']=='D5D-A_SIGNED_REFEREE_EXACT_MEASUREMENTS_STRAIGHT_ONLY','bundle')
    require(data['tests_first']=={'red_observed':True,'red_exit_code':1,'red_failures':['tests/push_classifier_tests.cpp: fatal error: rules/push_classifier.h file not found','tests/frozen_cushion_classifier_tests.cpp: fatal error: rules/frozen_cushion_classifier.h file not found'],'green':True},'red green')
    c=data['contract']
    require(c['duration_threshold_seconds'] is None and c['distance_threshold_cm'] is None and c['comparison_tolerance']==0.0,'report only')
    require(c['signed_same_shot_referee_final_required'] and c['jaw_counts'] is False and c['straight_cushion_velocity_impulse_only'],'selected policy')
    require(c['cue_ball_pocket_is_not_object_pocket'] and c['signed_frozen_does_not_suppress_push'],'boundaries')
    gate=json.loads((ROOT/'docs/rules/m4-d5-d-push-frozen-classification-implementation-design-gate-v1.json').read_text())
    require(data['fixtures']==gate['required_fixtures'],'fixtures')
    require([x['path'] for x in data['source_bindings']]==gate['implementation_allowlist']['production_files'],'prod allowlist')
    require([x['path'] for x in data['test_bindings']]==gate['implementation_allowlist']['test_files'],'test allowlist')
    for b in data['source_bindings']+data['test_bindings']+[data['build_binding']]:
        require(sha(ROOT/b['path'])==b['sha256'],'live '+b['path'])
    cpu=data['cpu']; require(cpu['passed_count']==11 and cpu['failed_count']==0 and len(cpu['binaries'])==11 and cpu['gpu'] is False,'cpu')
    for b in cpu['binaries']:
        require(b['exit_code']==0 and sha(pathlib.Path(b['path']))==b['sha256'],'binary')
    done=data['completion']; require(done['d5_d_push_and_frozen_classification_implemented'],'implemented')
    require(not done['d5_complete'] and not done['audit_rows_changed'] and not done['visible_or_global_ui_modified'] and not done['gpu_readiness_or_authorization'] and not done['m4_complete'],'no false complete')
def main():
    try: validate(json.loads(RECEIPT.read_text()))
    except Exception as exc: print('INVALID_M4_D5_D_IMPLEMENTATION_RECEIPT:',exc); return 1
    print('PASS_M4_D5_D_IMPLEMENTATION_RECEIPT'); return 0
if __name__=='__main__': sys.exit(main())
