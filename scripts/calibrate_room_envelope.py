from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from backend.sim_core.room_envelope_calibration import (
    calibrate_room_envelope,
    room_envelope_calibration_to_dict,
)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run weighted room-temperature envelope calibration for the curated NASA Ames room manifest."
    )
    parser.add_argument("--manifest", default="nasa_room_canonical", help="Manifest id or path.")
    parser.add_argument("--calibration-profile", default="electrical_first", help="Calibration profile id.")
    parser.add_argument("--threshold-profile", default=None, help="Optional threshold profile override.")
    parser.add_argument("--truth-data-dir", default=None, help="Optional truth-data directory override.")
    parser.add_argument("--manifest-dir", default=None, help="Optional validation-manifest directory override.")
    parser.add_argument("--output", default="build/room_envelope_calibration.json", help="Primary JSON artifact path.")
    parser.add_argument("--markdown", default="build/room_envelope_calibration.md", help="Markdown diagnostics path.")
    parser.add_argument("--parameters-output", default="build/room_envelope_parameters.json", help="Selected parameter artifact path.")
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    artifact = calibrate_room_envelope(
        manifest_id_or_path=args.manifest,
        calibration_profile_id=args.calibration_profile,
        threshold_profile_id=args.threshold_profile,
        truth_data_dir=args.truth_data_dir,
        manifest_dir=args.manifest_dir,
    )
    payload = room_envelope_calibration_to_dict(artifact)

    output_path = Path(args.output).expanduser().resolve()
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")

    markdown_path = Path(args.markdown).expanduser().resolve()
    markdown_path.parent.mkdir(parents=True, exist_ok=True)
    markdown_path.write_text(artifact.diagnostics_markdown, encoding="utf-8")

    parameters_path = Path(args.parameters_output).expanduser().resolve()
    parameters_path.parent.mkdir(parents=True, exist_ok=True)
    parameters_path.write_text(json.dumps(artifact.selected_candidate, indent=2), encoding="utf-8")

    print(
        json.dumps(
            {
                "ok": True,
                "manifest_id": artifact.manifest.manifest_id,
                "calibration_profile_id": artifact.calibration_profile.profile_id,
                "recommendation_text": artifact.recommendation_text,
                "selected_candidate_id": artifact.selected_candidate["candidate_id"],
                "baseline_objective": artifact.baseline_validation["validation_summary"]["calibration_objective"]["total_score"],
                "selected_objective": artifact.selected_validation["validation_summary"]["calibration_objective"]["total_score"],
                "output": str(output_path),
            },
            indent=2,
        )
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
