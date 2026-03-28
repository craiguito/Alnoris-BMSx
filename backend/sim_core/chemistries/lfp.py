from __future__ import annotations

from ..types import OcvLookupPoint, PhysicsConfig, SocLookupPoint
from ..chemistry import ChemistryPreset


LFP_PRESET = ChemistryPreset(
    name="lfp",
    display_name="LFP",
    cell_nominal_voltage=3.3,
    cell_full_voltage=3.6,
    cell_empty_voltage=2.0,
    cell_cutoff_voltage=2.0,
    ocv_curve=(
        OcvLookupPoint(0.0, 2.0),
        OcvLookupPoint(0.05, 2.8),
        OcvLookupPoint(0.15, 3.15),
        OcvLookupPoint(0.40, 3.26),
        OcvLookupPoint(0.70, 3.31),
        OcvLookupPoint(0.90, 3.38),
        OcvLookupPoint(1.0, 3.6),
    ),
    default_physics=PhysicsConfig(
        resistance_vs_soc_enabled=True,
        resistance_soc_curve=(
            SocLookupPoint(0.0, 1.30),
            SocLookupPoint(0.15, 1.12),
            SocLookupPoint(0.5, 1.0),
            SocLookupPoint(0.85, 1.04),
            SocLookupPoint(1.0, 1.10),
        ),
        hysteresis_enabled=True,
        hysteresis_max_voltage_v=0.045,
        hysteresis_response_rate_per_s=0.22,
        hysteresis_relaxation_tau_s=240.0,
        hysteresis_current_scale_a=6.0,
        diffusion_stress_enabled=True,
        diffusion_stress_max_v=0.035,
        diffusion_stress_build_rate_per_s=0.16,
        diffusion_stress_decay_tau_s=70.0,
        diffusion_stress_current_scale_a=7.0,
        diffusion_stress_resistance_coeff=0.22,
        two_node_thermal_enabled=True,
        core_surface_thermal_coupling_w_per_k=2.2,
        surface_thermal_mass_fraction=0.28,
        nonlinear_cooling_enabled=True,
        nonlinear_cooling_delta_threshold_c=6.0,
        nonlinear_cooling_gain_per_c=0.025,
        discharge_efficiency=0.998,
        charge_efficiency=0.993,
        resistance_temperature_alpha_per_c=0.004,
        charge_resistance_multiplier=1.08,
        discharge_resistance_multiplier=0.98,
    ),
)
