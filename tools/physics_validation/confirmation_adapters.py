import copy
from dataclasses import dataclass
from typing import Callable


@dataclass(frozen=True)
class ConfirmationEvaluation:
    rows: tuple
    summary_metrics: dict
    diagnostics: tuple = ()


@dataclass(frozen=True)
class ConfirmationAdapter:
    dataset_id: str
    build_scenarios: Callable[[dict, object], dict]
    evaluate: Callable[[dict, dict, object], ConfirmationEvaluation]


_ADAPTERS = {}


def register_confirmation_adapter(adapter):
    if adapter.dataset_id in _ADAPTERS:
        raise ValueError(
            f"duplicate confirmation adapter: {adapter.dataset_id}")
    _ADAPTERS[adapter.dataset_id] = adapter


def confirmation_adapter(dataset_id):
    try:
        return _ADAPTERS[dataset_id]
    except KeyError as error:
        raise ValueError(
            f"unsupported confirmation package: {dataset_id}") from error


def base_scenario(profile, scenario_id, balls, boundary_mode, ticks,
                  description, *, evidence_source="confirmation_contract"):
    return {
        "schema_version": 11,
        "id": scenario_id,
        "description": description,
        "boundary_mode": boundary_mode,
        "physics_profile": copy.deepcopy(profile),
        "balls": balls,
        "simulation": {
            "ticks": ticks,
            "time_step_seconds": profile["solver"]["time_step_seconds"],
        },
        "expectations": [
            {"metric": "finite_state", "operator": "eq", "value": True},
            {
                "metric": "nonincreasing_translational_energy",
                "operator": "eq",
                "value": True,
            },
        ],
        "evidence": {
            "equipment": "WPA_POOL",
            "grade": "B",
            "pool_applicability": "DIRECT",
            "source": evidence_source,
        },
    }


def scenario_ball(index, position, velocity, radius, rolling=True):
    speed_x = velocity[0]
    spin_z = -speed_x / radius if rolling else 0.0
    return {
        "index": index,
        "position_cm": position,
        "velocity_cm_s": velocity,
        "angular_velocity_rad_s": [0.0, 0.0, spin_z],
        "pocketed": False,
    }


def execute_deterministically(executable, scenarios, execute_once):
    traces = {}
    for key, scenario in scenarios.items():
        first = execute_once(executable, scenario)
        second = execute_once(executable, scenario)
        if first != second:
            raise RuntimeError(
                f"confirmation trace is nondeterministic: {key}")
        if len(first) != scenario["simulation"]["ticks"]:
            raise RuntimeError(f"confirmation trace is incomplete: {key}")
        traces[key] = first
    return traces
