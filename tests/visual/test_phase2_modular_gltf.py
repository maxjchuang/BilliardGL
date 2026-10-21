import base64
import copy
from collections import Counter
import json
import math
from pathlib import Path
import struct
import subprocess
import sys
import tempfile
import unittest


REPOSITORY_ROOT = Path(__file__).resolve().parents[2]
ASSET_DIR = REPOSITORY_ROOT / "assets/modern/phase2"
ASSET = ASSET_DIR / "focused_practice_hall_modular_v1.gltf"
MANIFEST = ASSET_DIR / "focused_practice_hall_modular_v1.asset.json"
ATLAS = ASSET_DIR / "focused_practice_hall_ball_identity_atlas_v1.png"
SVG_SOURCE = ASSET_DIR / "focused_practice_hall_ball_identity_atlas_v1.svg"
CLOSEUP = ASSET_DIR / "focused_practice_hall_ball_identity_closeup_1_8_11_15_v2.png"
GENERATOR = REPOSITORY_ROOT / "scripts/generate_phase2_modular_gltf.py"
VALIDATOR = REPOSITORY_ROOT / "scripts/validate_phase2_modular_gltf.py"

sys.path.insert(0, str(REPOSITORY_ROOT / "scripts"))
from validate_phase2_modular_gltf import validate  # noqa: E402
from generate_m1_graybox_gltf import POCKETS  # noqa: E402
from generate_phase2_modular_gltf import (  # noqa: E402
    BALL_DIAMETER_M,
    PLAY_SURFACE_Y_M,
    POCKET_BACK_LIP_DROP_M,
    POCKET_BACK_LIP_RECESS_RATIO,
    POCKET_BACK_LIP_WIDTH_RATIO,
    POCKET_CLOSED_BOWL_CONTRACTION,
    POCKET_BODY_TOP_Y_M,
    POCKET_SHOULDER_TOP_Y_M,
    POCKET_RECESSED_BODY_APERTURE_UNDERCUT_M,
    POCKET_LINER_WIDTH_M,
    POCKET_PHYSICS_CAVITY_DROP_M,
    POCKET_PHYSICS_THROAT_DEPTH_M,
    POCKET_PHYSICS_VOID_BOTTOM_M,
    POCKET_RECESSED_BACK_LIP_Y_M,
    POCKET_RECESSED_BAG_BOTTOM_M,
    POCKET_RECESSED_BACK_WALL_Y_M,
    POCKET_RECESSED_CAPTURE_WALL_Y_M,
    POCKET_RECESSED_JAW_THICKNESS_M,
    POCKET_RECESSED_DEEP_RING_Y_M,
    POCKET_RECESSED_LIP_DESCENT_M,
    POCKET_VISUAL_SOLID_CLEARANCE_M,
    POCKET_UPPER_ROLL_RADIUS_M,
    POCKET_UPPER_ROLL_STEPS,
    _pocket_upper_roll,
    _convex_hull_xz,
    _pocket_body_opening_seam,
    _pocket_opening_boundary,
    _recessed_funnel_rings,
    sphere,
    curved_leather_terminal,
    mouth_apron_wedge,
    profiled_physics_pocket_cavity,
    profiled_pocket_v10c,
    table_body_with_pocket_openings,
)


class Phase2ModularGltfTests(unittest.TestCase):
    def test_carved_u_full_triangles_stay_inside_previous_wood_cap(self):
        def contained(point,a,b):
            outside=(b[0]-a[0])*(point[2]-a[2])-(b[2]-a[2])*(point[0]-a[0])
            return (outside<=1e-12 and
                    POCKET_BODY_TOP_Y_M-PLAY_SURFACE_Y_M-1e-12 <= point[1] <= -.006+1e-12 and
                    math.hypot(point[0],point[2]) <= .160+1e-12)
        for segments in (24,16,10):
            for pocket in POCKETS:
                profile=profiled_physics_pocket_cavity(pocket[6],segments,pocket[2],True)
                rim=profile[0];physical=profile[4]
                for offset in range(0,36*segments,3):
                    panel=(offset//6)%segments
                    a=(physical[panel][0],0,physical[panel][1])
                    b=(physical[panel+1][0],0,physical[panel+1][1])
                    for index in rim[4][offset:offset+3]:
                        p=rim[0][index]
                        self.assertTrue(contained(p,a,b))
                        self.assertFalse(contained((p[0],0.0,p[2]),a,b),
                                         "raised-solid negative control must fail")
                for index in (0,segments):
                    x,d=physical[index]
                    for actual,expected in zip(rim[0][index],(x,-.018,d)):
                        self.assertAlmostEqual(actual,expected,places=12)
                    for actual,expected in zip(rim[0][segments+1+index],(x,-.006,d)):
                        self.assertAlmostEqual(actual,expected,places=12)
                # Exact inherited sphere-contact curve at the fixed mouth
                # anchor, not a false universal 1 mm claim during descent.
                radius=BALL_DIAMETER_M*.5
                self.assertAlmostEqual(math.hypot(radius,0)-radius,0)
                threshold=math.sqrt((radius+.001)**2-radius**2)
                self.assertAlmostEqual(math.hypot(radius,threshold)-radius,.001)

    def test_upper_roll_is_real_geometry_with_tapered_clear_endpoints(self):
        for segments in (24,16,10):
            for pocket in POCKETS:
                seam=_pocket_body_opening_seam(pocket[6],pocket[2],segments,True)
                geometry,inner=_pocket_upper_roll(seam)
                positions,normals,_,_,indices=geometry
                self.assertEqual(inner[0],(seam[0][0],-0.002,seam[0][1]))
                self.assertEqual(inner[-1],(seam[-1][0],-0.002,seam[-1][1]))
                self.assertGreater(max(n[1] for n in normals),0.95)
                self.assertLess(min(n[1] for n in normals),0.30)
                for index,(x,d) in enumerate(seam):
                    radius=POCKET_UPPER_ROLL_RADIUS_M*math.sin(math.pi*index/segments)
                    self.assertAlmostEqual(inner[index][2]-d,radius,places=12)
                    self.assertAlmostEqual(inner[index][1],-0.002-radius,places=12)
                for point in positions:
                    self.assertLessEqual(point[1],-0.002+1e-12)
                    self.assertFalse(0<point[2]<0.060 and abs(point[0])<pocket[6]*0.5-1e-12)
                for offset in range(0,len(indices),3):
                    tri=indices[offset:offset+3]
                    a,b,c=[positions[i] for i in tri]
                    u=[b[i]-a[i] for i in range(3)];v=[c[i]-a[i] for i in range(3)]
                    face=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                    agreement=sum(face[axis]*sum(normals[i][axis] for i in tri) for axis in range(3))
                    self.assertGreater(agreement,0)
                    self.assertLess(-agreement,0,"reversed-normal negative control")
    def test_recessed_shoulder_has_no_open_internal_edges(self):
        for segments, rounded in ((24, True), (16, False), (10, False)):
            geometry = table_body_with_pocket_openings(
                rounded, segments, recessed_funnel_candidate=True)
            positions, normals, _, _, indices = geometry
            x_limit, z_limit = ((0.740, 1.375) if rounded else (0.775, 1.410))
            holes = []
            physical_opening_area=0.0
            leather_projected_area=0.0
            for _, _, kind, center, inward, tangent, width in POCKETS:
                profile = profiled_physics_pocket_cavity(width, segments, kind)
                candidate = profiled_physics_pocket_cavity(width,segments,kind,True)
                rim_outer=[(p[0],p[2]) for p in candidate[0][0][segments+1:2*(segments+1)]]
                physical_hull=_convex_hull_xz([
                    (center[0]+tangent[0]*x-inward[0]*d,
                     center[1]+tangent[1]*x-inward[1]*d)
                    for x,d in profile[4]+profile[6]])
                physical_opening_area+=abs(sum(a[0]*b[1]-a[1]*b[0]
                    for a,b in zip(physical_hull,physical_hull[1:]+physical_hull[:1])))*.5
                rp,_,_,_,ri=candidate[0]
                for offset in range(0,len(ri),3):
                    a,b,c=[rp[i] for i in ri[offset:offset+3]]
                    leather_projected_area+=abs((b[0]-a[0])*(c[2]-a[2])-
                                                (b[2]-a[2])*(c[0]-a[0]))*.5
                holes.append(_convex_hull_xz([
                    (center[0]+tangent[0]*x-inward[0]*d,
                     center[1]+tangent[1]*x-inward[1]*d)
                    for x,d in profile[4]+profile[6]]))
                for index in range(segments):
                    for tri in ((profile[4][index],profile[4][index+1],rim_outer[index+1]),
                                (profile[4][index],rim_outer[index+1],rim_outer[index])):
                        points=[(center[0]+tangent[0]*x-inward[0]*d,
                                 center[1]+tangent[1]*x-inward[1]*d) for x,d in tri]
                        a,b,c=points
                        if abs((b[0]-a[0])*(c[1]-a[1])-(b[1]-a[1])*(c[0]-a[0]))>1e-14:
                            holes.append(points)
            top = []
            for offset in range(0,len(indices),3):
                tri = indices[offset:offset+3]
                if all(positions[i][1] >= POCKET_BODY_TOP_Y_M-1e-9 and
                       normals[i][1] > 1e-6 for i in tri):
                    top.append([tuple(round(v,12) for v in positions[i]) for i in tri])
            def point_on_segment(p,a,b):
                dx,dz=b[0]-a[0],b[1]-a[1]
                t=((p[0]-a[0])*dx+(p[2]-a[1])*dz)/(dx*dx+dz*dz)
                length=math.hypot(dx,dz)
                tolerance=1e-8/length
                return -tolerance<=t<=1+tolerance and \
                    abs((p[0]-a[0])*dz-(p[2]-a[1])*dx)/length<1e-8
            def open_edges(triangles):
                edges=Counter(tuple(sorted((a,b))) for tri in triangles
                              for a,b in zip(tri,tri[1:]+tri[:1]))
                bad=[]
                for (a,b),count in edges.items():
                    if count==2:continue
                    outer=any(abs(a[axis]-sign*limit)<1e-8 and
                              abs(b[axis]-sign*limit)<1e-8
                              for axis,limit in ((0,x_limit),(2,z_limit))
                              for sign in (-1,1))
                    dx,dz=b[0]-a[0],b[2]-a[2]
                    length2=dx*dx+dz*dz;length=math.sqrt(length2)
                    intervals=[]
                    for h in holes:
                        for c,d in zip(h,h[1:]+h[:1]):
                            if all(abs((p[0]-a[0])*dz-(p[1]-a[2])*dx)/length<1e-8
                                   for p in (c,d)):
                                ts=[((p[0]-a[0])*dx+(p[1]-a[2])*dz)/length2 for p in (c,d)]
                                intervals.append((max(0,min(ts)),min(1,max(ts))))
                    cursor=0.0
                    for start,end in sorted(intervals):
                        if start>cursor+1e-8/length:break
                        cursor=max(cursor,end)
                    hole=cursor>=1-1e-8/length
                    if count!=1 or not (outer or hole):bad.append((a,b,count))
                return bad
            face_normals={}
            max_slope=0.0
            max_angle=0.0
            body_projected_area=0.0
            for tri in top:
                a,b,c=tri
                u=tuple(b[i]-a[i] for i in range(3))
                v=tuple(c[i]-a[i] for i in range(3))
                normal=(u[1]*v[2]-u[2]*v[1],u[2]*v[0]-u[0]*v[2],u[0]*v[1]-u[1]*v[0])
                body_projected_area+=normal[1]*.5
                length=math.sqrt(sum(value*value for value in normal))
                normal=tuple(value/length for value in normal)
                max_slope=max(max_slope,math.hypot(normal[0],normal[2])/normal[1])
                for first,second in zip(tri,tri[1:]+tri[:1]):
                    key=tuple(sorted((first,second)))
                    if key in face_normals:
                        cosine=max(-1,min(1,sum(a*b for a,b in zip(normal,face_normals[key]))))
                        max_angle=max(max_angle,math.degrees(math.acos(cosine)))
                    face_normals[key]=normal
            self.assertLessEqual(max_slope,math.tan(math.radians(35)),
                                 "actual shoulder face slope, not analytic-function slope")
            self.assertLessEqual(max_angle,30.0,
                                 "actual adjacent face-normal discontinuity")
            self.assertAlmostEqual(body_projected_area+leather_projected_area,
                4*x_limit*z_limit-physical_opening_area,places=7,
                msg="body plus carved leather must retain the original physical opening")
            self.assertEqual(open_edges(top), [], "raised patches must share full 3D edges")
            self.assertTrue(open_edges(top[1:]), "deleted face negative control must fail")
            a,b,c=top[0]
            midpoint=tuple((a[axis]+b[axis])*0.5 for axis in range(3))
            t_junction=[[a,midpoint,c],[midpoint,b,c]]+top[1:]
            self.assertTrue(open_edges(t_junction),
                            "splitting only one side of a shared edge must fail")

    @classmethod
    def setUpClass(cls):
        cls.document = json.loads(ASSET.read_text(encoding="utf-8"))
        cls.manifest = json.loads(MANIFEST.read_text(encoding="utf-8"))

    def _validate_mutation(self, document=None, manifest=None):
        return validate(document or copy.deepcopy(self.document),
                        manifest or copy.deepcopy(self.manifest),
                        ASSET_DIR, verify_hashes=False)

    def test_committed_candidate_passes_offline_validation(self):
        errors, report = validate(self.document, self.manifest, ASSET_DIR)
        self.assertEqual(errors, [])
        self.assertEqual(report["unique_triangles"], 8064)
        self.assertEqual(report["materials"], 29)
        self.assertEqual(report["textures"], 22)
        self.assertEqual(report["markers"], 23)

    def test_generator_is_byte_deterministic_for_all_outputs(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-phase2-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory,
                 "--legacy-inward-ball-winding"],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=60)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            generated = Path(directory)
            for committed in sorted(ASSET_DIR.iterdir()):
                if committed.is_dir() or committed.name == "README.md" or \
                        committed.name.startswith("focused_practice_hall_venue_"):
                    continue
                self.assertEqual((generated / committed.name).read_bytes(), committed.read_bytes(),
                                 committed.name)

    def test_default_generator_ball_lods_have_outward_winding_and_normals(self):
        for columns, rows in ((32, 24), (20, 14), (12, 8)):
            positions, normals, _, _, indices = sphere(
                0.028575, columns, rows)
            nondegenerate = 0
            for offset in range(0, len(indices), 3):
                first, second, third = (
                    positions[indices[offset + corner]] for corner in range(3))
                ab = [second[axis] - first[axis] for axis in range(3)]
                ac = [third[axis] - first[axis] for axis in range(3)]
                geometric = [
                    ab[1] * ac[2] - ab[2] * ac[1],
                    ab[2] * ac[0] - ab[0] * ac[2],
                    ab[0] * ac[1] - ab[1] * ac[0],
                ]
                if sum(value * value for value in geometric) <= 1e-24:
                    continue
                nondegenerate += 1
                centroid = [(first[axis] + second[axis] + third[axis]) / 3.0
                            for axis in range(3)]
                self.assertGreater(sum(geometric[axis] * centroid[axis]
                                       for axis in range(3)), 0.0)
            self.assertGreater(nondegenerate, 0)
            self.assertTrue(all(
                sum(position[axis] * normal[axis] for axis in range(3)) > 0.0
                for position, normal in zip(positions, normals)))

    def test_validator_rejects_inward_mesh_claiming_outward_contract(self):
        invalid = copy.deepcopy(self.document)
        invalid["asset"].setdefault("extras", {})[
            "ball_triangle_winding"] = "outward_ccw_v1"
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["ball_geometry_contract"] = {
            "triangle_winding": "outward_ccw_v1",
            "normal_orientation": "outward_radial_v1",
        }
        errors, _ = self._validate_mutation(
            document=invalid, manifest=invalid_manifest)
        self.assertTrue(any("inward-wound" in error for error in errors), errors)

    def test_validator_cli_reports_budget_counts(self):
        completed = subprocess.run(
            [sys.executable, str(VALIDATOR), str(ASSET)], cwd=str(REPOSITORY_ROOT),
            text=True, capture_output=True, timeout=20)
        self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
        self.assertIn("unique_triangles=8064", completed.stdout)
        self.assertIn("materials=29", completed.stdout)
        self.assertIn("textures=22", completed.stdout)

    def test_all_lod_sets_strictly_reduce_triangles_and_share_markers(self):
        marker_lists = []
        for lod_set in self.manifest["lod_sets"].values():
            triangles = lod_set["triangles"]
            self.assertGreater(triangles["0"], triangles["1"])
            self.assertGreater(triangles["1"], triangles["2"])
            marker_lists.append(lod_set["alignment_markers"])
        self.assertTrue(all(markers == marker_lists[0] for markers in marker_lists[1:]))

    def test_invalid_tangent_layout_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        tangent_accessor = invalid["meshes"][0]["primitives"][0]["attributes"]["TANGENT"]
        invalid["accessors"][tangent_accessor]["type"] = "VEC3"
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("invalid tangent" in error or "attribute counts" in error
                            for error in errors), errors)

    def test_unpromoted_pocket_marker_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["nodes"].append({
            "name": "SOCKET_pocket_0_throat",
            "translation": [-0.67, 0.84, -1.31],
        })
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("unpromoted pocket marker" in error for error in errors), errors)

    def test_external_image_uri_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        invalid["images"][0]["uri"] = "https://example.invalid/competitor-atlas.png"
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("external" in error or "generated ball atlas" in error
                            for error in errors), errors)

    def test_missing_lod_marker_reference_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        root = self.manifest["lod_sets"]["ball"]["nodes"]["2"]
        invalid["nodes"][root]["extras"]["alignment_marker_nodes"] = []
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("identical marker list" in error for error in errors), errors)

    def test_geometry_entering_pocket_clearance_is_rejected(self):
        invalid = copy.deepcopy(self.document)
        leather = next(node for node in invalid["nodes"]
                       if node.get("extras", {}).get("asset_role") ==
                       "pocket_facing_visual_only")
        mesh = invalid["meshes"][leather["mesh"]]
        accessor_index = mesh["primitives"][0]["attributes"]["POSITION"]
        accessor = invalid["accessors"][accessor_index]
        view = invalid["bufferViews"][accessor["bufferView"]]
        prefix = "data:application/octet-stream;base64,"
        encoded = invalid["buffers"][0]["uri"]
        payload = bytearray(base64.b64decode(encoded[len(prefix):]))
        offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
        struct.pack_into("<3f", payload, offset, -0.635, 0.86483976, 0.0)
        invalid["buffers"][0]["uri"] = prefix + base64.b64encode(payload).decode("ascii")
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("NO_VISUAL_SOLID" in error for error in errors), errors)

    def test_manifest_cannot_claim_unpromoted_parameters(self):
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["constraints"]["unpromoted_pocket_parameters_present"] = True
        errors, _ = self._validate_mutation(manifest=invalid_manifest)
        self.assertTrue(any("unpromoted_pocket_parameters_present" in error
                            for error in errors), errors)

    def test_felt_topology_contract_rejects_cracked_boundary_claim(self):
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["felt_topology"]["boundary_loops"] = 7
        errors, _ = self._validate_mutation(manifest=invalid_manifest)
        self.assertTrue(any("felt topology evidence" in error for error in errors), errors)

    def test_ball_atlas_uses_mip_safe_inset_and_review_identities(self):
        contract = self.manifest["ball_atlas_contract"]
        self.assertEqual(contract["tile_size_px"], [512, 256])
        self.assertEqual(contract["safe_inset_px"], 24)
        self.assertEqual(contract["review_identities"], [1, 8, 11, 15])
        self.assertEqual(contract["sphere_uv_u_orientation"],
                         "decreases_with_positive_longitude")
        self.assertEqual(contract["glyph_storage_orientation"], "upright_source")
        self.assertTrue(CLOSEUP.is_file())
        invalid_manifest = copy.deepcopy(self.manifest)
        invalid_manifest["ball_atlas_contract"]["safe_inset_px"] = 4
        errors, _ = self._validate_mutation(manifest=invalid_manifest)
        self.assertTrue(any("tile/mip-safe" in error for error in errors), errors)

    def test_textured_base_color_factor_must_remain_neutral(self):
        invalid = copy.deepcopy(self.document)
        material = next(item for item in invalid["materials"]
                        if item["name"] == "MAT_table_felt_teal")
        material["pbrMetallicRoughness"]["baseColorFactor"] = [0.0144, 0.107, 0.1248, 1.0]
        errors, _ = self._validate_mutation(document=invalid)
        self.assertTrue(any("double tint" in error for error in errors), errors)

    def test_smooth_pocket_candidate_shares_only_continuous_surface_vertices(self):
        def normal_angle(first, second):
            dot = max(-1.0, min(1.0,
                sum(first[axis] * second[axis] for axis in range(3))))
            return math.degrees(math.acos(dot))

        def normal_splits(geometry):
            positions, normals = geometry[:2]
            by_position = {}
            for position, normal in zip(positions, normals):
                key = tuple(round(value, 8) for value in position)
                by_position.setdefault(key, []).append(normal)
            return [normal_angle(normals[first], normals[second])
                    for normals in by_position.values()
                    for first in range(len(normals))
                    for second in range(first + 1, len(normals))]

        legacy_rim, legacy_sidewall, _, _, _ = profiled_pocket_v10c(
            0.13, 24)
        smooth_rim, smooth_sidewall, _, _, _ = profiled_pocket_v10c(
            0.13, 24, smooth=True)
        self.assertEqual((len(legacy_rim[0]), len(smooth_rim[0])), (96, 50))
        self.assertEqual((len(legacy_sidewall[0]), len(smooth_sidewall[0])),
                         (96, 50))
        self.assertEqual(len(legacy_rim[4]), len(smooth_rim[4]))
        self.assertEqual(len(legacy_sidewall[4]), len(smooth_sidewall[4]))
        self.assertGreater(max(normal_splits(legacy_rim)), 10.0)
        self.assertGreater(max(normal_splits(legacy_sidewall)), 10.0)
        self.assertEqual(normal_splits(smooth_rim), [])
        self.assertEqual(normal_splits(smooth_sidewall), [])
        for geometry in (smooth_rim, smooth_sidewall):
            for normal in geometry[1]:
                self.assertAlmostEqual(math.sqrt(sum(value * value
                    for value in normal)), 1.0, places=6)

        # Rim and sidewall remain separate geometries at their real slope
        # crease; smoothing must not blend the material-stage boundary.
        seam_count = 25
        seam_angles = [normal_angle(smooth_rim[1][index],
            smooth_sidewall[1][seam_count + index])
            for index in range(seam_count)]
        self.assertGreater(max(seam_angles), 20.0)

        legacy_leather = curved_leather_terminal(
            0.14, 0.04, 0.002, 0.02)
        smooth_leather = curved_leather_terminal(
            0.14, 0.04, 0.002, 0.02, smooth=True)
        self.assertEqual((len(legacy_leather[0]), len(smooth_leather[0])),
                         (120, 72))
        self.assertEqual(len(legacy_leather[4]), len(smooth_leather[4]))
        self.assertGreater(max(normal_splits(legacy_leather)), 90.0)
        self.assertGreater(max(normal_splits(smooth_leather)), 45.0,
            "the four intentional leather folds must remain hard")
        for side in range(4):
            offset = side * 16
            for station in range(7):
                for track in range(2):
                    self.assertLess(normal_angle(
                        smooth_leather[1][offset + station * 2 + track],
                        smooth_leather[1][offset + (station + 1) * 2 + track]),
                        35.0)

        # Candidate terminals occupy only the rail/outside half-space.  Their
        # inner edge stays on the cloth boundary, the 4 mm throat remains
        # outside it, and the terminal grows to the real 80 mm rail width.
        for inward_sign in (-1.0, 1.0):
            aligned = curved_leather_terminal(
                0.14, 0.04, 0.002, 0.0, smooth=True,
                inward_cross_sign=inward_sign)
            signed = [inward_sign * position[0] for position in aligned[0]]
            self.assertLessEqual(max(signed), 1e-12)
            self.assertAlmostEqual(min(signed), -0.080, places=12)
            throat = [inward_sign * position[0] for position in aligned[0]
                      if abs(position[2]) <= 1e-12]
            rail_end = [inward_sign * position[0] for position in aligned[0]
                        if abs(position[2] - 0.14) <= 1e-12]
            self.assertAlmostEqual(min(throat), -0.004, places=12)
            self.assertAlmostEqual(max(throat), 0.0, places=12)
            self.assertAlmostEqual(min(rail_end), -0.080, places=12)
            self.assertAlmostEqual(max(rail_end), 0.0, places=12)

        for geometry in (smooth_rim, smooth_sidewall, smooth_leather):
            positions, _, _, _, indices = geometry
            for offset in range(0, len(indices), 3):
                a, b, c = (positions[indices[offset + index]]
                           for index in range(3))
                first = tuple(b[axis] - a[axis] for axis in range(3))
                second = tuple(c[axis] - a[axis] for axis in range(3))
                cross = (first[1] * second[2] - first[2] * second[1],
                         first[2] * second[0] - first[0] * second[2],
                         first[0] * second[1] - first[1] * second[0])
                self.assertGreater(sum(value * value for value in cross), 1e-20)

        for pocket_type, width in (("side", 0.086), ("corner", 0.132)):
            rim, _, _, rim_inner, _ = profiled_pocket_v10c(
                width, 24, smooth=True, pocket_type=pocket_type)
            expected_outer = _pocket_opening_boundary(width, pocket_type, 24)
            expected_inner = _pocket_opening_boundary(
                width, pocket_type, 24, inset=0.009144)
            self.assertEqual(rim_inner, expected_inner)
            self.assertEqual([(position[0], position[2])
                              for position in rim[0][25:]], expected_outer)
            self.assertTrue(all(inner[1] <= outer[1]
                                for inner, outer in zip(expected_inner, expected_outer)))

    def test_generated_smooth_pocket_candidate_uses_audited_black_liner(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-phase2-smooth-pocket-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory,
                 "--smooth-pocket-candidate",
                 "--legacy-unsplit-pocket-facing"],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=60)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            generated = Path(directory)
            document = json.loads((generated / ASSET.name).read_text(encoding="utf-8"))
            manifest = json.loads((generated / MANIFEST.name).read_text(encoding="utf-8"))
            errors, _ = validate(document, manifest, generated)
            self.assertEqual(errors, [])

            topology = document["asset"]["extras"]["pocket_surface_topology"]
            self.assertEqual(topology["liner_finish"],
                             "existing_pure_black_cavity_material")
            self.assertEqual(topology["liner_opening_seam"],
                             "matches_actual_felt_cut_boundary")
            self.assertEqual(topology["leather_terminal_alignment"],
                             "inner_edge_on_cloth_boundary_outer_edge_on_rail")
            material_names = [material["name"] for material in document["materials"]]
            for role in ("pocket_liner_rim_visual_only",
                         "pocket_liner_sidewall_visual_only"):
                role_nodes = [node for node in document["nodes"]
                              if node.get("extras", {}).get("asset_role") == role]
                self.assertEqual(len(role_nodes), 3)
                for node in role_nodes:
                    primitive = document["meshes"][node["mesh"]]["primitives"][0]
                    self.assertEqual(material_names[primitive["material"]],
                                     "MAT_pocket_void")

            invalid = copy.deepcopy(document)
            rim = next(node for node in invalid["nodes"]
                       if node.get("extras", {}).get("asset_role") ==
                       "pocket_liner_rim_visual_only")
            liner_material = next(index for index, material in enumerate(invalid["materials"])
                                  if material["name"] == "MAT_pocket_liner")
            invalid["meshes"][rim["mesh"]]["primitives"][0]["material"] = liner_material
            errors, _ = validate(invalid, manifest, generated, verify_hashes=False)
            self.assertTrue(any("material order" in error for error in errors), errors)

            invalid = copy.deepcopy(document)
            invalid["asset"]["extras"]["pocket_surface_topology"]["liner_finish"] = \
                "unreviewed_finish"
            errors, _ = validate(invalid, manifest, generated, verify_hashes=False)
            self.assertTrue(any("surface topology contract" in error for error in errors), errors)

            invalid = copy.deepcopy(document)
            rim = next(node for node in invalid["nodes"]
                       if node.get("extras", {}).get("asset_role") ==
                       "pocket_liner_rim_visual_only")
            rim["extras"]["local_ranges"][0]["outer_boundary"][5][1] += 0.001
            errors, _ = validate(invalid, manifest, generated, verify_hashes=False)
            self.assertTrue(any("cloth opening seam" in error for error in errors), errors)

    def test_split_facing_candidate_opens_the_recessed_throat(self):
        full = curved_leather_terminal(
            0.14, 0.04, 0.002, 0.0, smooth=True,
            inward_cross_sign=1.0)
        jaw = curved_leather_terminal(
            0.14, 0.04, 0.002, 0.0, smooth=True,
            inward_cross_sign=1.0, surface="jaw", width_profile="cubic")
        facing = curved_leather_terminal(
            0.14, 0.04, 0.002, 0.0, smooth=True,
            inward_cross_sign=1.0, surface="facing", width_profile="cubic")
        self.assertEqual((len(full[0]), len(full[4])), (72, 180))
        self.assertEqual((len(jaw[0]), len(jaw[4])), (16, 42))
        self.assertEqual((len(facing[0]), len(facing[4])), (4, 6))
        self.assertTrue(all(abs(position[2]) <= 1e-12 for position in facing[0]))
        self.assertAlmostEqual(min(position[0] for position in facing[0]),
                               -0.004, places=12)
        self.assertAlmostEqual(max(position[0] for position in facing[0]),
                               0.0, places=12)
        first_section = [position for position in jaw[0]
                         if abs(position[2] - 0.14 / 7.0) <= 1e-12]
        self.assertTrue(first_section)
        self.assertTrue(all(abs(position[0]) <= 1e-12
                            for position in first_section))
        self.assertGreater(max(position[1] for position in first_section) -
                           min(position[1] for position in first_section), 0.0)
        for offset in range(0, len(jaw[4]), 3):
            triangle = [jaw[0][jaw[4][offset + index]] for index in range(3)]
            self.assertGreater(max(point[2] for point in triangle) -
                               min(point[2] for point in triangle), 0.0,
                               "jaw must contain only longitudinal shell triangles")

        for rounded, segments, limits in (
                (True, 24, (0.740, 1.375)),
                (False, 16, (0.775, 1.410)),
                (False, 10, (0.775, 1.410))):
            body = table_body_with_pocket_openings(rounded, segments)
            positions, normals, _, _, indices = body
            top_area = 0.0
            for offset in range(0, len(indices), 3):
                triangle = indices[offset:offset + 3]
                if not all(normals[index][1] > 0.999999
                           for index in triangle):
                    continue
                first, second, third = (positions[index]
                                        for index in triangle)
                twice_area = ((second[2] - first[2]) *
                              (third[0] - first[0]) -
                              (second[0] - first[0]) *
                              (third[2] - first[2]))
                self.assertGreater(twice_area, 0.0)
                top_area += 0.5 * twice_area
            opening_area = 0.0
            for _, _, pocket_type, center, inward, tangent, width in POCKETS:
                _, _, _, _, cavity_upper, _, cloth_seam, _ = \
                    profiled_physics_pocket_cavity(
                        width, segments, pocket_type)
                opening_points = [
                    (center[0] + tangent[0] * x - inward[0] * depth,
                     center[1] + tangent[1] * x - inward[1] * depth)
                    for x, depth in (cavity_upper + cloth_seam)]
                opening = _convex_hull_xz(opening_points)
                opening_area += abs(0.5 * sum(
                    opening[index][0] * opening[(index + 1) % len(opening)][1] -
                    opening[(index + 1) % len(opening)][0] * opening[index][1]
                    for index in range(len(opening))))
            expected_area = 4.0 * limits[0] * limits[1] - opening_area
            self.assertAlmostEqual(top_area, expected_area, places=11)

        for pocket_type, width in (("corner", 0.132), ("side", 0.086)):
            rim, sidewall, void, rim_inner, cavity_upper, lower, cloth, mouth = \
                profiled_physics_pocket_cavity(width, 24, pocket_type)
            self.assertEqual(len(mouth), 25)
            self.assertAlmostEqual(max(point[0] for point in mouth) -
                                   min(point[0] for point in mouth),
                                   width, places=12)
            self.assertTrue(all(abs(point[1]) <= 1e-12 for point in mouth))
            self.assertAlmostEqual(min(point[1] for point in lower), 0.0,
                                   places=12)
            self.assertAlmostEqual(max(point[1] for point in lower),
                                   POCKET_PHYSICS_THROAT_DEPTH_M + width * 0.25,
                                   places=12)
            self.assertAlmostEqual(
                max(point[1] for point in cavity_upper),
                POCKET_PHYSICS_THROAT_DEPTH_M + width * 0.25, places=12)
            throat_points = [point for point in cavity_upper
                             if abs(point[1] - POCKET_PHYSICS_THROAT_DEPTH_M)
                             <= 1e-12]
            self.assertGreaterEqual(len(throat_points), 2)
            self.assertAlmostEqual(
                max(point[0] for point in throat_points) -
                min(point[0] for point in throat_points), width, places=12)
            # The physical 0..6 cm passage stays a full-width rectangle.  Its
            # two capture corners are unchanged; only the visible boundary
            # behind that station forms a symmetric rounded back.
            rounded_back = [point for point in cavity_upper
                            if point[1] >=
                            POCKET_PHYSICS_THROAT_DEPTH_M - 1e-12]
            self.assertAlmostEqual(rounded_back[0][0], width * 0.5,
                                   places=12)
            self.assertAlmostEqual(rounded_back[-1][0], -width * 0.5,
                                   places=12)
            self.assertAlmostEqual(rounded_back[0][1],
                                   POCKET_PHYSICS_THROAT_DEPTH_M, places=12)
            self.assertAlmostEqual(rounded_back[-1][1],
                                   POCKET_PHYSICS_THROAT_DEPTH_M, places=12)
            for x, depth in rounded_back:
                expected_depth = (
                    POCKET_PHYSICS_THROAT_DEPTH_M + width * 0.25 *
                    math.sqrt(max(0.0, 1.0 - (x / (width * 0.5)) ** 2)))
                self.assertAlmostEqual(depth, expected_depth, places=12)
            for first, second in zip(rounded_back,
                                     reversed(rounded_back)):
                self.assertAlmostEqual(first[1], second[1], places=12)
            for upper, chord in ((cavity_upper[0], mouth[0]),
                                 (cavity_upper[-1], mouth[-1])):
                self.assertAlmostEqual(upper[0], chord[0], places=12)
                self.assertAlmostEqual(upper[1], chord[1], places=12)
            self.assertGreater(max(point[1] for point in cavity_upper[1:-1]),
                               0.0)
            self.assertTrue(all(point[1] <= 1e-12 for point in cloth),
                            "only the cloth seam may lie inward of the mouth")
            self.assertLess(min(point[1] for point in cloth), 0.0)
            self.assertTrue(all(point[2] >= -1e-12
                                for point in sidewall[0]))
            self.assertAlmostEqual(min(point[2] for point in void[0]),
                                   min(point[1] for point in cloth), places=12)
            self.assertLessEqual(max(point[2]
                                     for point in sidewall[0] + void[0]),
                                 POCKET_PHYSICS_THROAT_DEPTH_M + width * 0.25 +
                                 1e-12)
            self.assertEqual([(point[0], point[2]) for point in rim[0][:25]],
                             rim_inner)
            self.assertEqual([(point[0], point[2]) for point in rim[0][25:]],
                             cavity_upper)
            self.assertEqual([(point[0], point[2])
                              for point in sidewall[0][25:]], rim_inner)
            self.assertTrue(all(point[1] >= -1e-12
                                for point in rim_inner + cavity_upper))
            self.assertGreater(
                max(math.hypot(cavity_upper[index][0] - rim_inner[index][0],
                               cavity_upper[index][1] - rim_inner[index][1])
                    for index in range(1, len(cavity_upper) - 1)),
                POCKET_LINER_WIDTH_M * 0.99)
            self.assertTrue(any(abs(point[2] -
                                    POCKET_PHYSICS_THROAT_DEPTH_M) <= 1e-12
                                for point in void[0]))
            self.assertAlmostEqual(min(point[1] for point in void[0]),
                                   -POCKET_PHYSICS_VOID_BOTTOM_M, places=12)
            self.assertTrue(any(abs(normal[1]) <= 1e-12
                                for normal in void[1]))
            self.assertTrue(any(normal[1] > 0.999999
                                for normal in void[1]))

            closed_apron = mouth_apron_wedge(width)
            open_apron = mouth_apron_wedge(width, open_top=True)
            self.assertEqual(len(closed_apron[4]) - len(open_apron[4]), 12)
            self.assertLessEqual(max(point[1] + PLAY_SURFACE_Y_M
                                     for point in open_apron[0]), 0.775)
            # No candidate apron triangle may lie wholly on and seal Z=0.
            for offset in range(0, len(open_apron[4]), 3):
                triangle = [open_apron[0][open_apron[4][offset + index]]
                            for index in range(3)]
                self.assertFalse(all(abs(point[2]) <= 1e-12
                                     for point in triangle))

        with tempfile.TemporaryDirectory(
                prefix="billiardgl-phase2-split-facing-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory,
                 "--split-pocket-facing-candidate"],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True,
                timeout=60)
            self.assertEqual(completed.returncode, 0,
                             completed.stdout + completed.stderr)
            generated = Path(directory)
            document = json.loads((generated / ASSET.name).read_text(encoding="utf-8"))
            manifest = json.loads((generated / MANIFEST.name).read_text(encoding="utf-8"))
            errors, report = validate(document, manifest, generated)
            self.assertEqual(errors, [])
            self.assertEqual(report["unique_triangles"], 8064)
            self.assertEqual(report["materials"], 29)
            self.assertEqual(report["textures"], 22)

            material_names = [item["name"] for item in document["materials"]]
            roles = {}
            for node in document["nodes"]:
                role = node.get("extras", {}).get("asset_role")
                if role:
                    roles.setdefault(role, []).append(node)
            self.assertEqual(len(roles["pocket_cushion_jaw_visual_only"]), 2)
            self.assertEqual(len(roles["pocket_facing_visual_only"]), 2)
            self.assertNotIn("pocket_throat_void_visual_only", roles)
            self.assertEqual(document["asset"]["extras"]["pocket_visual_frame"], {
                "x": "physics_tangent", "y": "world_up",
                "z": "physics_outward_depth",
                "marker_z_mapping": "z_visual=z_marker",
                "handedness": "right",
            })
            self.assertEqual(document["asset"]["extras"][
                "pocket_physics_cavity_profile"], {
                    "corner_mouth_width_m": 0.132,
                    "side_mouth_width_m": 0.086,
                    "throat_depth_m": 0.060,
                    "cavity_drop_m": 0.055,
                })

            def positions_for(node, source=document):
                primitive = source["meshes"][node["mesh"]]["primitives"][0]
                accessor = source["accessors"][
                    primitive["attributes"]["POSITION"]]
                view = source["bufferViews"][accessor["bufferView"]]
                prefix = "data:application/octet-stream;base64,"
                payload = base64.b64decode(source["buffers"][0]["uri"][len(prefix):])
                offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                return [struct.unpack_from("<3f", payload, offset + index * 12)
                        for index in range(accessor["count"])]

            def uvs_for(node, source=document):
                primitive = source["meshes"][node["mesh"]]["primitives"][0]
                accessor = source["accessors"][
                    primitive["attributes"]["TEXCOORD_0"]]
                view = source["bufferViews"][accessor["bufferView"]]
                prefix = "data:application/octet-stream;base64,"
                payload = base64.b64decode(
                    source["buffers"][0]["uri"][len(prefix):])
                offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                return [struct.unpack_from("<2f", payload, offset + index * 8)
                        for index in range(accessor["count"])]

            pocket_by_id = {item[0]: item for item in POCKETS}
            for role in ("pocket_liner_rim_visual_only",
                         "pocket_liner_sidewall_visual_only",
                         "pocket_void_visual_only",
                         "pocket_void_bottom_visual_only"):
                for node in roles[role]:
                    positions = positions_for(node)
                    for item in node["extras"]["local_ranges"]:
                        pocket = pocket_by_id[item["pocket_id"]]
                        center, inward, tangent, width = (
                            pocket[3], pocket[4], pocket[5], pocket[6])
                        subset = positions[item["vertex_start"]:
                                           item["vertex_start"] +
                                           item["vertex_count"]]
                        local = []
                        for position in subset:
                            delta = (position[0] - center[0],
                                     position[2] - center[1])
                            local.append((
                                delta[0] * tangent[0] + delta[1] * tangent[1],
                                -(delta[0] * inward[0] + delta[1] * inward[1])))
                        depths = [point[1] for point in local]
                        expected_minimum = (min(
                            -point[1] for point in _pocket_opening_boundary(
                                width, pocket[2], 24 if node["extras"]["lod"] == 0
                                else (16 if node["extras"]["lod"] == 1 else 10),
                                physics_aligned=True))
                            if role in ("pocket_void_visual_only",
                                        "pocket_void_bottom_visual_only") else 0.0)
                        self.assertAlmostEqual(min(depths), expected_minimum,
                                               places=6)
                        maximum_depth = (POCKET_PHYSICS_THROAT_DEPTH_M +
                                         (width * 0.25 if role in (
                                             "pocket_liner_rim_visual_only",
                                             "pocket_liner_sidewall_visual_only",
                                             "pocket_void_visual_only",
                                             "pocket_void_bottom_visual_only") else 0.0))
                        self.assertLessEqual(
                            max(depths), maximum_depth + 1e-6)
                        if role == "pocket_void_visual_only":
                            self.assertTrue(node["extras"]
                                            ["vertical_capture_back"])
                            self.assertTrue(node["extras"]
                                            ["inward_cloth_seam_wall"])
                            self.assertNotIn("sloped_recessed_funnel",
                                             node["extras"])
                            self.assertAlmostEqual(item["throat_width_m"],
                                                   width, places=12)
                            self.assertAlmostEqual(
                                item["post_capture_rounding_depth_m"],
                                width * 0.25, places=12)
                            self.assertAlmostEqual(max(depths),
                                                   POCKET_PHYSICS_THROAT_DEPTH_M +
                                                   width * 0.25,
                                                   places=6)
                        if role == "pocket_void_bottom_visual_only":
                            self.assertEqual(item["uv_depth_mapping"],
                                             "constant_deep_v1")
                            self.assertTrue(all(abs(position[1] -
                                                    (PLAY_SURFACE_Y_M -
                                                     POCKET_PHYSICS_VOID_BOTTOM_M))
                                                <= 1e-6
                                                for position in subset))
                    if role == "pocket_void_bottom_visual_only":
                        self.assertEqual(node["extras"]["occlusion_texture_set"],
                                         "pocket_void")
                        self.assertEqual(node["extras"]["uv_depth_mapping"],
                                         "constant_deep_v1")
                        self.assertTrue(all(abs(uv[1] - 1.0) <= 1e-7
                                            for uv in uvs_for(node)))
            self.assertNotIn("pocket_apron_integrated_wedge", roles)
            self.assertEqual(document["asset"]["extras"]
                             ["pocket_surface_topology"]["apron"],
                             "table_body_support_without_separate_throat_frame")
            for node in roles["pocket_void_bottom_visual_only"]:
                primitive = document["meshes"][node["mesh"]]["primitives"][0]
                self.assertEqual(material_names[primitive["material"]],
                                 "MAT_pocket_void_depth")
            depth_material = next(material for material in document["materials"]
                                  if material["name"] == "MAT_pocket_void_depth")
            self.assertEqual(depth_material["pbrMetallicRoughness"]
                             ["baseColorFactor"],
                             [0.0005, 0.0005, 0.0006, 1.0])
            self.assertEqual(depth_material["extras"]["pbr_texture_set"],
                             "pocket_void")
            for node in roles["pocket_cushion_jaw_visual_only"]:
                primitive = document["meshes"][node["mesh"]]["primitives"][0]
                self.assertEqual(material_names[primitive["material"]],
                                 "MAT_table_cushion_rubber")
                self.assertEqual(node["extras"]["width_profile"],
                                 "quadratic_throat_to_rail_gap")
            for node in roles["pocket_facing_visual_only"]:
                primitive = document["meshes"][node["mesh"]]["primitives"][0]
                self.assertEqual(material_names[primitive["material"]],
                                 "MAT_pocket_leather")
                self.assertEqual(node["extras"]["surface_scope"],
                                 "throat_cap_only")

            invalid = copy.deepcopy(document)
            jaw_node = next(node for node in invalid["nodes"]
                            if node.get("extras", {}).get("asset_role") ==
                            "pocket_cushion_jaw_visual_only")
            jaw_primitive = invalid["meshes"][jaw_node["mesh"]]["primitives"][0]
            jaw_primitive["material"] = material_names.index("MAT_pocket_leather")
            errors, _ = validate(invalid, manifest, generated, verify_hashes=False)
            self.assertTrue(any("material stages" in error for error in errors), errors)

            invalid = copy.deepcopy(document)
            facing_node = next(node for node in invalid["nodes"]
                               if node.get("extras", {}).get("asset_role") ==
                               "pocket_facing_visual_only")
            facing_node["extras"]["surface_scope"] = "broad_top_plate"
            errors, _ = validate(invalid, manifest, generated, verify_hashes=False)
            self.assertTrue(any("throat-cap-only" in error for error in errors), errors)

            invalid = copy.deepcopy(document)
            void_node = next(node for node in invalid["nodes"]
                             if node.get("extras", {}).get("asset_role") ==
                             "pocket_void_visual_only")
            void_node["extras"]["throat_depth_m"] = 0.061
            errors, _ = validate(invalid, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("exact mouth/throat binding" in error
                                for error in errors), errors)

            invalid = copy.deepcopy(document)
            sidewall_node = next(node for node in invalid["nodes"]
                                 if node.get("extras", {}).get("asset_role") ==
                                 "pocket_liner_sidewall_visual_only" and
                                 node.get("extras", {}).get("lod") == 0)
            first_range = sidewall_node["extras"]["local_ranges"][0]
            pocket = pocket_by_id[first_range["pocket_id"]]
            center, inward = pocket[3], pocket[4]
            primitive = invalid["meshes"][sidewall_node["mesh"]]["primitives"][0]
            accessor = invalid["accessors"][primitive["attributes"]["POSITION"]]
            view = invalid["bufferViews"][accessor["bufferView"]]
            prefix = "data:application/octet-stream;base64,"
            payload = bytearray(base64.b64decode(
                invalid["buffers"][0]["uri"][len(prefix):]))
            offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            x, y, z = struct.unpack_from("<3f", payload, offset)
            struct.pack_into("<3f", payload, offset,
                             x + inward[0] * 0.010, y,
                             z + inward[1] * 0.010)
            invalid["buffers"][0]["uri"] = prefix + \
                base64.b64encode(payload).decode("ascii")
            errors, _ = validate(invalid, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("crosses inward of the physics mouth plane"
                                in error for error in errors), errors)

            invalid = copy.deepcopy(document)
            facing = next(node for node in invalid["nodes"]
                          if node.get("extras", {}).get("asset_role") ==
                          "pocket_facing_visual_only" and
                          node.get("extras", {}).get("lod") == 0)
            first_range = facing["extras"]["local_ranges"][0]
            pocket = next(item for item in POCKETS
                          if item[0] == first_range["pocket_id"])
            sign = first_range["terminal_sign"]
            inward = pocket[4]
            tangent = pocket[5]
            railward = (tangent[0] * sign + inward[0],
                        tangent[1] * sign + inward[1])
            railward_length = math.hypot(*railward)
            railward = (railward[0] / railward_length,
                        railward[1] / railward_length)
            cross_axis = (-railward[1], railward[0])
            inward_cross_sign = first_range["inward_cross_sign"]
            primitive = invalid["meshes"][facing["mesh"]]["primitives"][0]
            accessor = invalid["accessors"][primitive["attributes"]["POSITION"]]
            view = invalid["bufferViews"][accessor["bufferView"]]
            prefix = "data:application/octet-stream;base64,"
            payload = bytearray(base64.b64decode(invalid["buffers"][0]["uri"][len(prefix):]))
            offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
            x, y, z = struct.unpack_from("<3f", payload, offset)
            struct.pack_into("<3f", payload, offset,
                             x + cross_axis[0] * inward_cross_sign * 0.020,
                             y,
                             z + cross_axis[1] * inward_cross_sign * 0.020)
            invalid["buffers"][0]["uri"] = prefix + \
                base64.b64encode(payload).decode("ascii")
            errors, _ = validate(invalid, manifest, generated, verify_hashes=False)
            self.assertTrue(any("terminal crosses cloth" in error for error in errors), errors)

            invalid = copy.deepcopy(document)
            rim = next(node for node in invalid["nodes"]
                       if node.get("extras", {}).get("asset_role") ==
                       "pocket_liner_rim_visual_only" and
                       node.get("extras", {}).get("lod") == 0)
            primitive = invalid["meshes"][rim["mesh"]]["primitives"][0]
            accessor = invalid["accessors"][primitive["attributes"]["POSITION"]]
            view = invalid["bufferViews"][accessor["bufferView"]]
            prefix = "data:application/octet-stream;base64,"
            payload = bytearray(base64.b64decode(invalid["buffers"][0]["uri"][len(prefix):]))
            # First pocket's outer ring begins after its 25 inner-ring vertices.
            offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0) + 25 * 12
            x, y, z = struct.unpack_from("<3f", payload, offset)
            struct.pack_into("<3f", payload, offset, x + 0.001, y, z)
            invalid["buffers"][0]["uri"] = prefix + \
                base64.b64encode(payload).decode("ascii")
            errors, _ = validate(invalid, manifest, generated, verify_hashes=False)
            self.assertTrue(any("cloth opening seam" in error for error in errors), errors)

    def test_recessed_funnel_candidate_shares_apron_opening_and_contracts_below_capture(self):
        for pocket_type, width in (("corner", 0.132), ("side", 0.086)):
            legacy = profiled_physics_pocket_cavity(width, 24, pocket_type)
            candidate = profiled_physics_pocket_cavity(
                width, 24, pocket_type, recessed_funnel_candidate=True)
            rim, sidewall, void, rim_inner, opening, lower, cloth_seam, mouth = \
                candidate
            # The shipping plan geometry stays byte-identical; only the
            # visual Y profile and post-capture shell are replaced.
            self.assertEqual(opening, legacy[4])
            self.assertEqual(cloth_seam, legacy[6])
            self.assertEqual(mouth, legacy[7])
            self.assertEqual(rim_inner, opening)
            body_seam = _pocket_body_opening_seam(
                width, pocket_type, 24,
                recessed_funnel_candidate=True)
            self.assertEqual(len(body_seam), len(cloth_seam))
            for cloth_point, body_point in zip(cloth_seam, body_seam):
                self.assertAlmostEqual(body_point[0], cloth_point[0], places=12)
                self.assertAlmostEqual(
                    body_point[1],
                    cloth_point[1] -
                    POCKET_RECESSED_BODY_APERTURE_UNDERCUT_M, places=12)
            legacy_rim_inner = legacy[3]
            legacy_lower = legacy[5]
            expected_outer, expected_inner, expected_lower = \
                _recessed_funnel_rings(
                    opening, legacy_rim_inner, legacy_lower)
            expected_outer=[(a[0]+(b[0]-a[0])*5/7,b[1],a[2]+(b[2]-a[2])*5/7)
                            for a,b in zip(expected_inner,expected_outer)]
            ring_count = len(opening)
            self.assertEqual(rim[0][:ring_count], expected_inner)
            self.assertEqual(rim[0][ring_count:2*ring_count], expected_outer)
            self.assertEqual(sidewall[0][:ring_count], expected_lower)
            self.assertEqual(sidewall[0][ring_count:2 * ring_count], expected_inner)
            self.assertEqual(len(rim[4]), 36 * (ring_count - 1) + 6*(len(body_seam)-1))
            self.assertEqual(len(sidewall[4]),
                             6 * (ring_count - 1) + 6 * (len(body_seam) - 1))
            self.assertEqual(body_seam, cloth_seam,
                             "the cabinet aperture must not excavate beyond the cloth seam")

            # -6/-18 mm is deliberate: it exactly reuses the two existing jaw
            # nose vertices.  A nominal -12 mm single line would need another
            # cap or leave the jaw-to-lip edge unwelded.
            for actual, expected in zip(
                    expected_outer[0], (width * 0.5, -0.006, 0.0)):
                self.assertAlmostEqual(actual, expected, places=12)
            for actual, expected in zip(
                    expected_inner[0], (width * 0.5, -0.018, 0.0)):
                self.assertAlmostEqual(actual, expected, places=12)
            capture_outer = [point for point, boundary in
                             zip(expected_outer, opening)
                             if abs(boundary[1] -
                                    POCKET_PHYSICS_THROAT_DEPTH_M) <= 1e-12]
            self.assertTrue(capture_outer)
            self.assertTrue(all(
                abs(point[1] -
                    (POCKET_SHOULDER_TOP_Y_M - PLAY_SURFACE_Y_M)) <= 1e-12
                for point in capture_outer))
            self.assertAlmostEqual(min(point[1] for point in expected_outer),
                                   POCKET_RECESSED_BACK_LIP_Y_M, places=12)
            transition_samples = sorted([
                (boundary[1], outer[1], inner[1], lower_point[1])
                for boundary, outer, inner, lower_point in zip(
                    opening, expected_outer, expected_inner, expected_lower)
                if boundary[1] <= POCKET_PHYSICS_THROAT_DEPTH_M + 1e-12],
                key=lambda item: item[0])
            self.assertTrue(any(
                abs(depth - POCKET_RECESSED_LIP_DESCENT_M) <= 0.006
                for depth, _, _, _ in transition_samples))
            previous = None
            for depth, outer_y, inner_y, lower_y in transition_samples:
                if previous is not None:
                    self.assertLessEqual(outer_y, previous[0] + 1e-12)
                    self.assertLessEqual(inner_y, previous[1] + 1e-12)
                    self.assertLessEqual(lower_y, previous[2] + 1e-12)
                previous = (outer_y, inner_y, lower_y)
                if depth >= POCKET_RECESSED_LIP_DESCENT_M - 1e-12:
                    self.assertAlmostEqual(
                        outer_y, POCKET_SHOULDER_TOP_Y_M - PLAY_SURFACE_Y_M,
                        places=12)
                    self.assertAlmostEqual(
                        lower_y, POCKET_RECESSED_CAPTURE_WALL_Y_M,
                        places=12)
            self.assertTrue(all(
                abs(point[0]) >= width * 0.5 - 1e-12
                for point in expected_inner
                if point[2] <= POCKET_PHYSICS_THROAT_DEPTH_M + 1e-12))
            self.assertTrue(any(
                1e-12 < abs(point[0]) < width * 0.5 - 1e-12
                for point in expected_lower
                if point[2] > POCKET_PHYSICS_THROAT_DEPTH_M + 1e-12))
            self.assertTrue(any(
                abs(point[1] - POCKET_RECESSED_CAPTURE_WALL_Y_M) <= 1e-12
                for point in expected_lower
                if abs(point[2] - POCKET_PHYSICS_THROAT_DEPTH_M) <= 1e-12))
            self.assertAlmostEqual(min(point[1] for point in expected_lower),
                                   POCKET_RECESSED_BACK_WALL_Y_M, places=12)

            top_ring = [point for point in void[0]
                        if point[1] > -POCKET_PHYSICS_VOID_BOTTOM_M + 1e-12]
            self.assertTrue(top_ring)
            shallow_triangles = []
            for offset in range(0, len(void[4]), 3):
                triangle = [void[0][void[4][offset + index]]
                            for index in range(3)]
                if any(0.0 <= point[2] <= 0.060 and
                       abs(point[0]) < width * 0.5 - 1e-12 and
                       point[1] > -(BALL_DIAMETER_M + POCKET_VISUAL_SOLID_CLEARANCE_M)
                       for point in triangle):
                    shallow_triangles.append(triangle)
            self.assertFalse(
                shallow_triangles,
                "the cloth-to-cabinet undercut must be an actual open aperture")
            body_top_relative_y = POCKET_SHOULDER_TOP_Y_M - PLAY_SURFACE_Y_M
            self.assertTrue(any(
                abs(point[1] - body_top_relative_y) <= 1e-12 and
                (round(point[0], 12), round(point[2], 12)) in {
                    (round(item[0], 12), round(item[1], 12))
                    for item in body_seam}
                for point in rim[0]),
                "the dark undercut must meet the actual cabinet aperture")
            self.assertTrue(all(
                point[1] <= -POCKET_PHYSICS_CAVITY_DROP_M + 1e-12
                for point in top_ring if point[2] > 1e-12),
                "the outward capture corridor must not gain an upper visual solid")
            deep_vertices = [point for point in void[0]
                             if abs(point[1] -
                                    POCKET_RECESSED_DEEP_RING_Y_M) <= 1e-12]
            self.assertTrue(deep_vertices)
            for index, (x, depth) in enumerate(body_seam[1:-1], 1):
                self.assertTrue(any(
                    abs(point[0] - x) < 1e-12 and
                    abs(point[2] - depth - POCKET_LINER_WIDTH_M *
                        math.sin(math.pi * index / (len(body_seam) - 1))) < 1e-12
                    for point in deep_vertices),
                    "the far wall must descend vertically instead of contracting into a visible tube")
            floor_vertices = [
                point for point, normal in zip(void[0], void[1])
                if normal[1] > 0.999999]
            self.assertTrue(floor_vertices)
            self.assertTrue(all(
                abs(point[1] + POCKET_RECESSED_BAG_BOTTOM_M) <= 1e-12
                for point in floor_vertices))
            local_void = void[0]
            under_channel = [
                point for point in local_void
                if -1e-12 <= point[2] <=
                POCKET_PHYSICS_THROAT_DEPTH_M + 1e-12 and
                abs(point[0]) < width * 0.5 - 1e-12]
            self.assertTrue(under_channel)
            self.assertTrue(all(
                point[1] <= -(BALL_DIAMETER_M +
                              POCKET_VISUAL_SOLID_CLEARANCE_M) + 1e-12
                for point in under_channel),
                "the interior bag wall must stay below a complete ball sweep")
            self.assertGreaterEqual(
                max(point[0] for point in deep_vertices) -
                min(point[0] for point in deep_vertices),
                BALL_DIAMETER_M + 2.0 * POCKET_VISUAL_SOLID_CLEARANCE_M,
                "a descending ball must clear the deep bag ring laterally")
            capture_depths = [point[2] for point in deep_vertices]
            self.assertGreaterEqual(
                max(capture_depths) - POCKET_PHYSICS_THROAT_DEPTH_M,
                BALL_DIAMETER_M * 0.5 +
                POCKET_VISUAL_SOLID_CLEARANCE_M,
                "the deep ring must not clip the outward half of a falling ball")
            self.assertGreaterEqual(
                POCKET_PHYSICS_THROAT_DEPTH_M - min(capture_depths),
                BALL_DIAMETER_M * 0.5 +
                POCKET_VISUAL_SOLID_CLEARANCE_M,
                "the deep ring must not clip the inward half of a falling ball")
            legacy_above_floor = legacy[2][0]
            self.assertTrue(any(
                abs(point[0]) < width * 0.49 and
                point[1] < -POCKET_BACK_LIP_DROP_M - 1e-12
                for point in legacy_above_floor),
                "the negative control must retain the rejected full-height board")

            legacy_body = table_body_with_pocket_openings(True, 24)
            candidate_body = table_body_with_pocket_openings(
                True, 24, recessed_funnel_candidate=True)
            self.assertGreater(len(candidate_body[4]), len(legacy_body[4]))
            self.assertLessEqual(max(p[1] for p in candidate_body[0]),
                                 POCKET_SHOULDER_TOP_Y_M + 1e-12)
            self.assertTrue(any(abs(p[1] - POCKET_BODY_TOP_Y_M) < 1e-12
                                for p in candidate_body[0]))

        for sign in (-1.0, 1.0):
            arguments = (0.123, 0.040, POCKET_LINER_WIDTH_M, 0.0)
            keywords = {
                "smooth": True,
                "inward_cross_sign": sign,
                "width_profile": "quadratic",
            }
            legacy_facing = curved_leather_terminal(
                *arguments, **keywords, surface="facing")
            compact_facing = curved_leather_terminal(
                *arguments, **keywords, surface="facing",
                compact_throat_facing=True)
            compact_jaw = curved_leather_terminal(
                *arguments, **keywords, surface="jaw",
                compact_throat_facing=True)
            solid_jaw = curved_leather_terminal(
                *arguments, **keywords, surface="jaw_solid",
                compact_throat_facing=True)
            self.assertAlmostEqual(
                min(point[1] for point in legacy_facing[0]), -0.035,
                places=12)
            self.assertAlmostEqual(
                min(point[1] for point in compact_facing[0]), -0.018,
                places=12)
            self.assertAlmostEqual(
                max(point[1] for point in compact_facing[0]), -0.006,
                places=12)
            shared = {
                tuple(round(value, 12) for value in point)
                for point in compact_facing[0]
            } & {
                tuple(round(value, 12) for value in point)
                for point in compact_jaw[0]
            }
            self.assertEqual(len(shared), 2,
                             "compact facing must stay welded to the jaw nose")
            self.assertTrue(all(
                len({solid_jaw[4][offset + index] for index in range(3)}) == 3
                for offset in range(0, len(solid_jaw[4]), 3)))
            for offset in range(0, len(solid_jaw[4]), 3):
                triangle = [solid_jaw[0][solid_jaw[4][offset + index]]
                            for index in range(3)]
                edge_a = tuple(triangle[1][axis] - triangle[0][axis]
                               for axis in range(3))
                edge_b = tuple(triangle[2][axis] - triangle[0][axis]
                               for axis in range(3))
                cross = (edge_a[1] * edge_b[2] - edge_a[2] * edge_b[1],
                         edge_a[2] * edge_b[0] - edge_a[0] * edge_b[2],
                         edge_a[0] * edge_b[1] - edge_a[1] * edge_b[0])
                self.assertGreater(sum(value * value for value in cross),
                                   1e-24)
            mouth_vertices = [point for point in solid_jaw[0]
                              if abs(point[2]) <= 1e-12]
            self.assertEqual(
                len({tuple(round(value, 12) for value in point)
                     for point in mouth_vertices}), 2,
                "the solid jaw must taper to the shared edge without a cap")
            rail_vertices = [point for point in solid_jaw[0]
                             if abs(point[2] - arguments[0]) <= 1e-12]
            self.assertAlmostEqual(
                max(point[0] for point in rail_vertices) -
                min(point[0] for point in rail_vertices),
                POCKET_RECESSED_JAW_THICKNESS_M,
                places=12)

        with tempfile.TemporaryDirectory(
                prefix="billiardgl-phase2-recessed-funnel-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory,
                 "--recessed-funnel-candidate"],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True,
                timeout=60)
            self.assertEqual(completed.returncode, 0,
                             completed.stdout + completed.stderr)
            generated = Path(directory)
            document = json.loads((generated / ASSET.name).read_text(encoding="utf-8"))
            manifest = json.loads((generated / MANIFEST.name).read_text(encoding="utf-8"))
            errors, report = validate(document, manifest, generated)
            self.assertEqual(errors, [])
            self.assertEqual(report["unique_triangles"], 69017)
            self.assertFalse(any(
                node.get("extras", {}).get("asset_role") ==
                "pocket_facing_visual_only"
                for node in document["nodes"]))
            jaw = next(node for node in document["nodes"]
                       if node.get("extras", {}).get("asset_role") ==
                       "pocket_cushion_jaw_visual_only" and
                       node.get("extras", {}).get("lod") == 0)
            self.assertTrue(all(
                item.get("mouth_endpoint_welded_to_liner_lip") is True
                for item in jaw["extras"]["local_ranges"]))
            self.assertTrue(all(
                item.get("tapered_closed_cross_section") is True and
                abs(item.get("rail_end_thickness_m", 0.0) -
                    POCKET_RECESSED_JAW_THICKNESS_M) <= 1e-12 and
                item.get("independent_mouth_cap") is False
                for item in jaw["extras"]["local_ranges"]))

            prefix = "data:application/octet-stream;base64,"

            def primitive_for(source, node):
                return source["meshes"][node["mesh"]]["primitives"][0]

            def accessor_layout(source, node, semantic):
                primitive = primitive_for(source, node)
                accessor_index = (primitive["indices"] if semantic == "INDICES"
                                  else primitive["attributes"][semantic])
                accessor = source["accessors"][accessor_index]
                view = source["bufferViews"][accessor["bufferView"]]
                component_bytes = 4 if accessor["componentType"] in (5125, 5126) else 2
                components = 3 if accessor["type"] == "VEC3" else 1
                return (view.get("byteOffset", 0) +
                        accessor.get("byteOffset", 0),
                        view.get("byteStride", component_bytes * components),
                        accessor["count"], accessor["componentType"])

            def decoded_payload(source):
                return bytearray(base64.b64decode(
                    source["buffers"][0]["uri"][len(prefix):]))

            def commit_payload(source, payload):
                source["buffers"][0]["uri"] = prefix + \
                    base64.b64encode(payload).decode("ascii")

            def read_vec3(source, node, semantic, index, payload=None):
                payload = decoded_payload(source) if payload is None else payload
                base, stride, _, component_type = accessor_layout(
                    source, node, semantic)
                self.assertEqual(component_type, 5126)
                return struct.unpack_from("<3f", payload, base + index * stride)

            def write_vec3(source, node, semantic, index, value, payload):
                base, stride, _, component_type = accessor_layout(
                    source, node, semantic)
                self.assertEqual(component_type, 5126)
                struct.pack_into("<3f", payload, base + index * stride, *value)

            def read_indices(source, node, payload=None):
                payload = decoded_payload(source) if payload is None else payload
                base, stride, count, component_type = accessor_layout(
                    source, node, "INDICES")
                format_code = {5123: "H", 5125: "I"}[component_type]
                return [struct.unpack_from("<" + format_code, payload,
                                           base + index * stride)[0]
                        for index in range(count)]

            invalid = copy.deepcopy(document)
            node = next(item for item in invalid["nodes"]
                        if item.get("extras", {}).get("asset_role") ==
                        "pocket_void_visual_only" and
                        item.get("extras", {}).get("lod") == 0)
            del node["extras"]["local_ranges"][0]["central_closure"]
            errors, _ = validate(invalid, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("recessed open-well binding" in error
                                for error in errors), errors)

            # Reverse winding AND supplied normals together: ordinary
            # winding/normal agreement still holds, but an inner wall now
            # faces away from its cavity.  The semantic check must reject it.
            invalid = copy.deepcopy(document)
            node = next(item for item in invalid["nodes"]
                        if item.get("extras", {}).get("asset_role") ==
                        "pocket_inner_visual_only" and item["extras"]["lod"] == 0)
            payload = decoded_payload(invalid)
            first_range = node["extras"]["local_ranges"][0]
            start = first_range["vertex_start"]
            end = start + first_range["vertex_count"]
            for index in range(start, end):
                normal = read_vec3(invalid, node, "NORMAL", index, payload)
                write_vec3(invalid, node, "NORMAL", index,
                           tuple(-value for value in normal), payload)
            base, stride, count, component_type = accessor_layout(invalid, node, "INDICES")
            code = {5123:"H",5125:"I"}[component_type]
            for offset in range(0,count,3):
                tri = [struct.unpack_from("<"+code,payload,base+(offset+i)*stride)[0]
                       for i in range(3)]
                if all(start <= index < end for index in tri):
                    struct.pack_into("<"+code,payload,base+(offset+1)*stride,tri[2])
                    struct.pack_into("<"+code,payload,base+(offset+2)*stride,tri[1])
            commit_payload(invalid,payload)
            errors,_=validate(invalid,manifest,generated,verify_hashes=False)
            self.assertTrue(any("wall normal faces away from cavity" in error
                                for error in errors), errors)

            # Negative: invert one complete pocket's supplied normals.  The
            # validator must reject actual winding/normal disagreement, not
            # merely rely on the topology identity string.
            invalid = copy.deepcopy(document)
            node = next(item for item in invalid["nodes"]
                        if item.get("extras", {}).get("asset_role") ==
                        "pocket_liner_sidewall_visual_only" and
                        item.get("extras", {}).get("lod") == 0)
            first_range = node["extras"]["local_ranges"][0]
            payload = decoded_payload(invalid)
            for index in range(first_range["vertex_start"],
                               first_range["vertex_start"] +
                               first_range["vertex_count"]):
                normal = read_vec3(invalid, node, "NORMAL", index, payload)
                write_vec3(invalid, node, "NORMAL", index,
                           tuple(-value for value in normal), payload)
            commit_payload(invalid, payload)
            errors, _ = validate(invalid, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("reversed or inconsistent surface normals"
                                in error for error in errors), errors)

            # Negative: reuse one existing void triangle as a vertical cap
            # spanning the mouth.  It must be caught geometrically even if
            # the metadata still claims mouth_front_cap=False.
            invalid = copy.deepcopy(document)
            node = next(item for item in invalid["nodes"]
                        if item.get("extras", {}).get("asset_role") ==
                        "pocket_void_visual_only" and
                        item.get("extras", {}).get("lod") == 0)
            first_range = node["extras"]["local_ranges"][0]
            indices = read_indices(invalid, node)
            triangle = next(
                indices[offset:offset + 3]
                for offset in range(0, len(indices), 3)
                if all(first_range["vertex_start"] <= index <
                       first_range["vertex_start"] + first_range["vertex_count"]
                       for index in indices[offset:offset + 3]))
            pocket = next(item for item in POCKETS
                          if item[0] == first_range["pocket_id"])
            center, inward, tangent, width = pocket[3], pocket[4], pocket[5], pocket[6]
            payload = decoded_payload(invalid)
            for index, (local_x, local_y) in zip(
                    triangle,
                    ((-width * 0.5, -0.060),
                     (width * 0.5, -0.060), (0.0, -0.105))):
                world = (center[0] + tangent[0] * local_x,
                         PLAY_SURFACE_Y_M + local_y,
                         center[1] + tangent[1] * local_x)
                write_vec3(invalid, node, "POSITION", index, world, payload)
            commit_payload(invalid, payload)
            errors, _ = validate(invalid, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("closes the visible mouth with a front cap"
                                in error for error in errors), errors)

            # Negative: push one sidewall vertex into the otherwise exact
            # 0..6 cm physical corridor.
            invalid = copy.deepcopy(document)
            node = next(item for item in invalid["nodes"]
                        if item.get("extras", {}).get("asset_role") ==
                        "pocket_liner_sidewall_visual_only" and
                        item.get("extras", {}).get("lod") == 0)
            first_range = node["extras"]["local_ranges"][0]
            pocket = next(item for item in POCKETS
                          if item[0] == first_range["pocket_id"])
            center, inward, tangent, width = pocket[3], pocket[4], pocket[5], pocket[6]
            payload = decoded_payload(invalid)
            selected = None
            for index in range(first_range["vertex_start"],
                               first_range["vertex_start"] +
                               first_range["vertex_count"]):
                position = read_vec3(invalid, node, "POSITION", index, payload)
                dx, dz = position[0] - center[0], position[2] - center[1]
                depth = -(dx * inward[0] + dz * inward[1])
                local_x = dx * tangent[0] + dz * tangent[1]
                if 0.015 < depth < 0.045 and \
                        abs(local_x) > width * 0.49:
                    selected = (index, position[1], depth)
                    break
            self.assertIsNotNone(selected)
            index, _world_y, depth = selected
            write_vec3(
                invalid, node, "POSITION", index,
                (center[0] - inward[0] * depth,
                 PLAY_SURFACE_Y_M - 0.020,
                 center[1] - inward[1] * depth), payload)
            commit_payload(invalid, payload)
            errors, _ = validate(invalid, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("intrudes into the full-width 0..6cm corridor"
                                in error for error in errors), errors)

            # Negative: move the compact jaw's upper mouth endpoint in Y.
            # X/Z remain aligned, so a metadata-only or plan-only check would
            # miss the unwelded visual cap.
            invalid = copy.deepcopy(document)
            node = next(item for item in invalid["nodes"]
                        if item.get("extras", {}).get("asset_role") ==
                        "pocket_cushion_jaw_visual_only" and
                        item.get("extras", {}).get("lod") == 0)
            first_range = node["extras"]["local_ranges"][0]
            pocket = next(item for item in POCKETS
                          if item[0] == first_range["pocket_id"])
            sign = first_range["terminal_sign"]
            center, tangent, width = pocket[3], pocket[5], pocket[6]
            guard = (center[0] + tangent[0] * sign * width * 0.5,
                     center[1] + tangent[1] * sign * width * 0.5)
            payload = decoded_payload(invalid)
            targets = []
            for index in range(first_range["vertex_start"],
                               first_range["vertex_start"] +
                               first_range["vertex_count"]):
                position = read_vec3(
                    invalid, node, "POSITION", index, payload)
                squared_distance = (
                    (position[0] - guard[0]) ** 2 +
                    (position[2] - guard[1]) ** 2 +
                    (position[1] - (PLAY_SURFACE_Y_M - 0.006)) ** 2)
                if squared_distance < 1e-10:
                    targets.append((index, position))
            self.assertTrue(targets)
            for target, position in targets:
                write_vec3(
                    invalid, node, "POSITION", target,
                    (position[0], position[1] - 0.002, position[2]), payload)
            commit_payload(invalid, payload)
            errors, _ = validate(invalid, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("jaw mouth endpoint is not welded"
                                in error for error in errors), errors)

    def test_generated_v10c_pocket_profile_enforces_connected_topology_and_halfspaces(self):
        with tempfile.TemporaryDirectory(prefix="billiardgl-phase2-v10c-") as directory:
            completed = subprocess.run(
                [sys.executable, str(GENERATOR), "--output-dir", directory,
                 "--legacy-unsplit-pocket-facing"],
                cwd=str(REPOSITORY_ROOT), text=True, capture_output=True, timeout=60)
            self.assertEqual(completed.returncode, 0, completed.stdout + completed.stderr)
            generated = Path(directory)
            document = json.loads((generated / ASSET.name).read_text(encoding="utf-8"))
            manifest = json.loads((generated / MANIFEST.name).read_text(encoding="utf-8"))
            errors, _ = validate(document, manifest, generated)
            self.assertEqual(errors, [])

            def move_first_vertex_across_plane(source, role, inward_sign):
                invalid = copy.deepcopy(source)
                node = next(item for item in invalid["nodes"]
                            if item.get("extras", {}).get("asset_role") == role and
                            item.get("extras", {}).get("lod") == 0)
                pocket_id = node["extras"]["local_ranges"][0]["pocket_id"]
                pocket = next(item for item in POCKETS if item[0] == pocket_id)
                center, inward = pocket[3], pocket[4]
                mesh = invalid["meshes"][node["mesh"]]
                accessor = invalid["accessors"][
                    mesh["primitives"][0]["attributes"]["POSITION"]]
                view = invalid["bufferViews"][accessor["bufferView"]]
                prefix = "data:application/octet-stream;base64,"
                payload = bytearray(base64.b64decode(invalid["buffers"][0]["uri"][len(prefix):]))
                offset = view.get("byteOffset", 0) + accessor.get("byteOffset", 0)
                distance = 0.01 * inward_sign
                struct.pack_into("<3f", payload, offset,
                                 center[0] + inward[0] * distance,
                                 0.82,
                                 center[1] + inward[1] * distance)
                invalid["buffers"][0]["uri"] = prefix + base64.b64encode(payload).decode("ascii")
                return invalid

            invalid_liner = move_first_vertex_across_plane(
                document, "pocket_liner_rim_visual_only", -1.0)
            errors, _ = validate(invalid_liner, manifest, generated, verify_hashes=False)
            self.assertTrue(any("throat/bottom geometry outside mouth plane" in error
                                for error in errors), errors)

            invalid_contract = copy.deepcopy(document)
            liner = next(item for item in invalid_contract["nodes"]
                         if item.get("extras", {}).get("asset_role") ==
                         "pocket_liner_rim_visual_only")
            liner["extras"]["shape"] = "rectangular_slot"
            errors, _ = validate(invalid_contract, manifest, generated, verify_hashes=False)
            self.assertTrue(any("v10c rim contract" in error for error in errors), errors)

            invalid_seam = copy.deepcopy(document)
            sidewall = next(item for item in invalid_seam["nodes"]
                            if item.get("extras", {}).get("asset_role") ==
                            "pocket_liner_sidewall_visual_only")
            sidewall["extras"]["local_ranges"][0]["upper_boundary"][0][0] += 0.001
            errors, _ = validate(invalid_seam, manifest, generated, verify_hashes=False)
            self.assertTrue(any("boundaries are not position matched" in error
                                for error in errors), errors)

            invalid_frame = copy.deepcopy(document)
            invalid_frame["asset"]["extras"]["pocket_visual_frame"][
                "marker_z_mapping"] = "z_visual=z_marker"
            errors, _ = validate(invalid_frame, manifest, generated, verify_hashes=False)
            self.assertTrue(any("frame mapping or handedness" in error for error in errors),
                            errors)

            invalid_marker_sha = copy.deepcopy(document)
            invalid_marker_sha["asset"]["extras"]["physics_marker_binding_sha256"] = \
                "sha256:" + "0" * 64
            errors, _ = validate(invalid_marker_sha, manifest, generated,
                                 verify_hashes=False)
            self.assertTrue(any("marker binding SHA-256" in error for error in errors), errors)


if __name__ == "__main__":
    unittest.main()
