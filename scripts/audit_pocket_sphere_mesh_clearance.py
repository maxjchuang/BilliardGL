#!/usr/bin/env python3
"""Read-only sphere clearance against actual frozen felt triangles, in cm.

This is a geometry diagnostic, never a physics or player-input PASS receipt.
Only identity-transform glTF geometry is accepted; unsupported assets fail closed.
"""
import argparse
import base64
import hashlib
import json
import math
from pathlib import Path
import struct


def sub(a, b):
    return tuple(x-y for x, y in zip(a, b))


def dot(a, b):
    return sum(x*y for x, y in zip(a, b))


def cross(a, b):
    return (a[1]*b[2]-a[2]*b[1], a[2]*b[0]-a[0]*b[2], a[0]*b[1]-a[1]*b[0])


def segment_distance(p, a, b):
    ab = sub(b, a)
    t = max(0, min(1, dot(sub(p, a), ab)/dot(ab, ab))) if dot(ab, ab) else 0
    return math.sqrt(sum((p[i]-a[i]-t*ab[i])**2 for i in range(3)))


def triangle_distance(p, a, b, c):
    ab, ac = sub(b, a), sub(c, a)
    n = cross(ab, ac)
    nn = dot(n, n)
    if nn:
        signed = dot(sub(p, a), n)/nn
        q = tuple(p[i]-signed*n[i] for i in range(3))
        aq = sub(q, a)
        aa, bb, cc = dot(ab, ab), dot(ab, ac), dot(ac, ac)
        det = aa*cc-bb*bb
        u = (cc*dot(aq, ab)-bb*dot(aq, ac))/det
        v = (aa*dot(aq, ac)-bb*dot(aq, ab))/det
        if u >= -1e-10 and v >= -1e-10 and u+v <= 1+1e-10:
            return abs(signed)*math.sqrt(nn)
    return min(segment_distance(p, a, b), segment_distance(p, b, c), segment_distance(p, c, a))


def felt_triangles(path):
    g = json.loads(path.read_text())
    # Reject transforms on selected nodes and ancestors, not unrelated balls.
    parents = {}
    for i, node in enumerate(g['nodes']):
        for child in node.get('children', []):
            if child in parents:
                raise ValueError('multiple node parents unsupported')
            parents[child] = i
    for i, node in enumerate(g['nodes']):
        if not node.get('name', '').startswith('NODE_table_felt_LOD'):
            continue
        seen = set()
        while True:
            if i in seen:
                raise ValueError('node hierarchy cycle')
            seen.add(i)
            if any(k in g['nodes'][i] for k in ('matrix', 'translation', 'rotation', 'scale')):
                raise ValueError('transformed felt glTF unsupported; export world-space geometry first')
            if i not in parents:
                break
            i = parents[i]
    buffers = []
    for b in g['buffers']:
        uri = b['uri']
        buffers.append(base64.b64decode(uri.split(',', 1)[1]) if uri.startswith('data:') else (path.parent/uri).read_bytes())

    def accessor(index):
        a = g['accessors'][index]
        if 'sparse' in a:
            raise ValueError('sparse accessor unsupported')
        view = g['bufferViews'][a['bufferView']]
        fmt = {5126: 'f', 5125: 'I', 5123: 'H', 5121: 'B'}[a['componentType']]
        count = {'VEC3': 3, 'SCALAR': 1}[a['type']]
        size = struct.calcsize('<'+fmt*count)
        start = view.get('byteOffset', 0)+a.get('byteOffset', 0)
        stride = view.get('byteStride', size)
        data = buffers[view['buffer']]
        return [struct.unpack_from('<'+fmt*count, data, start+i*stride) for i in range(a['count'])]

    result = {}
    for node in g['nodes']:
        name = node.get('name', '')
        if not name.startswith('NODE_table_felt_LOD'):
            continue
        triangles = []
        for primitive in g['meshes'][node['mesh']]['primitives']:
            if primitive.get('mode', 4) != 4:
                raise ValueError('only TRIANGLES accepted')
            positions = [tuple(v*100 for v in p) for p in accessor(primitive['attributes']['POSITION'])]
            indices = [x[0] for x in accessor(primitive['indices'])]
            if len(indices)%3:
                raise ValueError('invalid triangle index count')
            triangles.extend(tuple(positions[j] for j in indices[i:i+3]) for i in range(0, len(indices), 3))
        result[name] = triangles
    if len(result) != 3:
        raise ValueError('requires all three felt LODs')
    return result


def audit(gltf, trace, tolerance):
    meshes = felt_triangles(gltf)
    fixtures = json.loads(trace.read_text())['fixtures']
    findings = []
    for f in fixtures:
        radius = f['radius_cm']
        samples = []
        for s in f['steps']:
            if not s['pocketed']:
                samples.append((s['tick'], 'actual_state', s['position_cm']))
            for contact in s['contacts']:
                if contact.get('pocket_event') == 'capture':
                    samples.append((s['tick'], 'capture_contact_before_storage', contact['position_at_contact_cm']))
        lods = {}
        for name, triangles in meshes.items():
            worst = None
            capture = []
            for tick, kind, position in samples:
                clearance = min(triangle_distance(position, *tri) for tri in triangles)-radius
                record = {'tick': tick, 'kind': kind, 'center_cm': position, 'clearance_cm': clearance}
                if worst is None or clearance < worst['clearance_cm']:
                    worst = record
                if kind.startswith('capture'):
                    # At capture the sphere must be wholly below the cloth and clear.
                    record['whole_sphere_below_cloth'] = position[1]+radius < f['cloth_y_cm']-tolerance
                    capture.append(record)
            lods[name] = {'triangles': len(triangles), 'minimum_clearance': worst, 'capture_contacts': capture}
        findings.append({'pocket_id': f['pocket_id'], 'lods': lods})
    return {'schema': 'pocket_sphere_felt_clearance_diagnostic_v1', 'status': 'DIAGNOSTIC_ONLY_NOT_ACCEPTANCE',
            'gltf_sha256': hashlib.sha256(gltf.read_bytes()).hexdigest(),
            'trace_sha256': hashlib.sha256(trace.read_bytes()).hexdigest(),
            'tolerance_cm': tolerance, 'findings': findings,
            'limitations': ['Felt surface triangles only; slate sidewalls, jaws, leather and sub-tick sweeps require separate checks.',
                            'Tangency to supported felt is permitted before entry; capture clearance alone does not prove descent.',
                            'Initial-state fixture is not a player-shot receipt.']}


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--gltf', type=Path, required=True)
    parser.add_argument('--trace', type=Path, required=True)
    parser.add_argument('--output', type=Path, required=True)
    parser.add_argument('--tolerance-cm', type=float, default=0.01)
    args = parser.parse_args()
    result = audit(args.gltf, args.trace, args.tolerance_cm)
    with args.output.open('x') as output:
        json.dump(result, output, indent=2)
        output.write('\n')
    print('Geometry diagnostic written; no acceptance inferred.')
