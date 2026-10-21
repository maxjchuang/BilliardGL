"""Short multi-shot route lessons and public-physics calibration."""
import copy
import json
import math
import hashlib
import tempfile
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT))
sys.path.insert(0, str(ROOT / 'scripts'))
from calibrate_phase4_reference_replays import AutomationClient, ghost_yaw, all_trace_frames, facts_from_frames

IDS = ['launch_two_ball_lane_route_v2', 'launch_three_ball_corner_route_v2']
LAYOUTS = [[(0,10,0),(1,40,0),(2,-40,0)],
           [(0,10,0),(3,40,0),(4,-40,0),(5,-53,80)]]
POCKETS = [[(63.5,0),(-63.5,0)],[(63.5,0),(-63.5,0),(-63.5,127)]]
POCKET_IDS = [['side_px','side_nx'], ['side_px','side_nx','corner_nx_pz']]
POWERS = [[55,65],[55,65,75]]
REFERENCE_ACTIONS = {IDS[0]:[(0,55,0,0),(math.pi,65,0,0)],
                     IDS[1]:[(0,55,0,0),(math.pi,65,0,0),(1.748847335159477,75,0,0)]}

def cond(predicate, **kw):
    return dict(predicate=predicate, **kw)

def zone(index,x,z,radius):
    return cond('ball_final_position_in_zone',ball_index=index,center_cm=[x,z],radius_cm=radius)

def build_remaining_routes():
    result=[]
    for i,sid in enumerate(IDS):
        order=[b[0] for b in LAYOUTS[i] if b[0]]
        hints=[]
        starts=[(10,0),(38.53441619873047,0),(-38.36112976074219,-0.000023358388716587797)]
        for j,(idx,x,z) in enumerate(LAYOUTS[i][1:]):
            target=POCKETS[i][j]
            hints.append({'recommended_power':{'minimum_percent':(POWERS[i][j]-2.5)/2,
                'maximum_percent':(POWERS[i][j]+2.5)/2,'default_percent':POWERS[i][j]/2},
                'recommended_tip':{'x_ball_radii':0,'y_ball_radii':0,'label':'center_ball'},
                'aim':{'yaw_radians':ghost_yaw(starts[j],(x,z),target),'tolerance_degrees':0.2},
                'aim_at_pocket':True,'target_pocket_enabled':True,'target_pocket_id':POCKET_IDS[i][j],
                'target_pocket_center_cm':list(target),
                'cue_ball_target_zone':{'center_cm':[-38,0] if i==0 else [-52,84], 'radius_cm':10,'visible':j==len(order)-1},
                'shot_line':{'cue_ball_index':0,'object_ball_index':idx,'start_cm':list(starts[j]),'object_center_cm':[x,z]},
                'goal_en':f'Pot ball {idx}; cue: no rails' + (' / finish in ring' if j==len(order)-1 else ' / prepare next shot')})
        h=copy.deepcopy(hints[0]);h['shot_steps']=hints
        result.append(dict(scenario_id=sid,category='continuous_position',
            title_zh='两球连续进球' if i==0 else '三球连续进球',
            title_en='Two-ball short route' if i==0 else 'Three-ball short route',
            description_zh='按顺序进球，母球不碰库，最后停入目标区。中间杆仅引导下一颗球，不强制停球区。',
            description_en='Pot balls in order without cue-ball rails; only the final shot must finish in the ring.',
            tags=['launch13','position_play','beginner','two_shot' if i==0 else 'three_shot'],balls=LAYOUTS[i],
            success_conditions=[cond('ordered_ball_pockets',ball_indices=order),cond('ball_not_pocketed',ball_index=0),
             cond('rail_contact_count_lte',ball_index=0,maximum=0),zone(0,*hints[-1]['cue_ball_target_zone']['center_cm'],10)] +
             [cond('ball_pocketed_in_pocket',ball_index=idx,pocket_id=pid) for idx,pid in zip(order,[5,4,2])],
            par_shots=len(order),tip_axis='none',preferred_tip=[0,0],content_version=2,teaching_hints=h))
    return result

def build_scenarios():
    from generate_phase4_training_pack import scenario, ball
    result=[]
    for spec in build_remaining_routes():
        d=copy.deepcopy(spec); d['success']=d.pop('success_conditions');d['balls']=[ball(*b) for b in d['balls']]
        d['reference_audits']={sid:('0'*64,'0'*64) for sid in IDS};d['reference_directory']='replays_v2'
        d['reject_cue_ball_pocketed_during_attempt']=True
        result.append(scenario(**d))
    return result

def run(binary, output):
    output=Path(output);output.mkdir(parents=True,exist_ok=True)
    reports=[]
    for i,s in enumerate(build_scenarios()):
        p=output/(s['id']+'.training.json');p.write_text(json.dumps(s))
        with AutomationClient(binary) as c:
            c.command('training_load',{'schema_version':1,'path':str(p)})
            c.start_physics_trace();actions=[];steps=[]
            for j,b in enumerate(LAYOUTS[i][1:]):
                state=c.state(); cue=state['balls'][0]['position']; ob=state['balls'][b[0]]['position']
                yaw=ghost_yaw((cue['x'],cue['z']),(ob['x'],ob['z']),POCKETS[i][j])
                power=POWERS[i][j];actions.append((yaw,power,0,0))
                c.set_aim_yaw(yaw);c.set_shot_power(power);c.shoot();c.run_until('balls_stopped',max_steps=6000)
                state=c.state(); steps.append({'cue':state['balls'][0]['position'],'object':state['balls'][b[0]],'tick':state['tick']})
            facts=facts_from_frames(all_trace_frames(c,state['tick']))
            score=c.command('training_score',{'schema_version':1,'shot_count':len(actions),'elapsed_ticks':state['tick'],**facts})
            reports.append({'id':s['id'],'actions':actions,'steps':steps,'facts':facts,'score':score})
    (output/'report.json').write_text(json.dumps(reports,indent=2));print(json.dumps(reports,indent=2))

def verify(binary, output):
    """Vary each shot independently; continue from its real settled state."""
    binary_sha=hashlib.sha256(Path(binary).read_bytes()).hexdigest()
    output=Path(output);output.mkdir(parents=True,exist_ok=True);reports=[];scenario_hashes={}
    temporary=tempfile.TemporaryDirectory(prefix='billiardgl-routes-')
    for i,s in enumerate(build_scenarios()):
        p=Path(temporary.name)/(s['id']+'.training.json');p.write_text(json.dumps(s))
        scenario_hashes[s['id']]=hashlib.sha256(p.read_bytes()).hexdigest()
        trials=[(-1,0,0)] + [(j,dp,dy) for j in range(len(POWERS[i]))
            for dp in (-2.5,2.5) for dy in (-0.2,0.2)]
        # Also perturb every shot in the same attempt so earlier positioning
        # deviations propagate into later dynamic aiming, rather than testing
        # each recommendation only from the exact reference position.
        trials += [(-2,dp,dy) for dp in (-2.5,0,2.5) for dy in (-0.2,0,0.2)
                   if dp or dy]
        with AutomationClient(binary) as c:
            for changed,dp,dy in trials:
                c.reset();c.command('training_load',{'schema_version':1,'path':str(p)});c.start_physics_trace()
                actions=[];steps=[]
                for j,b in enumerate(LAYOUTS[i][1:]):
                    state=c.state();cue=state['balls'][0]['position'];ob=state['balls'][b[0]]['position']
                    varying=j==changed or changed==-2
                    yaw=ghost_yaw((cue['x'],cue['z']),(ob['x'],ob['z']),POCKETS[i][j]) + (math.radians(dy) if varying else 0)
                    power=POWERS[i][j]+(dp if varying else 0);actions.append([yaw,power,0,0])
                    c.set_aim_yaw(yaw);c.set_shot_power(power);c.shoot();c.run_until('balls_stopped',max_steps=6000)
                    state=c.state();steps.append({'cue_position_cm':state['balls'][0]['position'],'target_pocketed':state['balls'][b[0]]['pocketed']})
                facts=facts_from_frames(all_trace_frames(c,state['tick']))
                score=c.command('training_score',{'schema_version':1,'shot_count':len(actions),'elapsed_ticks':state['tick'],**facts})
                reports.append({'id':s['id'],'changed_shot':changed,'power_delta_units':dp,'yaw_delta_degrees':dy,'actions':actions,'steps':steps,'facts':facts,'score':score})
    if hashlib.sha256(Path(binary).read_bytes()).hexdigest()!=binary_sha:
        raise RuntimeError('Executable changed during calibration; rerun after build completes')
    report={'binary_sha256':binary_sha,
        'calibration_scenario_sha256':scenario_hashes,
        'module_sha256':hashlib.sha256(Path(__file__).read_bytes()).hexdigest(),
        'method':'real_public_automation_sequential_shots_independent_and_whole_sequence_perturbations',
        'attempt_count':len(reports),'all_succeeded':all(x['score']['succeeded'] for x in reports),'attempts':reports}
    (output/'remaining_routes.range.json').write_text(json.dumps(report,indent=2))
    temporary.cleanup()
    print(json.dumps({k:v for k,v in report.items() if k!='attempts'}))
    for r in reports:
        if not r['score']['succeeded']: print(json.dumps(r))

if __name__=='__main__':
    (verify if '--verify' in sys.argv else run)(sys.argv[1],sys.argv[2])
