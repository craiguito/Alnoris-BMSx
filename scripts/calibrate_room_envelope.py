from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from backend.sim_core.room_envelope_calibration import (
    build_room_envelope_benchmark,
    room_envelope_benchmark_to_dict,
    room_envelope_calibration_to_dict,
    calibrate_room_envelope,
    set_room_envelope_artifact_paths,
)


def _build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(
        description="Run room-temperature envelope calibration for the curated NASA Ames room manifest."
    )
    parser.add_argument("--manifest", default="nasa_room_canonical", help="Manifest id or path.")
    parser.add_argument("--calibration-profile", default="electrical_first", help="Calibration profile id.")
    parser.add_argument("--search-plan", default=None, help="Optional search plan override.")
    parser.add_argument("--benchmark-profiles", nargs="*", default=["electrical_first", "balanced_electro_thermal", "electrical_tail_guarded", "aged_tail_guarded"], help="Calibration profiles to include in benchmark comparison.")
    parser.add_argument("--threshold-profile", default=None, help="Optional threshold profile override.")
    parser.add_argument("--truth-data-dir", default=None, help="Optional truth-data directory override.")
    parser.add_argument("--manifest-dir", default=None, help="Optional validation-manifest directory override.")
    parser.add_argument("--output", default="build/room_envelope_calibration.json", help="Primary JSON artifact path.")
    parser.add_argument("--markdown", default="build/room_envelope_calibration.md", help="Markdown diagnostics path.")
    parser.add_argument("--parameters-output", default="build/room_envelope_parameters.json", help="Selected parameter artifact path.")
    parser.add_argument("--benchmark-output", default="build/room_envelope_benchmark.json", help="Benchmark JSON artifact path.")
    parser.add_argument("--benchmark-markdown", default="build/room_envelope_benchmark.md", help="Benchmark markdown artifact path.")
    return parser


def _worst_tail_dataset_status(artifact: object) -> str:
    diagnostics = getattr(artifact, "per_dataset_diagnostics", ())
    if not diagnostics:
        return "Worst tail dataset: n/a"
    ranked = sorted(
        diagnostics,
        key=lambda item: float(item.baseline_tail_metrics.get("last_10_percent_voltage_rmse_v") or -1.0),
        reverse=True,
    )
    worst = ranked[0]
    baseline_tail = worst.baseline_tail_metrics.get("last_10_percent_voltage_rmse_v")
    candidate_tail = worst.candidate_tail_metrics.get("last_10_percent_voltage_rmse_v")
    if baseline_tail is None or candidate_tail is None:
        return f"Worst tail dataset: {worst.dataset_id} (tail RMSE unavailable)"
    improved = float(candidate_tail) < float(baseline_tail)
    return (
        f"Worst tail dataset: {worst.dataset_id} "
        f"({'improved' if improved else 'not improved'}; "
        f"{float(baseline_tail):.4f} -> {float(candidate_tail):.4f})"
    )


def _write_json(path: str, payload: dict[str, object]) -> str:
    output_path = Path(path).expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    return str(output_path)


def _write_text(path: str, content: str) -> str:
    output_path = Path(path).expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(content, encoding="utf-8")
    return str(output_path)


def _stage_summary(payload: dict[str, object], stage_id: str) -> dict[str, object]:
    validation_summary = dict(payload.get("validation_summary", {})) if isinstance(payload, dict) else {}
    for item in validation_summary.get("aging_stage_summaries", []) or []:
        if str(item.get("aging_stage", "")) == stage_id:
            return dict(item)
    return {}


def main(argv: list[str] | None = None) -> int:
    args = _build_parser().parse_args(argv)

    selected_artifact = calibrate_room_envelope(
        manifest_id_or_path=args.manifest,
        calibration_profile_id=args.calibration_profile,
        search_plan_id=args.search_plan,
        threshold_profile_id=args.threshold_profile,
        truth_data_dir=args.truth_data_dir,
        manifest_dir=args.manifest_dir,
    )

    benchmark_profile_ids: list[str] = []
    for profile_id in [args.calibration_profile, *args.benchmark_profiles]:
        key = str(profile_id).strip()
        if key and key not in benchmark_profile_ids:
            benchmark_profile_ids.append(key)

    artifacts_by_profile = {selected_artifact.calibration_profile.profile_id: selected_artifact}
    for profile_id in benchmark_profile_ids:
        if profile_id in artifacts_by_profile:
            continue
        artifacts_by_profile[profile_id] = calibrate_room_envelope(
            manifest_id_or_path=args.manifest,
            calibration_profile_id=profile_id,
            search_plan_id=args.search_plan,
            threshold_profile_id=args.threshold_profile,
            truth_data_dir=args.truth_data_dir,
            manifest_dir=args.manifest_dir,
        )

    benchmark = build_room_envelope_benchmark(
        [artifacts_by_profile[profile_id] for profile_id in benchmark_profile_ids if profile_id in artifacts_by_profile]
    )

    primary_json_path = Path(args.output).expanduser().resolve()
    markdown_path = Path(args.markdown).expanduser().resolve()
    parameters_path = Path(args.parameters_output).expanduser().resolve()
    benchmark_json_path = Path(args.benchmark_output).expanduser().resolve()
    benchmark_markdown_path = Path(args.benchmark_markdown).expanduser().resolve()

    selected_artifact = set_room_envelope_artifact_paths(
        selected_artifact,
        primary_json=str(primary_json_path),
        diagnostics_markdown=str(markdown_path),
        parameters_json=str(parameters_path),
        benchmark_json=str(benchmark_json_path),
        benchmark_markdown=str(benchmark_markdown_path),
        benchmark_comparison=benchmark,
    )
    payload = room_envelope_calibration_to_dict(selected_artifact)

    _write_json(str(primary_json_path), payload)
    _write_text(str(markdown_path), selected_artifact.diagnostics_markdown)
    _write_json(str(parameters_path), selected_artifact.selected_candidate)
    _write_json(str(benchmark_json_path), room_envelope_benchmark_to_dict(benchmark))
    _write_text(str(benchmark_markdown_path), benchmark.markdown)

    before_counts = selected_artifact.summary.baseline_status_counts
    after_counts = selected_artifact.summary.selected_status_counts
    baseline_objective = float(selected_artifact.summary.baseline_low_soc_objective_score)
    selected_low_soc_objective = float(selected_artifact.summary.selected_low_soc_objective_score)
    baseline_overall_objective = float(selected_artifact.baseline_validation.get("validation_summary", {}).get("calibration_objective", {}).get("total_score", 0.0))
    selected_overall_objective = float(selected_artifact.selected_validation.get("validation_summary", {}).get("calibration_objective", {}).get("total_score", 0.0))
    baseline_stage_aware_objective = float(selected_artifact.summary.baseline_stage_aware_objective_score)
    selected_stage_aware_objective = float(selected_artifact.summary.selected_stage_aware_objective_score)
    baseline_aged_tail_objective = float(selected_artifact.summary.baseline_aged_tail_objective_score)
    selected_aged_tail_objective = float(selected_artifact.summary.selected_aged_tail_objective_score)
    baseline_late_life = _stage_summary(selected_artifact.baseline_validation, "late_life")
    selected_late_life = _stage_summary(selected_artifact.selected_validation, "late_life")
    tail_candidate_comparison = selected_artifact.summary.tail_candidate_comparison
    improvement_summary = selected_artifact.summary.improvement_summary
    print(f"Room Envelope Calibration")
    print(f"Manifest: {selected_artifact.manifest.manifest_id}")
    print(f"Calibration profile: {selected_artifact.calibration_profile.profile_id}")
    print(f"Search plan: {selected_artifact.search_plan.plan_id}")
    print(
        "Candidates: raw={raw}, after_pruning={generated}, filtered={filtered}, full_validation={full}".format(
            raw=selected_artifact.summary.raw_candidate_count,
            generated=selected_artifact.summary.generated_candidate_count,
            filtered=selected_artifact.summary.filtered_candidate_count,
            full=selected_artifact.summary.full_validation_candidate_count,
        )
    )
    print(f"Winner: {selected_artifact.summary.best_candidate_id}")
    print(
        "Before: pass={pass_count}, warn={warn_count}, fail={fail_count}".format(
            pass_count=before_counts.get("pass", 0),
            warn_count=before_counts.get("warning", 0),
            fail_count=before_counts.get("fail", 0),
        )
    )
    print(
        "After: pass={pass_count}, warn={warn_count}, fail={fail_count}".format(
            pass_count=after_counts.get("pass", 0),
            warn_count=after_counts.get("warning", 0),
            fail_count=after_counts.get("fail", 0),
        )
    )
    print(
        "Overall objective: {before:.4f} -> {after:.4f}".format(
            before=baseline_overall_objective,
            after=selected_overall_objective,
        )
    )
    print(
        "Low-SOC objective: {before:.4f} -> {after:.4f}".format(
            before=baseline_objective,
            after=selected_low_soc_objective,
        )
    )
    print(
        "Stage-aware objective: {before:.4f} -> {after:.4f}".format(
            before=baseline_stage_aware_objective,
            after=selected_stage_aware_objective,
        )
    )
    print(
        "Aged-tail objective: {before:.4f} -> {after:.4f}".format(
            before=baseline_aged_tail_objective,
            after=selected_aged_tail_objective,
        )
    )
    if baseline_late_life or selected_late_life:
        print(
            "Late-life low-SOC RMSE: {before:.4f} -> {after:.4f}".format(
                before=float(baseline_late_life.get("average_low_soc_voltage_rmse_v", 0.0) or 0.0),
                after=float(selected_late_life.get("average_low_soc_voltage_rmse_v", 0.0) or 0.0),
            )
        )
        print(
            "Late-life status mix: {before_pass}/{before_warn}/{before_fail} -> {after_pass}/{after_warn}/{after_fail}".format(
                before_pass=int(baseline_late_life.get("passed_count", 0)),
                before_warn=int(baseline_late_life.get("warning_count", 0)),
                before_fail=int(baseline_late_life.get("failed_count", 0)),
                after_pass=int(selected_late_life.get("passed_count", 0)),
                after_warn=int(selected_late_life.get("warning_count", 0)),
                after_fail=int(selected_late_life.get("failed_count", 0)),
            )
        )
    if tail_candidate_comparison is not None:
        print(
            "Aggregate-best vs tail-best vs aged-tail-best: {aggregate} vs {tail} vs {aged} ({relation})".format(
                aggregate=tail_candidate_comparison.aggregate_best_candidate_id,
                tail=tail_candidate_comparison.low_soc_best_candidate_id,
                aged=tail_candidate_comparison.aged_tail_best_candidate_id,
                relation=(
                    "same"
                    if tail_candidate_comparison.same_candidate and tail_candidate_comparison.same_as_aged_tail
                    else "different"
                ),
            )
        )
    if improvement_summary is not None:
        print(
            "Late-life improved/regressed: {improved}/{regressed}".format(
                improved=improvement_summary.late_life_improved_count,
                regressed=improvement_summary.late_life_regressed_count,
            )
        )
        print(
            "Early-life materially regressed: {count}".format(
                count=improvement_summary.early_life_material_regression_count,
            )
        )
    print(_worst_tail_dataset_status(selected_artifact))
    print(f"Recommendation: {selected_artifact.recommendation_text}")
    print(f"Artifacts: {primary_json_path}")
    print(f"Diagnostics markdown: {markdown_path}")
    print(f"Benchmark JSON: {benchmark_json_path}")
    print(f"Benchmark markdown: {benchmark_markdown_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
