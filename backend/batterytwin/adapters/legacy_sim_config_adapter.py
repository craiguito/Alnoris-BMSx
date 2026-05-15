from __future__ import annotations

from typing import Any

from backend.batterytwin.schemas.results import BatterySimulationResult
from backend.batterytwin.schemas.scenarios import BatteryScenario
from backend.batterytwin.schemas.states import (
    BatteryBMSState,
    BatteryDegradationState,
    BatteryElectricalState,
    BatteryThermalState,
)
from backend.sim_core.bridge import simulation_result_to_dict
from backend.sim_core.types import (
    CurrentProfile,
    CurrentProfilePoint,
    ElectricalModelConfig,
    SimulationConfig,
    SimulationResult,
)
from backend.sim_core.validation import validate_simulation_config
from backend.twincore.schemas.provenance import ProvenanceRecord
from backend.twincore.schemas.simulation import ResultPackage, RunManifest, SimulationArtifact
from backend.twincore.schemas.validation import CredibilityCard


def _scenario_id(manifest: RunManifest) -> str:
    identity = getattr(manifest.scenario, "identity", None)
    return getattr(identity, "id", "")


def _current_profile_from_points(points: tuple[tuple[int, float], ...]) -> CurrentProfile | None:
    if not points:
        return None
    return CurrentProfile(
        points=tuple(
            CurrentProfilePoint(time_s=int(time_s), current_a=float(current_a))
            for time_s, current_a in points
        )
    )


def battery_scenario_to_legacy_simulation_config(scenario: BatteryScenario) -> SimulationConfig:
    """Translate a BatteryTwin scenario into the existing sim_core config."""

    pack = scenario.pack
    cell = pack.cell
    group_count = pack.resolved_group_count()
    config = SimulationConfig(
        cell_nominal_voltage=cell.cell_nominal_voltage,
        cell_full_voltage=cell.cell_full_voltage,
        cell_empty_voltage=cell.cell_empty_voltage,
        cell_cutoff_voltage=cell.cell_cutoff_voltage,
        cell_capacity_ah=cell.cell_capacity_ah,
        cells_in_series=pack.cells_in_series,
        cells_in_parallel=pack.cells_in_parallel,
        internal_resistance_ohm_per_cell=cell.internal_resistance_ohm,
        ambient_temp_c=scenario.ambient_temp_c,
        discharge_current_a=scenario.discharge_current_a,
        duration_s=int(scenario.duration_s),
        time_step_s=int(scenario.time_step_s),
        initial_soc=scenario.initial_soc,
        pack_mass_kg=pack.resolved_pack_mass_kg(),
        pack_heat_capacity_j_per_kgk=pack.resolved_pack_heat_capacity_j_per_kgk(),
        cooling_coeff_w_per_k=pack.cooling_coeff_w_per_k,
        electrical_model=ElectricalModelConfig(
            model_type="rint",
            r0_ohm_per_cell=cell.internal_resistance_ohm,
        ),
        current_profile=_current_profile_from_points(scenario.current_profile),
        group_count=group_count,
        group_zone_assignments=pack.resolved_group_zone_assignments(),
        group_labels=pack.resolved_group_labels(),
        group_entity_ids=pack.resolved_group_entity_ids(),
        chemistry_name=cell.chemistry_name,
    )
    if scenario.legacy_config_overrides:
        config = SimulationConfig(**{**config.__dict__, **scenario.legacy_config_overrides})
    return validate_simulation_config(config)


def _final_states(payload: dict[str, Any], result: SimulationResult) -> tuple[
    BatteryElectricalState,
    BatteryThermalState,
    BatteryDegradationState,
    BatteryBMSState,
]:
    summary = payload["summary"]
    final_point = payload.get("time_series", [{}])[-1] if payload.get("time_series") else {}
    electrical = BatteryElectricalState(
        pack_voltage_v=float(final_point.get("pack_voltage_v", result.pack_nominal_voltage_v)),
        pack_current_a=float(final_point.get("current_a", 0.0)),
        pack_power_w=float(final_point.get("pack_power_w", 0.0)),
        soc_avg=float(summary.get("final_soc_avg", summary.get("final_soc", 0.0))),
        soc_min=float(final_point.get("soc_min", summary.get("final_soc_avg", 0.0))),
        soc_max=float(final_point.get("soc_max", summary.get("final_soc_avg", 0.0))),
    )
    thermal = BatteryThermalState(
        pack_temp_avg_c=float(final_point.get("pack_temp_avg_c", summary.get("max_group_temp_c", 25.0))),
        pack_temp_max_c=float(final_point.get("pack_temp_max_c", summary.get("max_group_temp_c", 25.0))),
        max_core_temp_c=float(summary.get("max_core_temp_c", summary.get("max_group_temp_c", 25.0))),
        max_surface_temp_c=float(summary.get("max_surface_temp_c", summary.get("max_group_temp_c", 25.0))),
    )
    degradation = BatteryDegradationState(
        capacity_retention=float(summary.get("capacity_retention", summary.get("estimated_capacity_retention", 1.0))),
        resistance_growth=float(summary.get("resistance_growth", summary.get("estimated_resistance_growth", 0.0))),
        cumulative_charge_throughput_ah=float(summary.get("cumulative_charge_throughput_ah", 0.0)),
        cumulative_discharge_throughput_ah=float(summary.get("cumulative_discharge_throughput_ah", 0.0)),
    )
    bms = BatteryBMSState(
        balancing_active=bool(summary.get("balancing_used", False)),
        fault_count=int(summary.get("fault_count", 0)),
        warning_codes=tuple(str(warning.get("code", "")) for warning in summary.get("warnings", [])),
        termination_reason=str(summary.get("termination_reason", "")),
    )
    return electrical, thermal, degradation, bms


def legacy_simulation_result_to_battery_result(
    result: SimulationResult,
    manifest: RunManifest,
) -> BatterySimulationResult:
    payload = simulation_result_to_dict(result)
    electrical, thermal, degradation, bms = _final_states(payload, result)
    return BatterySimulationResult(
        run_id=manifest.run_id,
        scenario_id=_scenario_id(manifest),
        pack_nominal_voltage_v=result.pack_nominal_voltage_v,
        pack_capacity_ah=result.pack_capacity_ah,
        theoretical_energy_wh=result.theoretical_energy_wh,
        summary=dict(payload["summary"]),
        time_series=tuple(dict(point) for point in payload.get("time_series", [])),
        final_electrical_state=electrical,
        final_thermal_state=thermal,
        final_degradation_state=degradation,
        final_bms_state=bms,
        legacy_result=result,
        metadata={"time_series_metadata": payload.get("time_series_metadata", {})},
    )


def legacy_simulation_result_to_result_package(
    result: SimulationResult,
    manifest: RunManifest,
    *,
    solver_id: str,
    solver_version: str,
    fidelity: str = "screening",
) -> ResultPackage:
    battery_result = legacy_simulation_result_to_battery_result(result, manifest)
    provenance = ProvenanceRecord(
        source="backend.sim_core.engine.run_simulation",
        generated_by=solver_id,
        input_ids=tuple(item for item in (battery_result.scenario_id, manifest.run_id) if item),
        metadata={"adapter": "legacy_sim_config_adapter", "solver_version": solver_version},
    )
    credibility_card = CredibilityCard(
        credibility_level=fidelity,
        assumptions=(
            "Wrapped legacy reduced-order pack ECM solver.",
            "Phase 1 result package preserves sim_core native outputs.",
        ),
        limitations=(
            "Screening fidelity only; not a certification model.",
            "Persistence and full asset graph conversion are deferred.",
        ),
    )
    return ResultPackage(
        solver_id=solver_id,
        solver_version=solver_version,
        run_id=manifest.run_id,
        provenance_id=provenance.id,
        credibility_card=credibility_card,
        artifacts=(
            SimulationArtifact(
                artifact_type="battery_simulation_result",
                name="Battery Simulation Result",
                payload=battery_result,
            ),
        ),
        summary=dict(battery_result.summary),
        manifest=manifest,
        provenance=provenance,
        metadata={"fidelity": fidelity},
    )
