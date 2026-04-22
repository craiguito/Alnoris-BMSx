from __future__ import annotations

import argparse
import json
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from backend.sim_core.validation_pack import run_validation_pack, validation_pack_run_to_dict


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description="Run a curated BMSx validation pack against registered truth datasets."
    )
    parser.add_argument("manifest", help="Validation manifest id or path to a manifest JSON file.")
    parser.add_argument("--profile", dest="profile_id", default=None, help="Optional threshold profile override.")
    parser.add_argument("--truth-data-dir", dest="truth_data_dir", default=None, help="Optional truth-data directory override.")
    parser.add_argument("--manifest-dir", dest="manifest_dir", default=None, help="Optional validation-manifest directory override.")
    parser.add_argument("--no-calibrated", action="store_true", help="Skip the calibrated comparison pass and run only the default model.")
    parser.add_argument("--output", dest="output_path", default=None, help="Optional path for the JSON summary artifact.")
    parser.add_argument("--comparison-md", dest="comparison_markdown_path", default=None, help="Optional path for the markdown comparison artifact.")
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    artifact = run_validation_pack(
        args.manifest,
        threshold_profile_id=args.profile_id,
        truth_data_dir=args.truth_data_dir,
        manifest_dir=args.manifest_dir,
        include_calibrated=not args.no_calibrated,
    )
    payload = validation_pack_run_to_dict(artifact)

    if args.output_path:
        output_path = Path(args.output_path).expanduser().resolve()
        output_path.parent.mkdir(parents=True, exist_ok=True)
        output_path.write_text(json.dumps(payload, indent=2), encoding="utf-8")
    else:
        print(json.dumps(payload, indent=2))

    if args.comparison_markdown_path:
        markdown_path = Path(args.comparison_markdown_path).expanduser().resolve()
        markdown_path.parent.mkdir(parents=True, exist_ok=True)
        markdown_path.write_text(artifact.comparison_markdown, encoding="utf-8")

    return 0


if __name__ == "__main__":
    raise SystemExit(main())
