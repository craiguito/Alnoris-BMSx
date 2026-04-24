from __future__ import annotations

import json
import unittest
from pathlib import Path

from backend.sim_core.calibration import (
    apply_calibration_to_simulation_config,
    build_validation_config,
    calibrate_parameters,
    load_truth_dataset,
)
from backend.sim_core.engine import run_simulation
from backend.sim_core.physics.electrical import compute_open_circuit_voltage
from backend.sim_core.physics.pack import derive_pack_properties
from backend.sim_core.reference_cells import REFERENCE_CELLS

from backend.tests.helpers import make_config, make_profile, temporary_workspace_dir, write_truth_dataset


class CalibrationTests(unittest.TestCase):
    def test_truth_dataset_loader_backfills_soc_from_current_integral(self) -> None:
        with temporary_workspace_dir() as temp_dir:
            dataset_path = f"{temp_dir}/truth.json"
            with open(dataset_path, "w", encoding="utf-8") as handle:
                json.dump(
                    {
                        "metadata": {
                            "initial_soc": 0.9,
                            "capacity_ah": 2.0,
                        },
                        "data": [
                            {"time_s": 0, "current_a": 1.0, "voltage_v": 4.1, "temp_c": 25.0},
                            {"time_s": 3600, "current_a": 1.0, "voltage_v": 3.9, "temp_c": 25.5},
                            {"time_s": 7200, "current_a": 0.0, "voltage_v": 3.7, "temp_c": 26.0},
                        ],
                    },
                    handle,
                    indent=2,
                )

            dataset = load_truth_dataset(dataset_path)

        self.assertEqual(len(dataset.records), 3)
        self.assertAlmostEqual(dataset.records[0].soc, 0.9, places=6)
        self.assertAlmostEqual(dataset.records[1].soc, 0.4, places=6)
        self.assertAlmostEqual(dataset.records[2].soc, 0.0, places=6)

    def test_open_circuit_curve_matches_full_nominal_and_empty_metrics(self) -> None:
        for key, cell in REFERENCE_CELLS.items():
            config = make_config(key)

            full_v = compute_open_circuit_voltage(1.0, config)
            nominal_v = compute_open_circuit_voltage(0.5, config)
            empty_v = compute_open_circuit_voltage(0.0, config)

            self.assertAlmostEqual(full_v, cell.cell_full_voltage, places=6, msg=cell.name)
            self.assertAlmostEqual(nominal_v, cell.cell_nominal_voltage, places=6, msg=cell.name)
            self.assertAlmostEqual(empty_v, cell.cell_empty_voltage, places=6, msg=cell.name)

    def test_one_c_discharge_runs_close_to_expected_capacity_window(self) -> None:
        for key, cell in REFERENCE_CELLS.items():
            config = make_config(
                key,
                discharge_current_a=cell.cell_capacity_ah,
                duration_s=5400,
            )
            result = run_simulation(config)
            elapsed_s = result.time_series[-1].time_s
            delivered_ah = cell.cell_capacity_ah * elapsed_s / 3600.0

            self.assertGreater(delivered_ah, 0.70 * cell.cell_capacity_ah, cell.name)
            self.assertLess(delivered_ah, 1.10 * cell.cell_capacity_ah, cell.name)

    def test_higher_power_cell_sags_less_than_energy_cell_at_same_current(self) -> None:
        energy_cell = make_config("panasonic_ncr18650b")
        power_cell = make_config("samsung_30q")
        energy_pack = derive_pack_properties(energy_cell)
        power_pack = derive_pack_properties(power_cell)

        current_a = 5.0
        soc = 0.8
        energy_v = compute_open_circuit_voltage(soc, energy_cell, energy_pack.series_factor) - (
            current_a * energy_pack.group_base_resistance_ohm
        )
        power_v = compute_open_circuit_voltage(soc, power_cell, power_pack.series_factor) - (
            current_a * power_pack.group_base_resistance_ohm
        )

        self.assertGreater(power_v, energy_v)

    def test_summary_contains_backward_compatible_and_new_fields(self) -> None:
        config = make_config(
            "panasonic_ncr18650b",
            discharge_current_a=10.0,
            duration_s=1800,
        )
        result = run_simulation(config)

        self.assertGreaterEqual(result.summary.runtime_s, 0)
        self.assertGreaterEqual(result.summary.delivered_energy_wh, 0.0)
        self.assertTrue(result.summary.termination_reason)
        self.assertIsInstance(result.summary.warnings, list)
        self.assertEqual(result.summary.electrical_model_type, "rint")
        self.assertGreaterEqual(result.summary.estimated_capacity_retention, 0.0)
        self.assertGreaterEqual(result.summary.final_soc_avg, 0.0)
        self.assertGreaterEqual(result.time_series[-1].pack_voltage_v, 0.0)
        self.assertIn("current_a", result.time_series[-1].__dict__)
        self.assertIn("group_soc", result.time_series[-1].__dict__)
        self.assertIn("total_energy_wh", result.summary.__dict__)

    def test_calibration_pipeline_extracts_and_applies_fitted_parameters(self) -> None:
        config = make_config(
            "panasonic_ncr18650b",
            cells_in_series=4,
            group_count=4,
            discharge_current_a=0.0,
            duration_s=90,
            current_profile=make_profile((0, 0.0), (10, 2.5), (35, 0.0), (50, 1.5), (75, 0.0)),
        )
        result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            dataset_path = write_truth_dataset(f"{temp_dir}/truth.json", config, result)
            dataset = load_truth_dataset(str(dataset_path))

        calibrated = calibrate_parameters(dataset, rc_branch_count=1)
        calibrated_config = apply_calibration_to_simulation_config(config, calibrated)

        self.assertGreaterEqual(len(calibrated.ocv_curve), 5)
        self.assertGreater(calibrated.base_resistance_ohm_per_cell, 0.0)
        self.assertGreaterEqual(len(calibrated.resistance_soc_curve), 8)
        self.assertGreaterEqual(calibrated.rc_low_soc_multiplier, 1.0)
        self.assertEqual(len(calibrated.rc_branches), 1)
        self.assertTrue(calibrated_config.physics.ocv_curve)
        self.assertTrue(calibrated_config.physics.two_node_thermal_enabled)
        self.assertTrue(calibrated_config.physics.resistance_vs_soc_enabled)
        self.assertGreaterEqual(calibrated_config.physics.rc_low_soc_multiplier, 1.0)
        self.assertEqual(calibrated_config.electrical_model.model_type, "1rc")
        self.assertAlmostEqual(
            calibrated_config.electrical_model.r0_ohm_per_cell,
            calibrated.base_resistance_ohm_per_cell,
            places=9,
        )
        self.assertAlmostEqual(
            calibrated_config.internal_resistance_ohm_per_cell,
            calibrated.base_resistance_ohm_per_cell,
            places=9,
        )

    def test_tail_voltage_depression_increases_low_soc_rc_multiplier(self) -> None:
        config = make_config(
            "panasonic_ncr18650b",
            cells_in_series=1,
            group_count=1,
            discharge_current_a=0.0,
            duration_s=4200,
            time_step_s=5,
            current_profile=make_profile((0, 0.0), (10, 3.0), (3600, 3.0), (3900, 1.0), (4195, 0.0)),
        )
        result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            dataset_path = write_truth_dataset(f"{temp_dir}/tail_truth.json", config, result)
            payload = json.loads(Path(dataset_path).read_text(encoding="utf-8"))
            for row in payload["data"]:
                if float(row.get("soc", 1.0)) <= 0.25:
                    row["voltage_v"] = float(row["voltage_v"]) - 0.18
            Path(dataset_path).write_text(json.dumps(payload, indent=2), encoding="utf-8")
            dataset = load_truth_dataset(str(dataset_path))

        calibrated = calibrate_parameters(dataset, rc_branch_count=1)

        self.assertGreater(calibrated.rc_low_soc_multiplier, 1.0)

    def test_age_conditioned_tail_adjustment_is_applied_more_strongly_for_late_life(self) -> None:
        config = make_config(
            "panasonic_ncr18650b",
            cells_in_series=1,
            group_count=1,
            discharge_current_a=0.0,
            duration_s=4200,
            time_step_s=5,
            current_profile=make_profile((0, 0.0), (10, 3.0), (3600, 3.0), (3900, 1.0), (4195, 0.0)),
        )
        result = run_simulation(config)

        with temporary_workspace_dir() as temp_dir:
            dataset_path = write_truth_dataset(
                f"{temp_dir}/aged_tail_truth.json",
                config,
                result,
                extra_metadata={
                    "dataset_id": "aged_tail_truth",
                    "display_name": "Aged Tail Truth",
                    "description": "Synthetic aged tail truth dataset.",
                    "chemistry": "generic_liion",
                    "form_factor": "cylindrical",
                    "nominal_voltage_v": 3.7,
                    "nominal_capacity_ah": config.cell_capacity_ah,
                    "temperature_range_c": [20.0, 35.0],
                    "current_profile_type": "synthetic_pulse",
                    "tags": ["synthetic", "aged-tail"],
                    "source": "backend-tests",
                    "status": "canonical",
                    "created_at": "2026-04-23T00:00:00Z",
                    "notes": "Synthetic aged-tail dataset.",
                    "ambient_temp_c": 25.0,
                    "source_battery_id": "B0006",
                    "source_cycle_index": 650,
                },
            )
            payload = json.loads(Path(dataset_path).read_text(encoding="utf-8"))
            for row in payload["data"]:
                if float(row.get("soc", 1.0)) <= 0.20:
                    row["voltage_v"] = float(row["voltage_v"]) - 0.16
            Path(dataset_path).write_text(json.dumps(payload, indent=2), encoding="utf-8")
            dataset = load_truth_dataset(str(dataset_path), strict_metadata=True)

        calibrated = calibrate_parameters(dataset, rc_branch_count=1)
        calibrated_config = apply_calibration_to_simulation_config(config, calibrated)
        early_dataset = dataset.__class__(metadata={**dataset.metadata, "source_cycle_index": 50}, records=dataset.records)
        late_config = build_validation_config(calibrated_config, dataset)
        early_config = build_validation_config(calibrated_config, early_dataset)

        self.assertTrue(calibrated.age_conditioned_tail_enabled)
        self.assertGreater(calibrated.age_conditioned_tail_resistance_gain, 0.0)
        self.assertGreater(late_config.physics.rc_low_soc_multiplier, early_config.physics.rc_low_soc_multiplier)
        self.assertGreater(
            late_config.physics.resistance_soc_curve[0].multiplier,
            early_config.physics.resistance_soc_curve[0].multiplier,
        )
        if calibrated.age_conditioned_tail_ocv_drop_v > 0.0:
            self.assertLess(
                late_config.physics.ocv_curve[0].voltage_v,
                early_config.physics.ocv_curve[0].voltage_v,
            )


if __name__ == "__main__":
    unittest.main()
