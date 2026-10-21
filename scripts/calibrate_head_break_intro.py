"""Verify the beginner head-break lesson through real public physics."""
import hashlib
import json
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
from scripts.generate_phase4_training_pack import build_scenarios_v2
from scripts.calibrate_phase4_reference_replays import AutomationClient, evaluate_candidate, teaching_range_universe

def main():
    binary = Path(sys.argv[1]).resolve()
    producer = hashlib.sha256(binary.read_bytes()).hexdigest()
    scenario = next(s for s in build_scenarios_v2() if s['id'] == 'launch_head_ball_break_control_v2')
    with tempfile.TemporaryDirectory(prefix='billiardgl-head-intro-') as raw:
        path = Path(raw) / 'lesson.json'
        path.write_text(json.dumps(scenario, sort_keys=True))
        attempts = []
        for candidate in teaching_range_universe(scenario, 1000):
            with AutomationClient(str(binary), mode='headless') as client:
                try:
                    attempts.append(evaluate_candidate(client, path, candidate))
                except Exception as error:
                    attempts.append({'actions': candidate, 'score': {'succeeded': False},
                                     'error': str(error)})
    assert hashlib.sha256(binary.read_bytes()).hexdigest() == producer
    payload = {'binary_sha256': producer, 'scenario': scenario,
               'attempt_count': len(attempts), 'attempts': attempts,
               'all_succeeded': all(a['score']['succeeded'] for a in attempts)}
    Path(sys.argv[2]).write_text(json.dumps(payload, indent=2, sort_keys=True) + '\n')
    print(json.dumps({'attempt_count': len(attempts), 'passed': sum(a['score']['succeeded'] for a in attempts)}))
    if not payload['all_succeeded']:
        for a in attempts:
            if not a['score']['succeeded']: print(json.dumps(a))
        raise SystemExit(1)

if __name__ == '__main__':
    main()
