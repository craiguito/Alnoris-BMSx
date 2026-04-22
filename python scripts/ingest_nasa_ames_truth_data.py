#!/usr/bin/env python3
from __future__ import annotations

import argparse
from pathlib import Path
import sys

REPO_ROOT = Path(__file__).resolve().parents[1]
if str(REPO_ROOT) not in sys.path:
    sys.path.insert(0, str(REPO_ROOT))

from backend.sim_core.importers.nasa_ames import (
    DEFAULT_RELEVANT_BATTERY_IDS,
    ingest_nasa_ames_data,
)


def _parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(
        description=(
            "Normalize relevant NASA Ames battery aging .mat files into BMSx truth-data JSON files. "
            "By default the script looks for source archives under <repo>/Data and writes normalized datasets "
            "under backend/sim_core/truth_data/nasa_ames/."
        )
    )
    parser.add_argument(
        "--repo-root",
        type=Path,
        default=REPO_ROOT,
        help="Path to the Alnoris-BMSx repo root.",
    )
    parser.add_argument(
        "--data-root",
        type=Path,
        default=None,
        help="Path to the root Data directory containing NASA .zip/.mat files.",
    )
    parser.add_argument(
        "--export-dir",
        type=Path,
        default=None,
        help="Optional override for the truth-data export directory.",
    )
    parser.add_argument(
        "--battery-ids",
        nargs="*",
        default=list(DEFAULT_RELEVANT_BATTERY_IDS),
        help="NASA battery ids to ingest (default: curated relevant set).",
    )
    parser.add_argument(
        "--max-cycles-per-battery",
        type=int,
        default=3,
        help="Number of curated discharge cycles to export per battery (default: 3 = early/mid/late).",
    )
    return parser.parse_args()


def main() -> int:
    args = _parse_args()
    summary = ingest_nasa_ames_data(
        args.repo_root,
        data_root=args.data_root,
        export_dir=args.export_dir,
        relevant_battery_ids=args.battery_ids,
        max_cycles_per_battery=args.max_cycles_per_battery,
    )

    print("NASA Ames ingestion complete")
    print(f"  Extracted archives: {len(summary.extracted_archives)}")
    print(f"  Discovered .mat files: {len(summary.discovered_mat_files)}")
    print(f"  Exported truth datasets: {len(summary.exported_dataset_paths)}")

    if summary.skipped_battery_ids:
        print("  Skipped battery ids:", ", ".join(summary.skipped_battery_ids))

    print(f"  Battery summary CSV: {summary.battery_summary_csv}")
    print(f"  Impedance summary CSV: {summary.impedance_summary_csv}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())