#!/usr/bin/env python3
"""Independently verify a closed full-evidence native session (not M4 approval)."""
import argparse
import hashlib
import json
import math
from pathlib import Path


def digest(path):
    h = hashlib.sha256()
    with path.open('rb') as stream:
        for chunk in iter(lambda: stream.read(1024*1024), b''):
            h.update(chunk)
    return h.hexdigest()


def audit(directory, binary_sha):
    closing = json.loads((directory/'session-closed.json').read_text())
    if closing['status'] != 'FULL_EVIDENCE_CLOSED' or not closing['retention_complete']:
        raise ValueError('session is not closed full evidence')
    count = int(closing['segment_count'])
    sequence, previous = 0, ''
    kinds, input_before, input_after = {}, [], []
    starts, ends, tick_ranges = [], [], {}
    for segment in range(count):
        receipt_path = directory/f'segment-{segment}.closed.json'
        receipt = json.loads(receipt_path.read_text())
        if (int(receipt['segment']) != segment or receipt['status'] != 'CLOSED'
                or receipt['previous_segment_receipt_sha256'] != previous
                or int(receipt['first_event_sequence']) != sequence):
            raise ValueError(f'receipt lineage broken at {segment}')
        filename = receipt['data_file']
        if Path(filename).name != filename or filename != f'events-{segment}.jsonl':
            raise ValueError('unexpected full-evidence data filename')
        data = directory/filename
        if digest(data) != receipt['data_sha256']:
            raise ValueError(f'data hash mismatch at {segment}')
        records = 0
        with data.open() as stream:
            header = json.loads(next(stream))
            if (header['kind'] != 'segment_start' or int(header['segment']) != segment
                    or int(header['first_event_sequence']) != sequence
                    or header['previous_segment_receipt_sha256'] != previous
                    or header['retention_mode'] != 'full_evidence'):
                raise ValueError('data header lineage mismatch')
            for line in stream:
                row = json.loads(line)
                if int(row['sequence']) != sequence:
                    raise ValueError(f'event sequence gap/duplicate at {sequence}')
                sequence += 1
                records += 1
                kind = row['kind']
                kinds[kind] = kinds.get(kind, 0)+1
                if kind == 'session_start':
                    starts.append(row)
                if kind == 'session_end':
                    ends.append(row)
                if kind in ('input_before', 'input_after'):
                    (input_before if kind == 'input_before' else input_after).append(row)
                if kind == 'physics_step':
                    balls = row['balls']
                    if len(balls) != 16 or sorted(b['id'] for b in balls) != list(range(16)):
                        raise ValueError('physics step missing distinct sixteen balls')
                    for ball in balls:
                        for field in ('position_cm', 'velocity_cm_s', 'angular_velocity_rad_s'):
                            if len(ball[field]) != 3 or not all(math.isfinite(x) for x in ball[field]):
                                raise ValueError('invalid ball motion record')
                    generation, tick = row['journal_generation'], int(row['tick'])
                    current = tick_ranges.setdefault(generation, {'first': tick, 'last': tick-1, 'count': 0})
                    if tick != current['last']+1:
                        raise ValueError(f'physics tick discontinuity in generation {generation}')
                    current['last'], current['count'] = tick, current['count']+1
        if records != int(receipt['event_count']) or sequence != int(receipt['next_event_sequence']):
            raise ValueError('receipt event range/count mismatch')
        previous = digest(receipt_path)
    if (sequence != int(closing['next_event_sequence'])
            or previous != closing['last_segment_receipt_sha256']):
        raise ValueError('final session lineage mismatch')
    if len(starts) != 1 or starts[0]['sequence'] != '0' or len(ends) != 1 or int(ends[0]['sequence']) != sequence-1:
        raise ValueError('missing unique session start/end boundary')
    if binary_sha and starts[0]['details']['executable_sha256'] != binary_sha:
        raise ValueError('native executable binding mismatch')
    if not input_before or len(input_before) != len(input_after):
        raise ValueError('native input boundary pairs missing')
    for before, after in zip(input_before, input_after):
        if int(after['sequence']) <= int(before['sequence']):
            raise ValueError('input pair order mismatch')
        for key in ('kind', 'control', 'x', 'y', 'dx', 'dy', 'modifiers'):
            if before['details'].get(key) != after['details'].get(key):
                raise ValueError(f'input pair payload mismatch: {key}')
    if not tick_ranges:
        raise ValueError('no actual physics tick coverage')
    return {'status': 'PASS_CLOSED_NATIVE_LOG_COVERAGE_ONLY', 'event_count': sequence,
            'segment_count': count, 'kinds': kinds, 'input_pairs': len(input_before),
            'physics_tick_ranges': tick_ranges, 'binary_sha256': binary_sha,
            'not_claimed': ['pocket physics acceptance', 'visual acceptance', 'M4 acceptance',
                            'existence of an input not delivered to the logged native event loop']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('directory', type=Path)
    parser.add_argument('--binary-sha256')
    parser.add_argument('--output', type=Path, required=True)
    args = parser.parse_args()
    try:
        result = audit(args.directory, args.binary_sha256)
    except (ValueError, KeyError, FileNotFoundError, StopIteration) as error:
        result = {'status': 'FAIL_NATIVE_LOG_COVERAGE', 'reason': str(error)}
    with args.output.open('x') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
    print(result['status'])
    raise SystemExit(0 if result['status'].startswith('PASS_') else 1)
