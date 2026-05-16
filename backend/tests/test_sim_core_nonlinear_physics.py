from __future__ import annotations

import unittest

from backend.sim_core.bridge import run_simulation_from_dict
from backend.sim_core.engine import run_simulation
from backend.sim_core.types import CurrentProfile, CurrentProfilePoint, PhysicsConfig
from backend.tests.helpers import make_1rc_model, make_config


class NonlinearPhysicsTests(unittest.TestCase):
    def test_chemistry_defaults_to_generic_liion_when_omitted(self) -> None:
        result = run_simulation(make_config(duration_s=30))
        self.assertEqual(result.summary.chemistry_name, "generic_liion")

    def test_soc_dependent_resistance_changes_voltage_sag(self) -> None:
        physics = PhysicsConfig(resistance_vs_soc_enabled=True)
        low_soc = run_simulation(make_config(initial_soc=0.15, duration_s=30, discharge_current_a=5.0, physics=physics))
        high_soc = run_simulation(make_config(initial_soc=0.75, duration_s=30, discharge_current_a=5.0, physics=physics))
        self.assertLess(low_soc.time_series[0].pack_voltage_v, high_soc.time_series[0].pack_voltage_v)

    def test_hysteresis_causes_charge_discharge_voltage_difference(self) -> None:
        physics = PhysicsConfig(hysteresis_enabled=True, hysteresis_max_voltage_v=0.06)
        discharge = run_simulation(make_config(initial_soc=0.6, duration_s=60, current_profile=CurrentProfile(points=(CurrentProfilePoint(0, 3.0),)), physics=physics))
        charge = run_simulation(make_config(initial_soc=0.6, duration_s=60, current_profile=CurrentProfile(points=(CurrentProfilePoint(0, -3.0),)), physics=physics))
        self.assertNotAlmostEqual(discharge.time_series[-1].group_hysteresis_v[0], charge.time_series[-1].group_hysteresis_v[0], places=4)

    def test_hysteresis_relaxes_during_rest(self) -> None:
        physics = PhysicsConfig(hysteresis_enabled=True, hysteresis_max_voltage_v=0.06, hysteresis_relaxation_tau_s=40.0)
        result = run_simulation(make_config(
            duration_s=120,
            current_profile=CurrentProfile(points=(CurrentProfilePoint(0, 4.0), CurrentProfilePoint(60, 0.0))),
            physics=physics,
        ))
        self.assertGreater(abs(result.time_series[59].group_hysteresis_v[0]), abs(result.time_series[-1].group_hysteresis_v[0]))

    def test_diffusion_stress_builds_and_decays(self) -> None:
        physics = PhysicsConfig(
            diffusion_stress_enabled=True,
            diffusion_stress_max_v=0.08,
            diffusion_stress_build_rate_per_s=0.3,
            diffusion_stress_decay_tau_s=20.0,
        )
        result = run_simulation(make_config(
            duration_s=120,
            current_profile=CurrentProfile(points=(CurrentProfilePoint(0, 8.0), CurrentProfilePoint(60, 0.0))),
            physics=physics,
        ))
        self.assertGreater(result.time_series[50].group_diffusion_stress[0], 0.0)
        self.assertGreater(result.time_series[50].group_diffusion_stress[0], result.time_series[-1].group_diffusion_stress[0])

    def test_state_dependent_rc_changes_recovery(self) -> None:
        physics = PhysicsConfig(rc_state_dependence_enabled=True, rc_low_soc_multiplier=1.5)
        model = make_1rc_model(0.03, 0.02, 2600.0)
        low_soc = run_simulation(make_config(
            initial_soc=0.2,
            duration_s=120,
            current_profile=CurrentProfile(points=(CurrentProfilePoint(0, 5.0), CurrentProfilePoint(20, 0.0))),
            electrical_model=model,
            physics=physics,
        ))
        high_soc = run_simulation(make_config(
            initial_soc=0.8,
            duration_s=120,
            current_profile=CurrentProfile(points=(CurrentProfilePoint(0, 5.0), CurrentProfilePoint(20, 0.0))),
            electrical_model=model,
            physics=physics,
        ))
        low_recovery = low_soc.time_series[-1].pack_voltage_v - low_soc.time_series[20].pack_voltage_v
        high_recovery = high_soc.time_series[-1].pack_voltage_v - high_soc.time_series[20].pack_voltage_v
        self.assertNotAlmostEqual(low_recovery, high_recovery, places=4)

    def test_two_node_thermal_creates_core_surface_lag(self) -> None:
        physics = PhysicsConfig(two_node_thermal_enabled=True, core_surface_thermal_coupling_w_per_k=0.8)
        result = run_simulation(make_config(duration_s=600, discharge_current_a=8.0, physics=physics))
        self.assertGreater(result.summary.max_core_temp_c, result.summary.max_surface_temp_c)
        self.assertGreater(result.summary.temp_gradient_max_c, 0.0)

    def test_stronger_cooling_affects_surface_more_directly_than_core(self) -> None:
        physics = PhysicsConfig(two_node_thermal_enabled=True, core_surface_thermal_coupling_w_per_k=0.8)
        weak_cooling = run_simulation(
            make_config(duration_s=600, discharge_current_a=8.0, cooling_coeff_w_per_k=0.3, physics=physics)
        )
        strong_cooling = run_simulation(
            make_config(duration_s=600, discharge_current_a=8.0, cooling_coeff_w_per_k=2.0, physics=physics)
        )
        surface_drop = weak_cooling.summary.max_surface_temp_c - strong_cooling.summary.max_surface_temp_c
        core_drop = weak_cooling.summary.max_core_temp_c - strong_cooling.summary.max_core_temp_c
        self.assertGreater(surface_drop, 0.0)
        self.assertGreater(surface_drop, core_drop)

    def test_nonlinear_cooling_changes_thermal_trajectory(self) -> None:
        linear = run_simulation(make_config(duration_s=900, discharge_current_a=8.0))
        nonlinear = run_simulation(
            make_config(
                duration_s=900,
                discharge_current_a=8.0,
                physics=PhysicsConfig(
                    nonlinear_cooling_enabled=True,
                    nonlinear_cooling_delta_threshold_c=0.0,
                    nonlinear_cooling_gain_per_c=0.04,
                ),
            )
        )
        self.assertNotAlmostEqual(linear.summary.max_group_temp_c, nonlinear.summary.max_group_temp_c, places=4)

    def test_reversible_heat_changes_heat_behavior(self) -> None:
        baseline = run_simulation(make_config(duration_s=60, discharge_current_a=4.0))
        reversible = run_simulation(make_config(duration_s=60, discharge_current_a=4.0, physics=PhysicsConfig(reversible_heat_enabled=True, reversible_heat_coeff_v_per_k=0.00012)))
        self.assertNotAlmostEqual(baseline.time_series[0].pack_heat_w, reversible.time_series[0].pack_heat_w, places=5)

    def test_negative_reversible_heat_does_not_cool_below_ambient(self) -> None:
        ambient_temp_c = 25.0
        result = run_simulation(
            make_config(
                duration_s=120,
                initial_soc=1.0,
                discharge_current_a=4.0,
                ambient_temp_c=ambient_temp_c,
                physics=PhysicsConfig(
                    two_node_thermal_enabled=True,
                    reversible_heat_enabled=True,
                    reversible_heat_coeff_v_per_k=0.02,
                    core_surface_thermal_coupling_w_per_k=0.8,
                ),
            )
        )
        self.assertGreaterEqual(min(point.pack_temp_avg_c for point in result.time_series), ambient_temp_c - 1e-6)
        self.assertGreaterEqual(min(min(point.group_core_temp_c) for point in result.time_series), ambient_temp_c - 1e-6)
        self.assertGreaterEqual(min(min(point.group_surface_temp_c) for point in result.time_series), ambient_temp_c - 1e-6)

    def test_current_direction_asymmetry_changes_response(self) -> None:
        physics = PhysicsConfig(charge_resistance_multiplier=1.25, discharge_resistance_multiplier=0.95)
        discharge = run_simulation(make_config(duration_s=60, current_profile=CurrentProfile(points=(CurrentProfilePoint(0, 4.0),)), physics=physics))
        charge = run_simulation(make_config(duration_s=60, current_profile=CurrentProfile(points=(CurrentProfilePoint(0, -4.0),)), physics=physics))
        self.assertNotAlmostEqual(discharge.time_series[0].group_effective_resistance_ohm[0], charge.time_series[0].group_effective_resistance_ohm[0], places=5)

    def test_repeated_pulse_history_changes_later_pulse_response(self) -> None:
        physics = PhysicsConfig(
            diffusion_stress_enabled=True,
            diffusion_stress_max_v=0.05,
            diffusion_stress_build_rate_per_s=0.25,
            diffusion_stress_decay_tau_s=120.0,
        )
        result = run_simulation(
            make_config(
                duration_s=180,
                current_profile=CurrentProfile(
                    points=(
                        CurrentProfilePoint(0, 8.0),
                        CurrentProfilePoint(20, 0.0),
                        CurrentProfilePoint(60, 8.0),
                        CurrentProfilePoint(80, 0.0),
                        CurrentProfilePoint(120, 8.0),
                        CurrentProfilePoint(140, 0.0),
                    )
                ),
                physics=physics,
            )
        )
        first_pulse_voltage = result.time_series[10].pack_voltage_v
        later_pulse_voltage = result.time_series[130].pack_voltage_v
        self.assertLess(later_pulse_voltage, first_pulse_voltage)

    def test_chemistry_specific_presets_change_behavior(self) -> None:
        generic = run_simulation(
            make_config(
                cell_key="a123_anr26650m1b",
                chemistry_name="generic_liion",
                duration_s=180,
                discharge_current_a=6.0,
            )
        )
        lfp = run_simulation(
            make_config(
                cell_key="a123_anr26650m1b",
                chemistry_name="lfp",
                duration_s=180,
                discharge_current_a=6.0,
            )
        )
        self.assertNotAlmostEqual(generic.time_series[0].pack_voltage_v, lfp.time_series[0].pack_voltage_v, places=4)
        self.assertNotEqual(generic.summary.nonlinear_features_enabled, lfp.summary.nonlinear_features_enabled)

    def test_outputs_remain_serializable(self) -> None:
        payload = {
            "cell_nominal_voltage": 3.6,
            "cell_full_voltage": 4.2,
            "cell_empty_voltage": 3.0,
            "cell_cutoff_voltage": 3.0,
            "cell_capacity_ah": 3.35,
            "cells_in_series": 4,
            "cells_in_parallel": 1,
            "internal_resistance_ohm_per_cell": 0.035,
            "ambient_temp_c": 25.0,
            "discharge_current_a": 3.0,
            "duration_s": 120,
            "time_step_s": 1,
            "initial_soc": 0.8,
            "pack_mass_kg": 1.0,
            "pack_heat_capacity_j_per_kgk": 900.0,
            "cooling_coeff_w_per_k": 1.0,
            "group_count": 4,
            "physics": {
                "hysteresis_enabled": True,
                "hysteresis_max_voltage_v": 0.04,
                "diffusion_stress_enabled": True,
                "diffusion_stress_max_v": 0.03,
                "two_node_thermal_enabled": True,
            },
        }
        result = run_simulation_from_dict(payload)
        self.assertIn("group_hysteresis_v", result["time_series"][-1])
        self.assertIn("group_core_temp", result["time_series"][-1])
        self.assertIn("group_core_temp_c", result["time_series"][-1])
        self.assertIn("max_core_temp_c", result["summary"])

    def test_backward_compatible_simple_run_still_works(self) -> None:
        result = run_simulation(make_config(duration_s=30))
        self.assertGreater(len(result.time_series), 0)

    def test_no_crash_full_feature_stress_run(self) -> None:
        physics = PhysicsConfig(
            discharge_efficiency=0.995,
            charge_efficiency=0.99,
            resistance_temperature_alpha_per_c=0.01,
            capacity_cold_derate_per_c=0.003,
            self_discharge_per_day=0.0001,
            interconnect_resistance_ohm_per_group=0.002,
            neighbor_thermal_coupling_w_per_k=0.2,
            resistance_vs_soc_enabled=True,
            hysteresis_enabled=True,
            hysteresis_max_voltage_v=0.05,
            rc_state_dependence_enabled=True,
            diffusion_stress_enabled=True,
            diffusion_stress_max_v=0.04,
            two_node_thermal_enabled=True,
            nonlinear_cooling_enabled=True,
            reversible_heat_enabled=True,
            reversible_heat_coeff_v_per_k=0.00008,
            charge_resistance_multiplier=1.15,
            discharge_resistance_multiplier=0.95,
        )
        result = run_simulation(make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=600,
            current_profile=CurrentProfile(points=(
                CurrentProfilePoint(0, 8.0),
                CurrentProfilePoint(120, 0.0),
                CurrentProfilePoint(240, -4.0),
                CurrentProfilePoint(360, 6.0),
                CurrentProfilePoint(480, 0.0),
            )),
            electrical_model=make_1rc_model(0.03, 0.018, 2200.0),
            physics=physics,
        ))
        self.assertGreater(len(result.time_series), 0)
        self.assertTrue(result.summary.nonlinear_features_enabled)


if __name__ == "__main__":
    unittest.main()
