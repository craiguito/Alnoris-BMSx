from __future__ import annotations

from backend.batterytwin.schemas.components import BatteryCellTwin, BatteryPackTwin, CellGroupTwin
from backend.batterytwin.schemas.scenarios import BatteryScenario
from backend.sim_core.reference_cells import REFERENCE_CELLS
from backend.sim_core.system_presets import BatterySystemPreset, build_simulation_config_for_preset, get_system_preset, validate_system_preset
from backend.twincore.schemas.identity import IdentityRef


def _bt_id(preset: BatterySystemPreset, *segments: object, project_id: str | None = None) -> str:
    parts = ["batterytwin"]
    if project_id:
        parts.append(project_id)
    parts.extend([preset.preset_id, *(str(segment) for segment in segments)])
    return ":".join(parts)


def preset_to_battery_scenario(preset: BatterySystemPreset, project_id: str | None = None) -> BatteryScenario:
    validate_system_preset(preset)
    legacy_config = build_simulation_config_for_preset(preset)
    cell_ref = REFERENCE_CELLS[preset.cell.key]
    cell = BatteryCellTwin(
        name=cell_ref.name,
        chemistry_name=preset.chemistry_name,
        cell_nominal_voltage=cell_ref.cell_nominal_voltage,
        cell_full_voltage=cell_ref.cell_full_voltage,
        cell_empty_voltage=cell_ref.cell_empty_voltage,
        cell_cutoff_voltage=cell_ref.cell_cutoff_voltage,
        cell_capacity_ah=cell_ref.cell_capacity_ah,
        internal_resistance_ohm=cell_ref.internal_resistance_ohm_per_cell,
        mass_kg=cell_ref.pack_mass_kg,
        heat_capacity_j_per_kgk=cell_ref.pack_heat_capacity_j_per_kgk,
        identity=IdentityRef(kind="battery_cell", name=cell_ref.name, id=_bt_id(preset, "cell", "representative", project_id=project_id)),
        metadata={
            "project_id": project_id or "",
            "preset_id": preset.preset_id,
            "cell_preset_key": preset.cell.key,
            "form_factor": preset.cell.form_factor,
        },
    )
    groups = tuple(
        CellGroupTwin(
            group_index=index,
            label=f"Group {index + 1}",
            cells_in_series=1,
            cells_in_parallel=preset.pack.parallel_count,
            thermal_zone_id=legacy_config.group_zone_assignments[index],
            cell=cell,
            identity=IdentityRef(kind="cell_group", name=f"Group {index + 1}", id=_bt_id(preset, "group", index, project_id=project_id)),
            metadata={"project_id": project_id or "", "series_index": index, "preset_id": preset.preset_id},
        )
        for index in range(preset.pack.series_count)
    )
    pack = BatteryPackTwin(
        name=preset.display_name,
        cell=cell,
        cells_in_series=preset.pack.series_count,
        cells_in_parallel=preset.pack.parallel_count,
        group_count=preset.pack.series_count,
        pack_mass_kg=preset.pack_mass_kg,
        pack_heat_capacity_j_per_kgk=preset.pack_heat_capacity_j_per_kgk,
        cooling_coeff_w_per_k=preset.cooling_coeff_w_per_k,
        cell_groups=groups,
        identity=IdentityRef(kind="battery_pack", name=preset.display_name, id=_bt_id(preset, "pack", project_id=project_id)),
        metadata={
            "project_id": project_id or "",
            "preset_id": preset.preset_id,
            "category": preset.category,
            "chemistry_display_name": preset.chemistry_display_name,
        },
    )
    return BatteryScenario(
        name=f"{preset.display_name} Screening Scenario",
        pack=pack,
        ambient_temp_c=preset.ambient_temp_c,
        discharge_current_a=preset.discharge_current_a,
        duration_s=preset.duration_s,
        time_step_s=preset.time_step_s,
        initial_soc=preset.initial_soc,
        legacy_config_overrides={
            "balancing": legacy_config.balancing,
            "thermal_zones": legacy_config.thermal_zones,
            "group_zone_assignments": legacy_config.group_zone_assignments,
            "group_labels": legacy_config.group_labels,
            "group_entity_ids": legacy_config.group_entity_ids,
            "physics": legacy_config.physics,
        },
        identity=IdentityRef(kind="battery_scenario", name=preset.display_name, id=_bt_id(preset, "scenario", "default", project_id=project_id)),
        metadata={
            "project_id": project_id or "",
            "preset_id": preset.preset_id,
            "operating_limits": dict(preset.operating_limits),
            "recommended_virtual_tests": list(preset.recommended_virtual_tests),
        },
    )


def build_default_battery_scenario_from_preset_id(preset_id: str) -> BatteryScenario:
    return preset_to_battery_scenario(get_system_preset(preset_id))
