from __future__ import annotations

import unittest

from backend.sim_core.calibration import truth_dataset_from_dict
from backend.sim_core.calibration_profiles import evaluate_scorecard_objective, get_calibration_profile
from backend.sim_core.engine import run_simulation
from backend.sim_core.validation_scorecard import build_validation_scorecard, summarize_validation_scorecards
from backend.tests.helpers import build_truth_dataset_payload, make_config, make_profile


def _metadata(dataset_id: str, display_name: str, *, cycle_index: int) -> dict[str, object]:
    return {
        "dataset_id": dataset_id,
        "display_name": display_name,
        "description": f"Synthetic validation dataset for {display_name}.",
        "chemistry": "generic_liion",
        "form_factor": "cylindrical",
        "nominal_voltage_v": 3.7,
        "nominal_capacity_ah": 3.0,
        "temperature_range_c": [20.0, 35.0],
        "current_profile_type": "synthetic_pulse",
        "tags": ["synthetic", "validation-scorecard"],
        "source": "backend-tests",
        "status": "canonical",
        "created_at": "2026-04-23T00:00:00Z",
        "notes": "Synthetic scorecard dataset.",
        "ambient_temp_c": 25.0,
        "initial_soc": 1.0,
        "source_battery_id": "B0005",
        "source_cycle_index": cycle_index,
    }


def _thresholds() -> dict[str, float]:
    return {
        "max_voltage_rmse_v": 0.05,
        "max_final_voltage_error_v": 0.08,
        "max_energy_error_fraction": 0.08,
        "max_temp_rmse_c": 2.5,
        "max_low_soc_voltage_rmse_v": 0.06,
        "max_last_10_percent_voltage_rmse_v": 0.07,
    }


class ValidationScorecardTests(unittest.TestCase):
    def _build_result_and_dataset(self, *, cycle_index: int, flatten_soc: bool = False, truncate_to: int | None = None):
        config = make_config(
            cells_in_series=1,
            cells_in_parallel=1,
            group_count=1,
            discharge_current_a=0.0,
            duration_s=4200,
            time_step_s=10,
            current_profile=make_profile((0, 0.0), (10, 3.0), (3600, 3.0), (3900, 1.0), (4190, 0.0)),
        )
        result = run_simulation(config)
        payload = build_truth_dataset_payload(
            config,
            result,
            extra_metadata=_metadata(f"scorecard_{cycle_index}", f"Scorecard {cycle_index}", cycle_index=cycle_index),
        )
        if flatten_soc:
            for row in payload["data"]:
                row["soc"] = 1.0
        if truncate_to is not None:
            payload["data"] = payload["data"][:truncate_to]
        dataset = truth_dataset_from_dict(payload, strict_metadata=True, source_path=f"scorecard_{cycle_index}.json")
        return config, result, dataset

    def test_segmented_metrics_are_emitted_for_soc_segments(self) -> None:
        config, result, dataset = self._build_result_and_dataset(cycle_index=293)
        scorecard = build_validation_scorecard(
            result,
            dataset,
            dataset_path="synthetic.json",
            base_config=config,
            requested_metrics=("rmse_voltage", "final_voltage_error", "energy_error", "temp_rmse"),
            thresholds=_thresholds(),
        )

        self.assertEqual(scorecard.segmentation_basis, "soc")
        self.assertEqual(scorecard.aging_stage, "mid_life")
        self.assertEqual(len(scorecard.segmented_metrics), 3)
        low_soc = next(item for item in scorecard.segmented_metrics if item.segment_id == "low_soc")
        self.assertGreater(low_soc.sample_count, 0)
        self.assertIsNotNone(scorecard.tail_metrics)
        self.assertLess(scorecard.tail_metrics.final_voltage_error_v, 1.0e-6)

    def test_segmented_metrics_fall_back_to_progress_when_soc_is_not_reliable(self) -> None:
        config, result, dataset = self._build_result_and_dataset(cycle_index=1, flatten_soc=True)
        scorecard = build_validation_scorecard(
            result,
            dataset,
            dataset_path="synthetic.json",
            base_config=config,
            requested_metrics=("rmse_voltage",),
            thresholds=_thresholds(),
        )

        self.assertEqual(scorecard.segmentation_basis, "normalized_progress")
        low_soc = next(item for item in scorecard.segmented_metrics if item.segment_id == "low_soc")
        self.assertGreater(low_soc.sample_count, 0)

    def test_small_segments_degrade_gracefully_without_crashing(self) -> None:
        config, result, dataset = self._build_result_and_dataset(cycle_index=613, truncate_to=3)
        scorecard = build_validation_scorecard(
            result,
            dataset,
            dataset_path="synthetic.json",
            base_config=config,
            requested_metrics=("rmse_voltage",),
            thresholds=_thresholds(),
        )

        low_soc = next(item for item in scorecard.segmented_metrics if item.segment_id == "low_soc")
        self.assertLessEqual(low_soc.sample_count, 1)
        self.assertIsNone(low_soc.voltage_rmse_v)
        self.assertLessEqual(scorecard.tail_metrics.sample_count, 1)
        self.assertIsNone(scorecard.tail_metrics.last_10_percent_voltage_rmse_v)

    def test_aging_stage_summary_groups_datasets_by_cycle_index(self) -> None:
        scorecards = []
        for cycle_index in (1, 293, 613):
            config, result, dataset = self._build_result_and_dataset(cycle_index=cycle_index)
            scorecards.append(
                build_validation_scorecard(
                    result,
                    dataset,
                    dataset_path=f"{cycle_index}.json",
                    base_config=config,
                    requested_metrics=("rmse_voltage", "final_voltage_error", "energy_error"),
                    thresholds=_thresholds(),
                )
            )

        summary = summarize_validation_scorecards(scorecards)
        stage_lookup = {item.aging_stage: item for item in summary.aging_stage_summaries}
        self.assertEqual(stage_lookup["early_life"].dataset_count, 1)
        self.assertEqual(stage_lookup["mid_life"].dataset_count, 1)
        self.assertEqual(stage_lookup["late_life"].dataset_count, 1)

    def test_tail_guarded_profile_uses_segmented_weights(self) -> None:
        candidate_a = {
            "metric_results": [
                {"metric_id": "rmse_voltage", "value": 0.045, "threshold_value": 0.05},
                {"metric_id": "final_voltage_error", "value": 0.070, "threshold_value": 0.08},
                {"metric_id": "energy_error", "value": 0.040, "threshold_value": 0.08},
                {"metric_id": "temp_rmse", "value": 1.0, "threshold_value": 2.5},
            ],
            "segmented_metrics": [
                {"segment_id": "low_soc", "voltage_rmse_v": 0.120, "threshold_voltage_rmse_v": 0.06},
            ],
            "tail_metrics": {
                "last_10_percent_voltage_rmse_v": 0.140,
                "last_10_percent_threshold_v": 0.07,
            },
        }
        candidate_b = {
            "metric_results": [
                {"metric_id": "rmse_voltage", "value": 0.050, "threshold_value": 0.05},
                {"metric_id": "final_voltage_error", "value": 0.080, "threshold_value": 0.08},
                {"metric_id": "energy_error", "value": 0.045, "threshold_value": 0.08},
                {"metric_id": "temp_rmse", "value": 1.0, "threshold_value": 2.5},
            ],
            "segmented_metrics": [
                {"segment_id": "low_soc", "voltage_rmse_v": 0.050, "threshold_voltage_rmse_v": 0.06},
            ],
            "tail_metrics": {
                "last_10_percent_voltage_rmse_v": 0.060,
                "last_10_percent_threshold_v": 0.07,
            },
        }

        electrical_first = get_calibration_profile("electrical_first").objective_weights
        tail_guarded = get_calibration_profile("electrical_tail_guarded").objective_weights
        electrical_score_a = evaluate_scorecard_objective(candidate_a, weights=electrical_first)
        electrical_score_b = evaluate_scorecard_objective(candidate_b, weights=electrical_first)
        tail_score_a = evaluate_scorecard_objective(candidate_a, weights=tail_guarded)
        tail_score_b = evaluate_scorecard_objective(candidate_b, weights=tail_guarded)

        self.assertLess(electrical_score_a.total_score, electrical_score_b.total_score)
        self.assertLess(tail_score_b.total_score, tail_score_a.total_score)
        self.assertIn("low_soc_voltage_rmse", tail_score_b.metrics_used)
        self.assertIn("last_10_percent_voltage_rmse", tail_score_b.metrics_used)

    def test_tail_guarded_profile_skips_missing_segmented_metrics_safely(self) -> None:
        scorecard = {
            "metric_results": [
                {"metric_id": "rmse_voltage", "value": 0.045, "threshold_value": 0.05},
                {"metric_id": "final_voltage_error", "value": 0.070, "threshold_value": 0.08},
                {"metric_id": "energy_error", "value": 0.040, "threshold_value": 0.08},
            ],
        }
        result = evaluate_scorecard_objective(
            scorecard,
            weights=get_calibration_profile("electrical_tail_guarded").objective_weights,
        )

        self.assertGreater(result.total_score, 0.0)
        self.assertNotIn("low_soc_voltage_rmse", result.metrics_used)


if __name__ == "__main__":
    unittest.main()
