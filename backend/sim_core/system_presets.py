from __future__ import annotations

"""Battery system presets.

These presets are pack-level engineering starting points, not exact production
pack definitions. Each preset can generate both simulation defaults and a
battery-aware CAD layout description without relying on imported mesh assets.
"""

from dataclasses import asdict, dataclass, field
from math import ceil

from .calibration import apply_calibration_to_simulation_config, calibrate_parameters, load_truth_dataset
from .reference_cells import REFERENCE_CELLS
from .test_catalog import build_test_catalog
from .types import BalancingConfig, ElectricalModelConfig, PhysicsConfig, SimulationConfig, ThermalZoneConfig
from .validation import validate_simulation_config


@dataclass(frozen=True)
class CellPresetRef:
    key: str
    form_factor: str
    radius_mm: float = 28.0
    height_mm: float = 220.0
    width_mm: float = 56.0
    depth_mm: float = 56.0


@dataclass(frozen=True)
class ModulePreset:
    module_count: int
    module_gap_x_mm: float = 64.0
    busbar_thickness_mm: float = 12.0
    cooling_channel_thickness_mm: float = 24.0
    enclosure_wall_thickness_mm: float = 8.0


@dataclass(frozen=True)
class PackPreset:
    series_count: int
    parallel_count: int
    x_spacing_mm: float
    z_spacing_mm: float


@dataclass(frozen=True)
class BatterySystemPreset:
    preset_id: str
    display_name: str
    description: str
    category: str
    chemistry_name: str
    chemistry_display_name: str
    cell: CellPresetRef
    module: ModulePreset
    pack: PackPreset
    ambient_temp_c: float
    cooling_coeff_w_per_k: float
    pack_mass_kg: float
    pack_heat_capacity_j_per_kgk: float
    discharge_current_a: float
    duration_s: int = 1800
    time_step_s: int = 1
    initial_soc: float = 0.95
    balancing_enabled: bool = False
    balancing_bleed_current_a: float = 0.0
    balancing_soc_threshold: float | None = None
    thermal_zone_count: int = 1
    thermal_zone_cooling_multipliers: tuple[float, ...] = (1.0,)
    interconnect_resistance_ohm_per_group: float = 0.0
    pack_interconnect_resistance_ohm: float = 0.0
    recommended_virtual_tests: tuple[str, ...] = field(default_factory=tuple)
    operating_limits: dict[str, float] = field(default_factory=dict)


def _cell(
    key: str,
    *,
    form_factor: str,
    radius_mm: float = 28.0,
    height_mm: float = 220.0,
    width_mm: float = 56.0,
    depth_mm: float = 56.0,
) -> CellPresetRef:
    return CellPresetRef(
        key=key,
        form_factor=form_factor,
        radius_mm=radius_mm,
        height_mm=height_mm,
        width_mm=width_mm,
        depth_mm=depth_mm,
    )


def _module(
    module_count: int,
    *,
    module_gap_x_mm: float = 64.0,
    busbar_thickness_mm: float = 12.0,
    cooling_channel_thickness_mm: float = 24.0,
    enclosure_wall_thickness_mm: float = 8.0,
) -> ModulePreset:
    return ModulePreset(
        module_count=module_count,
        module_gap_x_mm=module_gap_x_mm,
        busbar_thickness_mm=busbar_thickness_mm,
        cooling_channel_thickness_mm=cooling_channel_thickness_mm,
        enclosure_wall_thickness_mm=enclosure_wall_thickness_mm,
    )


def _pack(series_count: int, parallel_count: int, x_spacing_mm: float, z_spacing_mm: float) -> PackPreset:
    return PackPreset(
        series_count=series_count,
        parallel_count=parallel_count,
        x_spacing_mm=x_spacing_mm,
        z_spacing_mm=z_spacing_mm,
    )


PRIMARY_SYSTEM_PRESETS: tuple[BatterySystemPreset, ...] = (
    BatterySystemPreset(
        preset_id="generic_cylindrical_pack",
        display_name="Generic Cylindrical Pack",
        description="Baseline cylindrical trade-study archetype for fast pack layout, thermal zoning, and flagship test sweeps.",
        category="Trade Study Archetypes",
        chemistry_name="generic_liion",
        chemistry_display_name="NMC Cylindrical",
        cell=_cell("samsung_30q", form_factor="cylindrical", radius_mm=10.5, height_mm=70.0, width_mm=21.0, depth_mm=21.0),
        module=_module(6, module_gap_x_mm=74.0, busbar_thickness_mm=10.0, cooling_channel_thickness_mm=18.0),
        pack=_pack(96, 4, 28.0, 28.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=2.4,
        pack_mass_kg=420.0,
        pack_heat_capacity_j_per_kgk=910.0,
        discharge_current_a=220.0,
        thermal_zone_count=6,
        thermal_zone_cooling_multipliers=(1.2, 1.15, 1.1, 1.1, 1.15, 1.2),
        interconnect_resistance_ohm_per_group=0.00035,
        pack_interconnect_resistance_ohm=0.006,
        balancing_enabled=True,
        balancing_bleed_current_a=0.25,
        balancing_soc_threshold=0.92,
        recommended_virtual_tests=("rate_capability", "thermal_stress", "thermal_zone_comparison", "model_validation"),
        operating_limits={"max_discharge_current_a": 280.0, "max_charge_current_a": 160.0},
    ),
    BatterySystemPreset(
        preset_id="generic_prismatic_pack",
        display_name="Generic Prismatic Pack",
        description="Baseline prismatic trade-study archetype with strong zone differentiation for cooling architecture comparisons.",
        category="Trade Study Archetypes",
        chemistry_name="lfp",
        chemistry_display_name="LFP Prismatic",
        cell=_cell("generic_lfp_prismatic_ev", form_factor="prismatic", height_mm=180.0, width_mm=52.0, depth_mm=18.0),
        module=_module(8, module_gap_x_mm=72.0, busbar_thickness_mm=14.0, cooling_channel_thickness_mm=20.0),
        pack=_pack(104, 1, 60.0, 22.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=2.8,
        pack_mass_kg=490.0,
        pack_heat_capacity_j_per_kgk=960.0,
        discharge_current_a=180.0,
        thermal_zone_count=8,
        thermal_zone_cooling_multipliers=(1.25, 1.15, 1.05, 1.0, 1.0, 1.05, 1.15, 1.25),
        interconnect_resistance_ohm_per_group=0.00022,
        pack_interconnect_resistance_ohm=0.004,
        balancing_enabled=True,
        balancing_bleed_current_a=0.3,
        balancing_soc_threshold=0.9,
        recommended_virtual_tests=("rate_capability", "thermal_stress", "thermal_zone_comparison", "model_validation"),
        operating_limits={"max_discharge_current_a": 240.0, "max_charge_current_a": 140.0},
    ),
    BatterySystemPreset(
        preset_id="high_power_pack",
        display_name="High-Power Pack",
        description="High-power trade-study archetype tuned for aggressive load sweeps, thermal stress, and baseline-to-candidate comparison.",
        category="Trade Study Archetypes",
        chemistry_name="generic_liion",
        chemistry_display_name="NCA High Performance",
        cell=_cell("panasonic_ncr18650b", form_factor="cylindrical", radius_mm=9.0, height_mm=65.0, width_mm=18.0, depth_mm=18.0),
        module=_module(10, module_gap_x_mm=60.0, busbar_thickness_mm=9.0, cooling_channel_thickness_mm=16.0),
        pack=_pack(108, 3, 24.0, 24.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=3.0,
        pack_mass_kg=395.0,
        pack_heat_capacity_j_per_kgk=900.0,
        discharge_current_a=260.0,
        thermal_zone_count=10,
        thermal_zone_cooling_multipliers=(1.3, 1.2, 1.15, 1.1, 1.05, 1.05, 1.1, 1.15, 1.2, 1.3),
        interconnect_resistance_ohm_per_group=0.0004,
        pack_interconnect_resistance_ohm=0.007,
        balancing_enabled=True,
        balancing_bleed_current_a=0.22,
        balancing_soc_threshold=0.93,
        recommended_virtual_tests=("rate_capability", "thermal_stress", "thermal_zone_comparison", "model_validation"),
        operating_limits={"max_discharge_current_a": 340.0, "max_charge_current_a": 180.0},
    ),
)

EXPERIMENTAL_SYSTEM_PRESETS: tuple[BatterySystemPreset, ...] = (
    BatterySystemPreset(
        preset_id="motorsport_hybrid_pulse_pack",
        display_name="High-Power Hybrid Pulse Pack",
        description="A motorsport-oriented pulse pack for transient power delivery and aggressive thermal events.",
        category="Motorsport",
        chemistry_name="generic_liion",
        chemistry_display_name="High-Power Cylindrical",
        cell=_cell("a123_anr26650m1b", form_factor="cylindrical", radius_mm=13.0, height_mm=65.0, width_mm=26.0, depth_mm=26.0),
        module=_module(4, module_gap_x_mm=58.0, busbar_thickness_mm=16.0, cooling_channel_thickness_mm=22.0),
        pack=_pack(48, 6, 34.0, 34.0),
        ambient_temp_c=22.0,
        cooling_coeff_w_per_k=3.4,
        pack_mass_kg=82.0,
        pack_heat_capacity_j_per_kgk=930.0,
        discharge_current_a=320.0,
        thermal_zone_count=4,
        thermal_zone_cooling_multipliers=(1.3, 1.1, 1.1, 1.3),
        interconnect_resistance_ohm_per_group=0.00018,
        pack_interconnect_resistance_ohm=0.003,
        balancing_enabled=False,
        recommended_virtual_tests=("pulse_power", "thermal_stress", "fault_response"),
        operating_limits={"max_discharge_current_a": 420.0, "max_charge_current_a": 180.0},
    ),
    BatterySystemPreset(
        preset_id="aviation_evtol_pouch_power_pack",
        display_name="eVTOL Pouch Power Pack",
        description="A pouch-cell power pack tuned for short high-power vertical-lift segments.",
        category="Aviation / Drone",
        chemistry_name="generic_liion",
        chemistry_display_name="High-Power Pouch",
        cell=_cell("evtol_pouch_power", form_factor="pouch", height_mm=210.0, width_mm=82.0, depth_mm=12.0),
        module=_module(6, module_gap_x_mm=70.0, busbar_thickness_mm=8.0, cooling_channel_thickness_mm=14.0),
        pack=_pack(72, 2, 90.0, 18.0),
        ambient_temp_c=20.0,
        cooling_coeff_w_per_k=2.2,
        pack_mass_kg=145.0,
        pack_heat_capacity_j_per_kgk=920.0,
        discharge_current_a=260.0,
        thermal_zone_count=6,
        thermal_zone_cooling_multipliers=(1.25, 1.15, 1.05, 1.05, 1.15, 1.25),
        interconnect_resistance_ohm_per_group=0.00028,
        pack_interconnect_resistance_ohm=0.004,
        balancing_enabled=True,
        balancing_bleed_current_a=0.18,
        balancing_soc_threshold=0.9,
        recommended_virtual_tests=("pulse_power", "thermal_stress", "thermal_zone_comparison"),
        operating_limits={"max_discharge_current_a": 300.0, "max_charge_current_a": 120.0},
    ),
    BatterySystemPreset(
        preset_id="aviation_drone_long_endurance",
        display_name="Long-Endurance Drone High-Energy Pack",
        description="A high-energy drone pack biased toward endurance and storage behavior.",
        category="Aviation / Drone",
        chemistry_name="generic_liion",
        chemistry_display_name="High-Energy Cylindrical",
        cell=_cell("panasonic_ncr18650b", form_factor="cylindrical", radius_mm=9.0, height_mm=65.0, width_mm=18.0, depth_mm=18.0),
        module=_module(2, module_gap_x_mm=54.0, busbar_thickness_mm=9.0, cooling_channel_thickness_mm=12.0),
        pack=_pack(14, 8, 22.0, 22.0),
        ambient_temp_c=18.0,
        cooling_coeff_w_per_k=0.9,
        pack_mass_kg=6.5,
        pack_heat_capacity_j_per_kgk=900.0,
        discharge_current_a=38.0,
        thermal_zone_count=2,
        thermal_zone_cooling_multipliers=(1.05, 0.95),
        interconnect_resistance_ohm_per_group=0.0005,
        pack_interconnect_resistance_ohm=0.003,
        balancing_enabled=True,
        balancing_bleed_current_a=0.08,
        balancing_soc_threshold=0.94,
        recommended_virtual_tests=("constant_current_discharge", "storage_self_discharge", "rate_capability"),
        operating_limits={"max_discharge_current_a": 60.0, "max_charge_current_a": 20.0},
    ),
    BatterySystemPreset(
        preset_id="stationary_residential_lfp_wall",
        display_name="Residential LFP Wall Battery",
        description="A large-format LFP stationary-storage preset with moderate discharge and gentle balancing.",
        category="Stationary Storage",
        chemistry_name="lfp",
        chemistry_display_name="LFP Wall Storage",
        cell=_cell("residential_lfp_wall", form_factor="prismatic", height_mm=190.0, width_mm=72.0, depth_mm=28.0),
        module=_module(5, module_gap_x_mm=86.0, busbar_thickness_mm=14.0, cooling_channel_thickness_mm=18.0),
        pack=_pack(16, 1, 86.0, 30.0),
        ambient_temp_c=24.0,
        cooling_coeff_w_per_k=1.4,
        pack_mass_kg=115.0,
        pack_heat_capacity_j_per_kgk=980.0,
        discharge_current_a=55.0,
        thermal_zone_count=5,
        thermal_zone_cooling_multipliers=(1.1, 1.05, 1.0, 1.05, 1.1),
        interconnect_resistance_ohm_per_group=0.0002,
        pack_interconnect_resistance_ohm=0.002,
        balancing_enabled=True,
        balancing_bleed_current_a=0.12,
        balancing_soc_threshold=0.9,
        recommended_virtual_tests=("constant_current_discharge", "storage_self_discharge", "balancing_effectiveness"),
        operating_limits={"max_discharge_current_a": 90.0, "max_charge_current_a": 60.0},
    ),
    BatterySystemPreset(
        preset_id="stationary_sodium_ion_rack",
        display_name="Sodium-Ion Rack Battery",
        description="A concept sodium-ion storage rack using a generic reduced-order chemistry mapping.",
        category="Stationary Storage",
        chemistry_name="generic_liion",
        chemistry_display_name="Sodium-Ion Concept",
        cell=_cell("sodium_ion_rack", form_factor="prismatic", height_mm=210.0, width_mm=78.0, depth_mm=30.0),
        module=_module(6, module_gap_x_mm=90.0, busbar_thickness_mm=12.0, cooling_channel_thickness_mm=16.0),
        pack=_pack(20, 1, 92.0, 32.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=1.2,
        pack_mass_kg=145.0,
        pack_heat_capacity_j_per_kgk=990.0,
        discharge_current_a=48.0,
        thermal_zone_count=6,
        thermal_zone_cooling_multipliers=(1.08, 1.04, 1.0, 1.0, 1.04, 1.08),
        interconnect_resistance_ohm_per_group=0.00024,
        pack_interconnect_resistance_ohm=0.0025,
        balancing_enabled=True,
        balancing_bleed_current_a=0.1,
        balancing_soc_threshold=0.88,
        recommended_virtual_tests=("storage_self_discharge", "thermal_zone_comparison", "constant_current_discharge"),
        operating_limits={"max_discharge_current_a": 75.0, "max_charge_current_a": 45.0},
    ),
    BatterySystemPreset(
        preset_id="consumer_power_tool_high_discharge",
        display_name="Power Tool High-Discharge Pack",
        description="A compact high-discharge pack for burst-heavy handheld tools.",
        category="Consumer / Light Mobility",
        chemistry_name="generic_liion",
        chemistry_display_name="High-Discharge Cylindrical",
        cell=_cell("samsung_30q", form_factor="cylindrical", radius_mm=9.0, height_mm=65.0, width_mm=18.0, depth_mm=18.0),
        module=_module(1, module_gap_x_mm=40.0, busbar_thickness_mm=10.0, cooling_channel_thickness_mm=10.0),
        pack=_pack(10, 2, 24.0, 24.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=0.8,
        pack_mass_kg=1.9,
        pack_heat_capacity_j_per_kgk=900.0,
        discharge_current_a=45.0,
        thermal_zone_count=1,
        thermal_zone_cooling_multipliers=(1.0,),
        interconnect_resistance_ohm_per_group=0.0007,
        pack_interconnect_resistance_ohm=0.0015,
        balancing_enabled=False,
        recommended_virtual_tests=("pulse_power", "thermal_stress", "fault_response"),
        operating_limits={"max_discharge_current_a": 70.0, "max_charge_current_a": 20.0},
    ),
    BatterySystemPreset(
        preset_id="consumer_ebike_pack",
        display_name="E-Bike Battery Pack",
        description="A balanced light-mobility pack tuned for moderate current and long cycle life.",
        category="Consumer / Light Mobility",
        chemistry_name="generic_liion",
        chemistry_display_name="NMC E-Bike Pack",
        cell=_cell("panasonic_ncr18650b", form_factor="cylindrical", radius_mm=9.0, height_mm=65.0, width_mm=18.0, depth_mm=18.0),
        module=_module(2, module_gap_x_mm=42.0, busbar_thickness_mm=8.0, cooling_channel_thickness_mm=12.0),
        pack=_pack(13, 4, 23.0, 23.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=0.7,
        pack_mass_kg=3.8,
        pack_heat_capacity_j_per_kgk=900.0,
        discharge_current_a=22.0,
        thermal_zone_count=2,
        thermal_zone_cooling_multipliers=(1.05, 0.95),
        interconnect_resistance_ohm_per_group=0.00055,
        pack_interconnect_resistance_ohm=0.0018,
        balancing_enabled=True,
        balancing_bleed_current_a=0.06,
        balancing_soc_threshold=0.93,
        recommended_virtual_tests=("constant_current_discharge", "rate_capability", "balancing_effectiveness"),
        operating_limits={"max_discharge_current_a": 35.0, "max_charge_current_a": 10.0},
    ),
    BatterySystemPreset(
        preset_id="frontier_solid_state_ev_concept",
        display_name="Solid-State EV Concept Pack",
        description="A frontier concept preset using a pouch-like geometry and optimistic thermal assumptions.",
        category="Frontier / Concept",
        chemistry_name="generic_liion",
        chemistry_display_name="Solid-State Concept",
        cell=_cell("solid_state_concept", form_factor="pouch", height_mm=240.0, width_mm=96.0, depth_mm=10.0),
        module=_module(8, module_gap_x_mm=84.0, busbar_thickness_mm=7.0, cooling_channel_thickness_mm=14.0),
        pack=_pack(120, 2, 102.0, 16.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=2.1,
        pack_mass_kg=330.0,
        pack_heat_capacity_j_per_kgk=940.0,
        discharge_current_a=210.0,
        thermal_zone_count=8,
        thermal_zone_cooling_multipliers=(1.16, 1.12, 1.08, 1.04, 1.04, 1.08, 1.12, 1.16),
        interconnect_resistance_ohm_per_group=0.00018,
        pack_interconnect_resistance_ohm=0.003,
        balancing_enabled=True,
        balancing_bleed_current_a=0.18,
        balancing_soc_threshold=0.91,
        recommended_virtual_tests=("rate_capability", "thermal_zone_comparison", "storage_self_discharge"),
        operating_limits={"max_discharge_current_a": 260.0, "max_charge_current_a": 150.0},
    ),
    BatterySystemPreset(
        preset_id="frontier_lithium_sulfur_aerospace",
        display_name="Lithium-Sulfur Aerospace Concept Pack",
        description="A high-energy concept pack for aerospace studies with light cooling and storage sensitivity.",
        category="Frontier / Concept",
        chemistry_name="generic_liion",
        chemistry_display_name="Lithium-Sulfur Concept",
        cell=_cell("lithium_sulfur_concept", form_factor="pouch", height_mm=250.0, width_mm=88.0, depth_mm=11.0),
        module=_module(4, module_gap_x_mm=78.0, busbar_thickness_mm=6.0, cooling_channel_thickness_mm=10.0),
        pack=_pack(48, 3, 96.0, 18.0),
        ambient_temp_c=18.0,
        cooling_coeff_w_per_k=1.0,
        pack_mass_kg=62.0,
        pack_heat_capacity_j_per_kgk=940.0,
        discharge_current_a=95.0,
        thermal_zone_count=4,
        thermal_zone_cooling_multipliers=(1.02, 0.98, 0.98, 1.02),
        interconnect_resistance_ohm_per_group=0.0003,
        pack_interconnect_resistance_ohm=0.0025,
        balancing_enabled=True,
        balancing_bleed_current_a=0.12,
        balancing_soc_threshold=0.9,
        recommended_virtual_tests=("constant_current_discharge", "storage_self_discharge", "thermal_stress"),
        operating_limits={"max_discharge_current_a": 130.0, "max_charge_current_a": 60.0},
    ),
)

SYSTEM_PRESETS: tuple[BatterySystemPreset, ...] = PRIMARY_SYSTEM_PRESETS
ALL_SYSTEM_PRESETS: tuple[BatterySystemPreset, ...] = PRIMARY_SYSTEM_PRESETS + EXPERIMENTAL_SYSTEM_PRESETS

LEGACY_PRESET_ID_ALIASES: dict[str, str] = {
    "road_ev_generic_nmc_cylindrical": "generic_cylindrical_pack",
    "road_ev_generic_lfp_prismatic": "generic_prismatic_pack",
    "road_ev_high_performance_nca": "high_power_pack",
}


def get_system_preset(preset_id: str) -> BatterySystemPreset:
    resolved_preset_id = LEGACY_PRESET_ID_ALIASES.get(preset_id, preset_id)
    for preset in ALL_SYSTEM_PRESETS:
        if preset.preset_id == resolved_preset_id:
            return preset
    raise ValueError(f"Unknown system preset: {preset_id}")


def validate_system_preset(preset: BatterySystemPreset) -> None:
    if preset.cell.key not in REFERENCE_CELLS:
        raise ValueError(f"Preset '{preset.preset_id}' references unknown cell preset '{preset.cell.key}'.")
    cell = REFERENCE_CELLS[preset.cell.key]
    if preset.pack.series_count < 1 or preset.pack.parallel_count < 1:
        raise ValueError(f"Preset '{preset.preset_id}' must have positive series and parallel counts.")
    if preset.module.module_count < 1 or preset.module.module_count > preset.pack.series_count:
        raise ValueError(f"Preset '{preset.preset_id}' has an invalid module count.")
    if preset.thermal_zone_count < 1:
        raise ValueError(f"Preset '{preset.preset_id}' must define at least one thermal zone.")
    if len(preset.thermal_zone_cooling_multipliers) != preset.thermal_zone_count:
        raise ValueError(f"Preset '{preset.preset_id}' thermal zone multiplier count does not match thermal_zone_count.")
    if any(multiplier <= 0.0 for multiplier in preset.thermal_zone_cooling_multipliers):
        raise ValueError(f"Preset '{preset.preset_id}' thermal zone multipliers must be positive.")
    if preset.chemistry_name == "lfp" and "lfp" not in cell.chemistry.lower():
        raise ValueError(f"Preset '{preset.preset_id}' pairs LFP chemistry with a non-LFP cell preset.")
    valid_tests = {definition.test_id for definition in build_test_catalog(include_experimental=True)}
    invalid_tests = [test_id for test_id in preset.recommended_virtual_tests if test_id not in valid_tests]
    if invalid_tests:
        raise ValueError(f"Preset '{preset.preset_id}' references unknown virtual tests: {', '.join(invalid_tests)}")


def _band_assignments(group_count: int, zone_count: int) -> list[int]:
    groups_per_zone = max(1, ceil(group_count / zone_count))
    assignments: list[int] = []
    for group_index in range(group_count):
        zone_index = min(group_index // groups_per_zone, zone_count - 1)
        assignments.append(zone_index)
    return assignments


def _resolve_preset(preset_or_id: BatterySystemPreset | str) -> BatterySystemPreset:
    if isinstance(preset_or_id, BatterySystemPreset):
        validate_system_preset(preset_or_id)
        return preset_or_id
    return get_system_preset(str(preset_or_id))


def build_simulation_config_for_preset(preset_or_id: BatterySystemPreset | str) -> SimulationConfig:
    preset = _resolve_preset(preset_or_id)
    cell = REFERENCE_CELLS[preset.cell.key]
    group_count = preset.pack.series_count
    assignments = tuple(_band_assignments(group_count, preset.thermal_zone_count))
    thermal_zones = tuple(
        ThermalZoneConfig(
            zone_id=zone_index,
            name=f"{preset.display_name} Zone {zone_index + 1}",
            cooling_coeff_multiplier=multiplier,
            note="Generated from battery system preset cooling bands.",
        )
        for zone_index, multiplier in enumerate(preset.thermal_zone_cooling_multipliers)
    )
    group_labels = tuple(f"Group {index + 1}" for index in range(group_count))
    group_entity_ids = tuple(f"group-{index}" for index in range(group_count))

    return validate_simulation_config(
        SimulationConfig(
            cell_nominal_voltage=cell.cell_nominal_voltage,
            cell_full_voltage=cell.cell_full_voltage,
            cell_empty_voltage=cell.cell_empty_voltage,
            cell_cutoff_voltage=cell.cell_cutoff_voltage,
            cell_capacity_ah=cell.cell_capacity_ah,
            cells_in_series=preset.pack.series_count,
            cells_in_parallel=preset.pack.parallel_count,
            internal_resistance_ohm_per_cell=cell.internal_resistance_ohm_per_cell,
            ambient_temp_c=preset.ambient_temp_c,
            discharge_current_a=preset.discharge_current_a,
            duration_s=preset.duration_s,
            time_step_s=preset.time_step_s,
            initial_soc=preset.initial_soc,
            pack_mass_kg=preset.pack_mass_kg,
            pack_heat_capacity_j_per_kgk=preset.pack_heat_capacity_j_per_kgk,
            cooling_coeff_w_per_k=preset.cooling_coeff_w_per_k,
            electrical_model=ElectricalModelConfig(
                model_type="rint",
                r0_ohm_per_cell=cell.internal_resistance_ohm_per_cell,
            ),
            group_count=group_count,
            balancing=BalancingConfig(
                enabled=preset.balancing_enabled,
                mode="passive",
                soc_threshold=preset.balancing_soc_threshold,
                bleed_current_a=preset.balancing_bleed_current_a,
            ),
            thermal_zones=thermal_zones,
            group_zone_assignments=assignments,
            group_labels=group_labels,
            group_entity_ids=group_entity_ids,
            chemistry_name=preset.chemistry_name,
            physics=PhysicsConfig(
                interconnect_resistance_ohm_per_group=preset.interconnect_resistance_ohm_per_group,
                pack_interconnect_resistance_ohm=preset.pack_interconnect_resistance_ohm,
            ),
        )
    )


def build_calibrated_simulation_config_for_preset(
    preset_or_id: BatterySystemPreset | str,
    dataset_path: str,
    *,
    rc_branch_count: int = 1,
) -> SimulationConfig:
    base_config = build_simulation_config_for_preset(preset_or_id)
    dataset = load_truth_dataset(dataset_path)
    calibrated = calibrate_parameters(dataset, rc_branch_count=rc_branch_count)
    return validate_simulation_config(
        apply_calibration_to_simulation_config(base_config, calibrated)
    )


def build_system_preset_payload(preset: BatterySystemPreset) -> dict[str, object]:
    validate_system_preset(preset)
    cell = REFERENCE_CELLS[preset.cell.key]
    group_count = preset.pack.series_count
    assignments = _band_assignments(group_count, preset.thermal_zone_count)
    thermal_zones = [
        {
            "zone_id": zone_index,
            "name": f"{preset.display_name} Zone {zone_index + 1}",
            "cooling_coeff_multiplier": multiplier,
            "note": "Generated from battery system preset cooling bands.",
        }
        for zone_index, multiplier in enumerate(preset.thermal_zone_cooling_multipliers)
    ]
    return {
        "preset_id": preset.preset_id,
        "display_name": preset.display_name,
        "description": preset.description,
        "category": preset.category,
        "chemistry_name": preset.chemistry_name,
        "chemistry_display_name": preset.chemistry_display_name,
        "cell_preset_key": preset.cell.key,
        "cell_preset_name": cell.name,
        "recommended_virtual_tests": list(preset.recommended_virtual_tests),
        "operating_limits": dict(preset.operating_limits),
        "simulation_defaults": {
            "chemistry_name": preset.chemistry_name,
            "cell_nominal_voltage": cell.cell_nominal_voltage,
            "cell_full_voltage": cell.cell_full_voltage,
            "cell_empty_voltage": cell.cell_empty_voltage,
            "cell_cutoff_voltage": cell.cell_cutoff_voltage,
            "cell_capacity_ah": cell.cell_capacity_ah,
            "cells_in_series": preset.pack.series_count,
            "cells_in_parallel": preset.pack.parallel_count,
            "internal_resistance_ohm_per_cell": cell.internal_resistance_ohm_per_cell,
            "ambient_temp_c": preset.ambient_temp_c,
            "discharge_current_a": preset.discharge_current_a,
            "duration_s": preset.duration_s,
            "time_step_s": preset.time_step_s,
            "initial_soc": preset.initial_soc,
            "pack_mass_kg": preset.pack_mass_kg,
            "pack_heat_capacity_j_per_kgk": preset.pack_heat_capacity_j_per_kgk,
            "cooling_coeff_w_per_k": preset.cooling_coeff_w_per_k,
            "group_count": group_count,
            "balancing": {
                "enabled": preset.balancing_enabled,
                "mode": "passive",
                "bleed_current_a": preset.balancing_bleed_current_a,
                "soc_threshold": preset.balancing_soc_threshold,
            },
            "thermal_zones": thermal_zones,
            "group_zone_assignments": assignments,
            "physics": {
                "interconnect_resistance_ohm_per_group": preset.interconnect_resistance_ohm_per_group,
                "pack_interconnect_resistance_ohm": preset.pack_interconnect_resistance_ohm,
            },
        },
        "cad_defaults": {
            "preset_name": preset.display_name,
            "cells_in_series": preset.pack.series_count,
            "cells_in_parallel": preset.pack.parallel_count,
            "module_count": preset.module.module_count,
            "cell_form_factor": preset.cell.form_factor,
            "cell_radius_mm": preset.cell.radius_mm,
            "cell_height_mm": preset.cell.height_mm,
            "cell_width_mm": preset.cell.width_mm,
            "cell_depth_mm": preset.cell.depth_mm,
            "x_spacing_mm": preset.pack.x_spacing_mm,
            "z_spacing_mm": preset.pack.z_spacing_mm,
            "module_gap_x_mm": preset.module.module_gap_x_mm,
            "busbar_thickness_mm": preset.module.busbar_thickness_mm,
            "cooling_channel_thickness_mm": preset.module.cooling_channel_thickness_mm,
            "enclosure_wall_thickness_mm": preset.module.enclosure_wall_thickness_mm,
        },
    }


def battery_system_preset_catalog_to_dict() -> dict[str, object]:
    presets = [build_system_preset_payload(preset) for preset in PRIMARY_SYSTEM_PRESETS]
    experimental_presets = [build_system_preset_payload(preset) for preset in EXPERIMENTAL_SYSTEM_PRESETS]
    return {
        "default_preset_id": PRIMARY_SYSTEM_PRESETS[0].preset_id,
        "presets": presets,
        "experimental_presets": experimental_presets,
        "categories": sorted({preset.category for preset in PRIMARY_SYSTEM_PRESETS}),
    }
