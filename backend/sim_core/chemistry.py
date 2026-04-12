from __future__ import annotations

"""Chemistry presets for reduced-order ECM simulation.

Chemistry presets provide clean defaults for OCV shape, nonlinear electrical
behavior, and thermal tuning. They are intended for early engineering studies,
not full electrochemical fidelity.
"""

from dataclasses import replace

from .types import OcvLookupPoint, PhysicsConfig, SimulationConfig


class ChemistryPreset:
    def __init__(
        self,
        *,
        name: str,
        display_name: str,
        cell_nominal_voltage: float,
        cell_full_voltage: float,
        cell_empty_voltage: float,
        cell_cutoff_voltage: float,
        ocv_curve: tuple[OcvLookupPoint, ...] = (),
        default_physics: PhysicsConfig = PhysicsConfig(),
    ) -> None:
        self.name = name
        self.display_name = display_name
        self.cell_nominal_voltage = cell_nominal_voltage
        self.cell_full_voltage = cell_full_voltage
        self.cell_empty_voltage = cell_empty_voltage
        self.cell_cutoff_voltage = cell_cutoff_voltage
        self.ocv_curve = ocv_curve
        self.default_physics = default_physics


from .chemistries import GENERIC_LIION_PRESET, LFP_PRESET


CHEMISTRY_PRESETS: dict[str, ChemistryPreset] = {
    GENERIC_LIION_PRESET.name: GENERIC_LIION_PRESET,
    LFP_PRESET.name: LFP_PRESET,
}


def get_chemistry_preset(name: str | None) -> ChemistryPreset:
    preset_name = (name or "generic_liion").lower()
    try:
        return CHEMISTRY_PRESETS[preset_name]
    except KeyError as exc:
        raise ValueError(f"Unsupported chemistry: {preset_name}") from exc


def available_chemistries() -> tuple[str, ...]:
    return tuple(CHEMISTRY_PRESETS.keys())


def apply_chemistry_defaults(config: SimulationConfig) -> SimulationConfig:
    preset = get_chemistry_preset(config.chemistry_name)
    generic_defaults = GENERIC_LIION_PRESET.default_physics
    physics = config.physics

    merged_physics = PhysicsConfig(
        discharge_efficiency=physics.discharge_efficiency if physics.discharge_efficiency != generic_defaults.discharge_efficiency else preset.default_physics.discharge_efficiency,
        charge_efficiency=physics.charge_efficiency if physics.charge_efficiency != generic_defaults.charge_efficiency else preset.default_physics.charge_efficiency,
        resistance_temperature_alpha_per_c=physics.resistance_temperature_alpha_per_c if physics.resistance_temperature_alpha_per_c != generic_defaults.resistance_temperature_alpha_per_c else preset.default_physics.resistance_temperature_alpha_per_c,
        resistance_reference_temp_c=physics.resistance_reference_temp_c if physics.resistance_reference_temp_c != generic_defaults.resistance_reference_temp_c else preset.default_physics.resistance_reference_temp_c,
        capacity_temperature_reference_c=physics.capacity_temperature_reference_c if physics.capacity_temperature_reference_c != generic_defaults.capacity_temperature_reference_c else preset.default_physics.capacity_temperature_reference_c,
        capacity_cold_derate_per_c=physics.capacity_cold_derate_per_c if physics.capacity_cold_derate_per_c != generic_defaults.capacity_cold_derate_per_c else preset.default_physics.capacity_cold_derate_per_c,
        min_capacity_scale=physics.min_capacity_scale if physics.min_capacity_scale != generic_defaults.min_capacity_scale else preset.default_physics.min_capacity_scale,
        self_discharge_per_day=physics.self_discharge_per_day if physics.self_discharge_per_day != generic_defaults.self_discharge_per_day else preset.default_physics.self_discharge_per_day,
        interconnect_resistance_ohm_per_group=physics.interconnect_resistance_ohm_per_group if physics.interconnect_resistance_ohm_per_group != generic_defaults.interconnect_resistance_ohm_per_group else preset.default_physics.interconnect_resistance_ohm_per_group,
        pack_interconnect_resistance_ohm=physics.pack_interconnect_resistance_ohm if physics.pack_interconnect_resistance_ohm != generic_defaults.pack_interconnect_resistance_ohm else preset.default_physics.pack_interconnect_resistance_ohm,
        neighbor_thermal_coupling_w_per_k=physics.neighbor_thermal_coupling_w_per_k if physics.neighbor_thermal_coupling_w_per_k != generic_defaults.neighbor_thermal_coupling_w_per_k else preset.default_physics.neighbor_thermal_coupling_w_per_k,
        ocv_curve=physics.ocv_curve,
        resistance_vs_soc_enabled=physics.resistance_vs_soc_enabled if physics.resistance_vs_soc_enabled != generic_defaults.resistance_vs_soc_enabled else preset.default_physics.resistance_vs_soc_enabled,
        resistance_soc_curve=physics.resistance_soc_curve if physics.resistance_soc_curve != generic_defaults.resistance_soc_curve else preset.default_physics.resistance_soc_curve,
        hysteresis_enabled=physics.hysteresis_enabled if physics.hysteresis_enabled != generic_defaults.hysteresis_enabled else preset.default_physics.hysteresis_enabled,
        hysteresis_max_voltage_v=physics.hysteresis_max_voltage_v if physics.hysteresis_max_voltage_v != generic_defaults.hysteresis_max_voltage_v else preset.default_physics.hysteresis_max_voltage_v,
        hysteresis_response_rate_per_s=physics.hysteresis_response_rate_per_s if physics.hysteresis_response_rate_per_s != generic_defaults.hysteresis_response_rate_per_s else preset.default_physics.hysteresis_response_rate_per_s,
        hysteresis_relaxation_tau_s=physics.hysteresis_relaxation_tau_s if physics.hysteresis_relaxation_tau_s != generic_defaults.hysteresis_relaxation_tau_s else preset.default_physics.hysteresis_relaxation_tau_s,
        hysteresis_current_scale_a=physics.hysteresis_current_scale_a if physics.hysteresis_current_scale_a != generic_defaults.hysteresis_current_scale_a else preset.default_physics.hysteresis_current_scale_a,
        rc_state_dependence_enabled=physics.rc_state_dependence_enabled if physics.rc_state_dependence_enabled != generic_defaults.rc_state_dependence_enabled else preset.default_physics.rc_state_dependence_enabled,
        rc_low_soc_multiplier=physics.rc_low_soc_multiplier if physics.rc_low_soc_multiplier != generic_defaults.rc_low_soc_multiplier else preset.default_physics.rc_low_soc_multiplier,
        rc_high_temp_multiplier_per_c=physics.rc_high_temp_multiplier_per_c if physics.rc_high_temp_multiplier_per_c != generic_defaults.rc_high_temp_multiplier_per_c else preset.default_physics.rc_high_temp_multiplier_per_c,
        diffusion_stress_enabled=physics.diffusion_stress_enabled if physics.diffusion_stress_enabled != generic_defaults.diffusion_stress_enabled else preset.default_physics.diffusion_stress_enabled,
        diffusion_stress_max_v=physics.diffusion_stress_max_v if physics.diffusion_stress_max_v != generic_defaults.diffusion_stress_max_v else preset.default_physics.diffusion_stress_max_v,
        diffusion_stress_build_rate_per_s=physics.diffusion_stress_build_rate_per_s if physics.diffusion_stress_build_rate_per_s != generic_defaults.diffusion_stress_build_rate_per_s else preset.default_physics.diffusion_stress_build_rate_per_s,
        diffusion_stress_decay_tau_s=physics.diffusion_stress_decay_tau_s if physics.diffusion_stress_decay_tau_s != generic_defaults.diffusion_stress_decay_tau_s else preset.default_physics.diffusion_stress_decay_tau_s,
        diffusion_stress_current_scale_a=physics.diffusion_stress_current_scale_a if physics.diffusion_stress_current_scale_a != generic_defaults.diffusion_stress_current_scale_a else preset.default_physics.diffusion_stress_current_scale_a,
        diffusion_stress_resistance_coeff=physics.diffusion_stress_resistance_coeff if physics.diffusion_stress_resistance_coeff != generic_defaults.diffusion_stress_resistance_coeff else preset.default_physics.diffusion_stress_resistance_coeff,
        two_node_thermal_enabled=physics.two_node_thermal_enabled if physics.two_node_thermal_enabled != generic_defaults.two_node_thermal_enabled else preset.default_physics.two_node_thermal_enabled,
        core_surface_thermal_coupling_w_per_k=physics.core_surface_thermal_coupling_w_per_k if physics.core_surface_thermal_coupling_w_per_k != generic_defaults.core_surface_thermal_coupling_w_per_k else preset.default_physics.core_surface_thermal_coupling_w_per_k,
        surface_thermal_mass_fraction=physics.surface_thermal_mass_fraction if physics.surface_thermal_mass_fraction != generic_defaults.surface_thermal_mass_fraction else preset.default_physics.surface_thermal_mass_fraction,
        core_thermal_mass_j_per_k=physics.core_thermal_mass_j_per_k if physics.core_thermal_mass_j_per_k is not None else preset.default_physics.core_thermal_mass_j_per_k,
        surface_thermal_mass_j_per_k=physics.surface_thermal_mass_j_per_k if physics.surface_thermal_mass_j_per_k is not None else preset.default_physics.surface_thermal_mass_j_per_k,
        nonlinear_cooling_enabled=physics.nonlinear_cooling_enabled if physics.nonlinear_cooling_enabled != generic_defaults.nonlinear_cooling_enabled else preset.default_physics.nonlinear_cooling_enabled,
        nonlinear_cooling_delta_threshold_c=physics.nonlinear_cooling_delta_threshold_c if physics.nonlinear_cooling_delta_threshold_c != generic_defaults.nonlinear_cooling_delta_threshold_c else preset.default_physics.nonlinear_cooling_delta_threshold_c,
        nonlinear_cooling_gain_per_c=physics.nonlinear_cooling_gain_per_c if physics.nonlinear_cooling_gain_per_c != generic_defaults.nonlinear_cooling_gain_per_c else preset.default_physics.nonlinear_cooling_gain_per_c,
        reversible_heat_enabled=physics.reversible_heat_enabled if physics.reversible_heat_enabled != generic_defaults.reversible_heat_enabled else preset.default_physics.reversible_heat_enabled,
        reversible_heat_coeff_v_per_k=physics.reversible_heat_coeff_v_per_k if physics.reversible_heat_coeff_v_per_k != generic_defaults.reversible_heat_coeff_v_per_k else preset.default_physics.reversible_heat_coeff_v_per_k,
        charge_resistance_multiplier=physics.charge_resistance_multiplier if physics.charge_resistance_multiplier != generic_defaults.charge_resistance_multiplier else preset.default_physics.charge_resistance_multiplier,
        discharge_resistance_multiplier=physics.discharge_resistance_multiplier if physics.discharge_resistance_multiplier != generic_defaults.discharge_resistance_multiplier else preset.default_physics.discharge_resistance_multiplier,
    )
    return replace(config, chemistry_name=preset.name, physics=merged_physics)
