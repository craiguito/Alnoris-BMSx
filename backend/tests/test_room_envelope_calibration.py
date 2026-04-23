from __future__ import annotations

import json
import unittest
from dataclasses import replace
from pathlib import Path

from backend.sim_core.calibration import CalibratedParameters
from backend.sim_core.calibration_profiles import (
    evaluate_calibration_objective,
    get_calibration_profile,
)
from backend.sim_core.calibration_search_plans import (
    CalibrationSearchPlan,
    get_calibration_search_plan,
)
from backend.sim_core.engine import run_simulation
from backend.sim_core.room_envelope_calibration import (
    CalibrationCandidate,
    DatasetTransitionSummary,
    _anchor_priority_scores,
    _build_improvement_summary,
    _candidate_sanity_issues,
    _generate_candidates,
    build_room_envelope_benchmark,
    calibrate_room_envelope,
    room_envelope_calibration_to_dict,
)
from backend.sim_core.truth_data_manager import TruthDatasetRegistry
from backend.sim_core.validation_pack import (
    ValidationPackManifest,
    build_single_cell_validation_base_config,
    resolve_validation_manifest,
    run_validation_manifest_case,
)
from backend.sim_core.validation_threshold_profiles import get_validation_threshold_profile
from backend.tests.helpers import make_config, make_profile, temporary_workspace_dir, write_truth_dataset
from scripts.calibrate_room_envelope import _build_parser


def _registry_metadata(dataset_id: str, display_name: str, source_battery_id: str) -> dict[str, object]:
    return {
        "dataset_id": dataset_id,
        "display_name": display_name,
        "description": f"Synthetic room-envelope dataset for {display_name}.",
        "chemistry": "generic_liion",
        "form_factor": "cylindrical",
        "nominal_voltage_v": 3.7,
        "nominal_capacity_ah": 3.0,
        "temperature_range_c": [20.0, 35.0],
        "current_profile_type": "synthetic_pulse",
        "tags": ["synthetic", "room-envelope"],
        "source": "backend-tests",
        "status": "canonical",
        "created_at": "2026-04-22T00:00:00Z",
        "notes": "Synthetic dataset used for room-envelope calibration tests.",
        "ambient_temp_c": 25.0,
        "initial_soc": 1.0,
        "source_battery_id": source_battery_id,
        "source_cycle_index": 1,
    }


def _make_workspace(dataset_count: int = 2) -> tuple[str, Path, Path, object]:
    config = make_config(
        cells_in_series=1,
        cells_in_parallel=1,
        group_count=1,
        discharge_current_a=0.0,
        duration_s=45,
        current_profile=make_profile((0, 0.0), (5, 2.0), (20, 0.0), (30, 1.0)),
    )
    result = run_simulation(config)
    temp_dir = temporary_workspace_dir()
    workspace = temp_dir.__enter__()
    truth_dir = Path(workspace) / "truth_data"
    manifest_dir = Path(workspace) / "manifests"
    truth_dir.mkdir(parents=True, exist_ok=True)
    manifest_dir.mkdir(parents=True, exist_ok=True)
    dataset_ids: list[str] = []
    families = ["B0005", "B0006", "B0007", "B0018"]
    for index in range(dataset_count):
        dataset_id = f"synthetic_room_{index + 1}"
        dataset_ids.append(dataset_id)
        write_truth_dataset(
            truth_dir / f"{dataset_id}.json",
            config,
            result,
            extra_metadata=_registry_metadata(dataset_id, f"Synthetic Room {index + 1}", families[index]),
        )
    (manifest_dir / "synthetic_room.json").write_text(
        json.dumps(
            {
                "manifest_id": "synthetic_room",
                "display_name": "Synthetic Room Envelope",
                "description": "Synthetic manifest for room-envelope calibration tests.",
                "dataset_ids": dataset_ids,
                "recommended_threshold_profile": "single_cell_v1_nasa",
                "calibration_dataset_id": dataset_ids[0],
                "validation_basis_label": "Synthetic room basis",
                "validation_basis_description": "Synthetic room basis used in tests.",
                "validation_limits": [
                    "Validated at single-cell level only.",
                    "Intended for comparative trade-study support only."
                ]
            },
            indent=2,
        ),
        encoding="utf-8",
    )
    return workspace, truth_dir, manifest_dir, temp_dir


class RoomEnvelopeCalibrationTests(unittest.TestCase):
    def test_weighted_objective_prioritizes_voltage_over_temperature(self) -> None:
        scorecards = [
            {
                "metric_results": [
                    {"metric_id": "rmse_voltage", "value": 0.12, "threshold_value": 0.05},
                    {"metric_id": "final_voltage_error", "value": 0.16, "threshold_value": 0.08},
                    {"metric_id": "energy_error", "value": 0.04, "threshold_value": 0.08},
                    {"metric_id": "temp_rmse", "value": 0.5, "threshold_value": 2.5},
                ]
            }
        ]
        electrical_first = evaluate_calibration_objective(
            scorecards,
            weights=get_calibration_profile("electrical_first").objective_weights,
        )
        balanced = evaluate_calibration_objective(
            scorecards,
            weights=get_calibration_profile("balanced_electro_thermal").objective_weights,
        )

        self.assertGreater(electrical_first.total_score, balanced.total_score)
        self.assertIn("avg_metric_i / threshold_i", electrical_first.normalized_formula)

    def test_search_plan_selection_and_defaults(self) -> None:
        fast = get_calibration_search_plan(None, calibration_profile_id="electrical_first")
        balanced = get_calibration_search_plan(None, calibration_profile_id="balanced_electro_thermal")
        self.assertEqual(fast.plan_id, "fast_product_default")
        self.assertEqual(balanced.plan_id, "balanced_default")

    def test_weighted_anchor_scoring_changes_by_profile(self) -> None:
        manifest = ValidationPackManifest(
            manifest_id="synthetic",
            display_name="Synthetic",
            description="Synthetic",
            dataset_ids=("a", "b"),
            recommended_threshold_profile="single_cell_v1_nasa",
            calibration_dataset_id="a",
        )
        baseline_payload = {
            "validation_scorecards": [
                {
                    "dataset_id": "a",
                    "metric_results": [
                        {"metric_id": "rmse_voltage", "value": 0.04, "threshold_value": 0.05},
                        {"metric_id": "final_voltage_error", "value": 0.04, "threshold_value": 0.08},
                        {"metric_id": "energy_error", "value": 0.02, "threshold_value": 0.08},
                        {"metric_id": "temp_rmse", "value": 10.0, "threshold_value": 2.5},
                    ],
                },
                {
                    "dataset_id": "b",
                    "metric_results": [
                        {"metric_id": "rmse_voltage", "value": 0.09, "threshold_value": 0.05},
                        {"metric_id": "final_voltage_error", "value": 0.12, "threshold_value": 0.08},
                        {"metric_id": "energy_error", "value": 0.04, "threshold_value": 0.08},
                        {"metric_id": "temp_rmse", "value": 0.5, "threshold_value": 2.5},
                    ],
                },
            ]
        }

        electrical_scores = _anchor_priority_scores(
            manifest,
            baseline_payload,
            calibration_profile=get_calibration_profile("electrical_first"),
        )
        balanced_scores = _anchor_priority_scores(
            manifest,
            baseline_payload,
            calibration_profile=get_calibration_profile("balanced_electro_thermal"),
        )

        self.assertGreater(electrical_scores["b"], electrical_scores["a"])
        self.assertGreater(balanced_scores["a"], balanced_scores["b"])

    def test_fast_product_default_is_narrower_than_exhaustive_debug(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=2)
        try:
            fast_artifact = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                search_plan_id="fast_product_default",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
            exhaustive_artifact = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                search_plan_id="exhaustive_debug",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
        finally:
            temp_dir.__exit__(None, None, None)

        self.assertLess(
            fast_artifact.summary.generated_candidate_count,
            exhaustive_artifact.summary.generated_candidate_count,
        )

    def test_candidate_generation_order_is_deterministic(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=2)
        try:
            artifact_a = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                search_plan_id="fast_product_default",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
            artifact_b = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                search_plan_id="fast_product_default",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
        finally:
            temp_dir.__exit__(None, None, None)

        self.assertEqual(
            artifact_a.summary.candidate_search_preview.generated_candidate_ids,
            artifact_b.summary.candidate_search_preview.generated_candidate_ids,
        )

    def test_candidate_filtering_heuristic_rejects_invalid_parameters(self) -> None:
        bad_candidate = CalibrationCandidate(
            candidate_id="bad",
            candidate_mode="single_anchor",
            anchor_dataset_ids=("synthetic_room_1",),
            anchor_priority_score=1.0,
            rc_branch_count=1,
            recipe_id="bad_recipe",
            electro_blend=1.0,
            thermal_blend=0.0,
            calibrated_parameters=CalibratedParameters(
                ocv_curve=(),
                base_resistance_ohm_per_cell=-1.0,
                resistance_soc_curve=(),
                resistance_temperature_alpha_per_c=0.0,
                rc_branches=(),
                core_thermal_mass_j_per_k=-5.0,
                surface_thermal_mass_j_per_k=-2.0,
                cooling_coeff_w_per_k=0.0,
                capacity_fade_per_throughput_ah=0.0,
                resistance_growth_per_throughput_ah=0.0,
                calendar_capacity_fade_per_hour=0.0,
                calendar_resistance_growth_per_hour=0.0,
            ),
        )

        issues = _candidate_sanity_issues(bad_candidate)
        self.assertTrue(any("base_resistance_ohm_per_cell" in issue for issue in issues))
        self.assertTrue(any("ocv_curve" in issue for issue in issues))

    def test_max_candidates_guardrail_filters_tail_candidates(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=2)
        try:
            manifest = resolve_validation_manifest("synthetic_room", manifest_dir=manifest_dir)
            threshold = get_validation_threshold_profile("single_cell_v1_nasa")
            profile = get_calibration_profile("electrical_first")
            registry = TruthDatasetRegistry(truth_dir)
            anchor = registry.get_truth_dataset(manifest.calibration_dataset_id)
            base_config = build_single_cell_validation_base_config(anchor.metadata)
            baseline_payload = run_validation_manifest_case(
                manifest,
                threshold,
                base_config=base_config,
                truth_data_dir=truth_dir,
                anchor_dataset_id=manifest.calibration_dataset_id,
                calibration_metadata={
                    "calibration_profile_id": profile.profile_id,
                    "objective_weights": profile.objective_weights.as_metric_weights(),
                    "calibration_objective": None,
                    "search_plan_id": "test_guardrail",
                },
            )
            limited_plan = replace(
                get_calibration_search_plan("fast_product_default"),
                max_candidates=3,
            )
            raw_count, candidates, filtered, preview = _generate_candidates(
                manifest,
                registry,
                baseline_payload,
                calibration_profile=profile,
                search_plan=limited_plan,
            )
        finally:
            temp_dir.__exit__(None, None, None)

        self.assertGreater(raw_count, len(candidates))
        self.assertEqual(len(candidates), 3)
        self.assertEqual(len(preview.generated_candidate_ids), 3)
        self.assertTrue(any(item.reason == "max_candidates_guardrail" for item in filtered))

    def test_room_envelope_summary_and_filtered_accounting_exist(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=2)
        try:
            artifact = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                search_plan_id="fast_product_default",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
        finally:
            temp_dir.__exit__(None, None, None)

        self.assertGreater(artifact.summary.generated_candidate_count, 0)
        self.assertGreaterEqual(artifact.summary.raw_candidate_count, artifact.summary.generated_candidate_count)
        self.assertGreaterEqual(artifact.summary.filtered_candidate_count, 1)
        self.assertTrue(any(item.reason == "screen_rank_cutoff" for item in artifact.filtered_candidates))
        self.assertEqual(artifact.summary.best_candidate_id, artifact.selected_candidate["candidate_id"])

    def test_benchmark_rows_include_baseline_and_profile_rows(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=2)
        try:
            electrical = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                calibration_profile_id="electrical_first",
                search_plan_id="fast_product_default",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
            balanced = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                calibration_profile_id="balanced_electro_thermal",
                search_plan_id="balanced_default",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
            benchmark = build_room_envelope_benchmark([electrical, balanced])
        finally:
            temp_dir.__exit__(None, None, None)

        self.assertGreaterEqual(len(benchmark.rows), 3)
        self.assertEqual(benchmark.rows[0].result_id, "baseline_default_model")
        self.assertIn("Baseline / Default Model", benchmark.markdown)
        self.assertIn("electrical_first", benchmark.markdown)
        self.assertIn("balanced_electro_thermal", benchmark.markdown)

    def test_status_transition_summary_generation(self) -> None:
        diagnostics = (
            DatasetTransitionSummary(
                dataset_id="a",
                dataset_display_name="Dataset A",
                baseline_status="fail",
                candidate_status="warning",
                status_transition="FAIL->WARNING",
                status_change=1,
                baseline_objective_score=2.0,
                candidate_objective_score=1.5,
                objective_delta=-0.5,
            ),
            DatasetTransitionSummary(
                dataset_id="b",
                dataset_display_name="Dataset B",
                baseline_status="warning",
                candidate_status="fail",
                status_transition="WARNING->FAIL",
                status_change=-1,
                baseline_objective_score=1.0,
                candidate_objective_score=1.2,
                objective_delta=0.2,
            ),
        )
        summary = _build_improvement_summary(diagnostics)
        self.assertEqual(summary.status_upgrade_count, 1)
        self.assertEqual(summary.status_downgrade_count, 1)
        self.assertEqual(summary.transition_counts["FAIL->WARNING"], 1)
        self.assertEqual(summary.transition_counts["WARNING->FAIL"], 1)

    def test_cli_parser_accepts_profile_and_search_plan(self) -> None:
        parser = _build_parser()
        args = parser.parse_args([
            "--calibration-profile", "balanced_electro_thermal",
            "--search-plan", "balanced_default",
            "--benchmark-profiles", "electrical_first", "balanced_electro_thermal",
        ])
        self.assertEqual(args.calibration_profile, "balanced_electro_thermal")
        self.assertEqual(args.search_plan, "balanced_default")
        self.assertEqual(args.benchmark_profiles, ["electrical_first", "balanced_electro_thermal"])

    def test_artifact_persists_profile_and_search_metadata(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=2)
        try:
            artifact = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
            payload = room_envelope_calibration_to_dict(artifact)
        finally:
            temp_dir.__exit__(None, None, None)

        self.assertEqual(payload["summary"]["calibration_profile_id"], "electrical_first")
        self.assertEqual(payload["summary"]["search_plan_id"], "fast_product_default")
        self.assertIn("benchmark_comparison", payload)

    def test_single_anchor_manifest_still_runs(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=1)
        try:
            artifact = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
        finally:
            temp_dir.__exit__(None, None, None)

        self.assertEqual(len(artifact.summary.candidate_search_preview.selected_anchor_dataset_ids), 1)
        self.assertTrue(artifact.candidate_evaluations)

    def test_per_dataset_diagnostics_include_required_keys(self) -> None:
        workspace, truth_dir, manifest_dir, temp_dir = _make_workspace(dataset_count=2)
        try:
            artifact = calibrate_room_envelope(
                manifest_id_or_path="synthetic_room",
                truth_data_dir=truth_dir,
                manifest_dir=manifest_dir,
            )
        finally:
            temp_dir.__exit__(None, None, None)

        diagnostic = artifact.per_dataset_diagnostics[0]
        self.assertTrue(diagnostic.status_transition)
        self.assertIsInstance(diagnostic.metric_deltas, dict)
        self.assertIn("baseline_status", room_envelope_calibration_to_dict(artifact)["per_dataset_diagnostics"][0])


if __name__ == "__main__":
    unittest.main()
