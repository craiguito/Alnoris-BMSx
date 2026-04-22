from __future__ import annotations

import csv
import json
import math
import zipfile
from dataclasses import dataclass
from pathlib import Path
from statistics import median
from typing import Any, Iterable

try:  # Optional dependency for offline data ingestion only.
    import numpy as np
    from scipy.io import loadmat
except Exception:  # pragma: no cover - exercised only when optional deps are missing
    np = None  # type: ignore[assignment]
    loadmat = None  # type: ignore[assignment]


DEFAULT_RELEVANT_BATTERY_IDS = (
    "B0005", "B0006", "B0007", "B0018",
    "B0025", "B0026", "B0027", "B0028",
    "B0029", "B0030", "B0031", "B0032",
    "B0038", "B0039", "B0040",
    "B0045", "B0046", "B0047", "B0048",
)

_CANONICAL_BATTERY_IDS = {
    "B0005", "B0006", "B0007", "B0018",
    "B0029", "B0030", "B0031", "B0032",
    "B0038", "B0039", "B0040",
    "B0045", "B0046", "B0047", "B0048",
}

_TRUSTED_BATTERY_IDS = {"B0025", "B0026", "B0027", "B0028"}


@dataclass(frozen=True)
class NasaCycleRecord:
    time_s: float
    current_a: float
    voltage_v: float
    temp_c: float
    soc: float
    extras: dict[str, Any]


@dataclass(frozen=True)
class NasaDischargeCycle:
    battery_id: str
    cycle_index: int
    ambient_temp_c: float
    timestamp: str
    capacity_ah: float | None
    current_profile_type: str
    status: str
    source_mat_path: str
    records: tuple[NasaCycleRecord, ...]


@dataclass(frozen=True)
class IngestionSummary:
    extracted_archives: tuple[str, ...]
    discovered_mat_files: tuple[str, ...]
    exported_dataset_paths: tuple[str, ...]
    skipped_battery_ids: tuple[str, ...]
    battery_summary_csv: str
    impedance_summary_csv: str


def _require_optional_dependencies() -> None:
    if np is None or loadmat is None:
        raise RuntimeError(
            "NASA Ames ingestion requires optional dependencies numpy and scipy. "
            "Install them in the environment used for the ingestion script."
        )


def _repo_root_from_any_path(path: Path) -> Path:
    for candidate in (path, *path.parents):
        if (candidate / "backend" / "sim_core").exists():
            return candidate
    raise ValueError(f"Could not locate repo root from {path}")


def _default_extract_root(data_root: Path) -> Path:
    return data_root / "_extracted" / "nasa_ames"


def extract_zip_archives(data_root: Path, extract_root: Path | None = None) -> list[Path]:
    extract_root = extract_root or _default_extract_root(data_root)
    extract_root.mkdir(parents=True, exist_ok=True)

    extracted_dirs: list[Path] = []
    processed_archives: set[Path] = set()
    search_roots = [data_root, extract_root]

    while True:
        new_archive_found = False
        for search_root in search_roots:
            for zip_path in sorted(search_root.rglob("*.zip")):
                resolved_zip_path = zip_path.resolve()
                if resolved_zip_path in processed_archives:
                    continue

                processed_archives.add(resolved_zip_path)
                target_dir = extract_root / resolved_zip_path.stem
                target_dir.mkdir(parents=True, exist_ok=True)

                with zipfile.ZipFile(resolved_zip_path) as archive:
                    archive.extractall(target_dir)

                extracted_dirs.append(target_dir)
                new_archive_found = True

        if not new_archive_found:
            break

    return extracted_dirs


def discover_mat_files(data_root: Path) -> list[Path]:
    mats = sorted(path for path in data_root.rglob("*.mat") if path.is_file())
    seen: set[Path] = set()
    unique: list[Path] = []

    for path in mats:
        resolved = path.resolve()
        if resolved in seen:
            continue
        seen.add(resolved)
        unique.append(resolved)

    return unique


def _mat_struct_to_array(value: Any) -> list[float]:
    if np is None:
        raise RuntimeError("numpy is required")
    arr = np.asarray(value).reshape(-1)
    return [float(item) for item in arr.tolist()]


def _safe_strftime(time_vector: Any) -> str:
    try:
        values = [int(float(value)) for value in list(np.asarray(time_vector).reshape(-1))[:6]]  # type: ignore[arg-type]
        year, month, day, hour, minute, second = values
        return f"{year:04d}-{month:02d}-{day:02d}T{hour:02d}:{minute:02d}:{second:02d}Z"
    except Exception:
        return ""


def _profile_type(currents: list[float]) -> str:
    if not currents:
        return "unknown_discharge"

    abs_currents = [abs(value) for value in currents]
    median_current = median(abs_currents) if abs_currents else 0.0
    if median_current <= 1e-6:
        return "unknown_discharge"

    step_count = 0
    for left, right in zip(abs_currents[:-1], abs_currents[1:]):
        if abs(right - left) > max(0.05, 0.1 * median_current):
            step_count += 1

    if step_count <= 3:
        return "constant_discharge"
    if step_count <= 20:
        return "stepped_discharge"
    return "pulse_discharge"


def _status_for_battery(battery_id: str) -> str:
    if battery_id in _CANONICAL_BATTERY_IDS:
        return "canonical"
    if battery_id in _TRUSTED_BATTERY_IDS:
        return "trusted"
    return "experimental"


def _parse_discharge_cycles(mat_path: Path) -> list[NasaDischargeCycle]:
    _require_optional_dependencies()
    payload = loadmat(str(mat_path), squeeze_me=True, struct_as_record=False)  # type: ignore[misc]
    battery_id = mat_path.stem

    if battery_id not in payload:
        raise ValueError(f"{mat_path} does not contain a top-level {battery_id} struct")

    battery_struct = payload[battery_id]
    cycles = getattr(battery_struct, "cycle", None)
    if cycles is None:
        return []

    parsed_cycles: list[NasaDischargeCycle] = []

    for cycle_index, cycle in enumerate(np.asarray(cycles).reshape(-1)):  # type: ignore[arg-type]
        if getattr(cycle, "type", "") != "discharge":
            continue

        data = getattr(cycle, "data", None)
        if data is None:
            continue

        time_values = _mat_struct_to_array(getattr(data, "Time", []))
        voltage_values = _mat_struct_to_array(getattr(data, "Voltage_measured", []))
        temp_values = _mat_struct_to_array(getattr(data, "Temperature_measured", []))
        measured_current_values = _mat_struct_to_array(getattr(data, "Current_measured", []))

        if len(time_values) < 10:
            continue
        if not (len(time_values) == len(voltage_values) == len(temp_values) == len(measured_current_values)):
            continue

        capacity_ah_raw = getattr(data, "Capacity", None)
        capacity_ah = float(capacity_ah_raw) if capacity_ah_raw is not None and math.isfinite(float(capacity_ah_raw)) else None
        if capacity_ah is not None and capacity_ah <= 0.0:
            capacity_ah = None

        records: list[NasaCycleRecord] = []
        elapsed_ah = 0.0
        previous_time_s: float | None = None

        for idx, (time_s, measured_current_a, voltage_v, temp_c) in enumerate(
            zip(time_values, measured_current_values, voltage_values, temp_values)
        ):
            if not all(math.isfinite(value) for value in (time_s, measured_current_a, voltage_v, temp_c)):
                continue

            # NASA discharge current is typically negative for discharge.
            # BMSx truth-data convention: positive current = discharge.
            current_a = max(0.0, -float(measured_current_a))
            normalized_time_s = max(0.0, float(time_s) - float(time_values[0]))

            if previous_time_s is not None and normalized_time_s <= previous_time_s:
                continue

            if previous_time_s is not None:
                dt_s = normalized_time_s - previous_time_s
                elapsed_ah += max(records[-1].current_a if records else current_a, 0.0) * dt_s / 3600.0

            if capacity_ah and capacity_ah > 0.0:
                soc = max(0.0, min(1.0, 1.0 - elapsed_ah / capacity_ah))
            else:
                soc = max(0.0, 1.0 - idx / max(len(time_values) - 1, 1))

            records.append(
                NasaCycleRecord(
                    time_s=normalized_time_s,
                    current_a=current_a,
                    voltage_v=float(voltage_v),
                    temp_c=float(temp_c),
                    soc=soc,
                    extras={
                        "source_cycle_type": "discharge",
                        "ambient_temp_c": float(getattr(cycle, "ambient_temperature", math.nan)),
                    },
                )
            )
            previous_time_s = normalized_time_s

        if len(records) < 10:
            continue

        parsed_cycles.append(
            NasaDischargeCycle(
                battery_id=battery_id,
                cycle_index=cycle_index,
                ambient_temp_c=float(getattr(cycle, "ambient_temperature", math.nan)),
                timestamp=_safe_strftime(getattr(cycle, "time", [])),
                capacity_ah=capacity_ah,
                current_profile_type=_profile_type([record.current_a for record in records]),
                status=_status_for_battery(battery_id),
                source_mat_path=str(mat_path),
                records=tuple(records),
            )
        )

    return parsed_cycles


def _select_curated_cycles(cycles: list[NasaDischargeCycle], max_cycles_per_battery: int) -> list[NasaDischargeCycle]:
    if max_cycles_per_battery <= 0 or len(cycles) <= max_cycles_per_battery:
        return cycles

    positions = sorted(
        {
            round(index * (len(cycles) - 1) / max(max_cycles_per_battery - 1, 1))
            for index in range(max_cycles_per_battery)
        }
    )
    return [cycles[position] for position in positions]


def _dataset_metadata(cycle: NasaDischargeCycle) -> dict[str, Any]:
    voltages = [record.voltage_v for record in cycle.records]
    temps = [record.temp_c for record in cycle.records]

    tags = ["nasa", "ames", "cell", "discharge", "aging", cycle.current_profile_type]
    if math.isfinite(cycle.ambient_temp_c):
        if cycle.ambient_temp_c <= 10.0:
            tags.append("cold")
        elif cycle.ambient_temp_c >= 40.0:
            tags.append("hot")
        else:
            tags.append("room-temp")

    dataset_id = f"nasa_ames_{cycle.battery_id.lower()}_discharge_{cycle.cycle_index:04d}"
    nominal_capacity = cycle.capacity_ah if cycle.capacity_ah and cycle.capacity_ah > 0.0 else None

    return {
        "dataset_id": dataset_id,
        "display_name": f"NASA Ames {cycle.battery_id} discharge cycle {cycle.cycle_index}",
        "description": (
            "Normalized NASA Ames single-cell discharge dataset for reduced-order calibration and "
            "single-cell validation. Intended for cell-model trust building, not direct pack-layout validation."
        ),
        "chemistry": "generic_liion",
        "form_factor": "cylindrical",
        "nominal_voltage_v": round(median(voltages), 4),
        "nominal_capacity_ah": round(float(nominal_capacity), 6) if nominal_capacity is not None else 1.0,
        "temperature_range_c": [round(min(temps), 3), round(max(temps), 3)],
        "current_profile_type": cycle.current_profile_type,
        "tags": tags,
        "source": "NASA Ames Battery Aging ARC",
        "status": cycle.status,
        "created_at": cycle.timestamp,
        "notes": (
            f"Source battery={cycle.battery_id}, cycle_index={cycle.cycle_index}, ambient_temp_c={cycle.ambient_temp_c:.1f}. "
            "Use for calibration and single-cell validation only."
        ),
        "ambient_temp_c": round(cycle.ambient_temp_c, 3) if math.isfinite(cycle.ambient_temp_c) else None,
        "initial_soc": 1.0,
        "cells_in_series": 1,
        "cells_in_parallel": 1,
        "group_count": 1,
        "source_battery_id": cycle.battery_id,
        "source_cycle_index": cycle.cycle_index,
        "source_mat_path": cycle.source_mat_path,
        "recommended_use": "single_cell_validation",
    }


def _dataset_payload(cycle: NasaDischargeCycle) -> dict[str, Any]:
    return {
        "metadata": _dataset_metadata(cycle),
        "records": [
            {
                "time_s": round(record.time_s, 6),
                "current_a": round(record.current_a, 9),
                "voltage_v": round(record.voltage_v, 9),
                "temp_c": round(record.temp_c, 6),
                "soc": round(record.soc, 9),
                **record.extras,
            }
            for record in cycle.records
        ],
    }


def _write_json(path: Path, payload: dict[str, Any]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(payload, indent=2), encoding="utf-8")


def _write_csv(path: Path, rows: Iterable[dict[str, Any]], fieldnames: list[str]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("w", encoding="utf-8", newline="") as handle:
        writer = csv.DictWriter(handle, fieldnames=fieldnames)
        writer.writeheader()
        for row in rows:
            writer.writerow(row)


def _load_impedance_summary_rows(mat_path: Path) -> list[dict[str, Any]]:
    _require_optional_dependencies()
    payload = loadmat(str(mat_path), squeeze_me=True, struct_as_record=False)  # type: ignore[misc]
    battery_id = mat_path.stem
    cycles = getattr(payload[battery_id], "cycle", None)
    if cycles is None:
        return []

    rows: list[dict[str, Any]] = []
    for cycle_index, cycle in enumerate(np.asarray(cycles).reshape(-1)):  # type: ignore[arg-type]
        if getattr(cycle, "type", "") != "impedance":
            continue

        data = getattr(cycle, "data", None)
        if data is None:
            continue

        re_value = getattr(data, "Re", None)
        rct_value = getattr(data, "Rct", None)
        if re_value is None and rct_value is None:
            continue

        rows.append(
            {
                "battery_id": battery_id,
                "cycle_index": cycle_index,
                "ambient_temp_c": float(getattr(cycle, "ambient_temperature", math.nan)),
                "timestamp": _safe_strftime(getattr(cycle, "time", [])),
                "re_ohm": float(re_value) if re_value is not None else "",
                "rct_ohm": float(rct_value) if rct_value is not None else "",
                "source_mat_path": str(mat_path),
            }
        )
    return rows


def ingest_nasa_ames_data(
    repo_root: str | Path,
    *,
    data_root: str | Path | None = None,
    export_dir: str | Path | None = None,
    relevant_battery_ids: Iterable[str] = DEFAULT_RELEVANT_BATTERY_IDS,
    max_cycles_per_battery: int = 3,
) -> IngestionSummary:
    _require_optional_dependencies()

    repo_root = _repo_root_from_any_path(Path(repo_root).resolve())
    data_root_path = Path(data_root).resolve() if data_root is not None else (repo_root / "Data").resolve()
    if not data_root_path.exists():
        raise FileNotFoundError(f"Data root does not exist: {data_root_path}")

    extracted_dirs = extract_zip_archives(data_root_path)
    mat_files = discover_mat_files(data_root_path)
    if not mat_files:
        raise FileNotFoundError(f"No .mat files were discovered under {data_root_path}")

    export_root = (
        Path(export_dir).resolve()
        if export_dir is not None
        else (repo_root / "backend" / "sim_core" / "truth_data" / "nasa_ames").resolve()
    )
    export_root.mkdir(parents=True, exist_ok=True)

    requested_ids = {battery_id.upper() for battery_id in relevant_battery_ids}
    exported_paths: list[str] = []
    skipped_ids: list[str] = []
    battery_rows: list[dict[str, Any]] = []
    impedance_rows: list[dict[str, Any]] = []

    discovered_by_id = {path.stem.upper(): path for path in mat_files}

    for battery_id in sorted(requested_ids):
        mat_path = discovered_by_id.get(battery_id)
        if mat_path is None:
            skipped_ids.append(battery_id)
            continue

        discharge_cycles = _parse_discharge_cycles(mat_path)
        selected_cycles = _select_curated_cycles(discharge_cycles, max_cycles_per_battery)
        if not selected_cycles:
            skipped_ids.append(battery_id)
            continue

        for cycle in selected_cycles:
            payload = _dataset_payload(cycle)
            status_dir = export_root / cycle.status / cycle.battery_id.lower()
            destination = status_dir / f"{payload['metadata']['dataset_id']}.json"
            _write_json(destination, payload)
            exported_paths.append(str(destination))

            battery_rows.append(
                {
                    "battery_id": cycle.battery_id,
                    "cycle_index": cycle.cycle_index,
                    "status": cycle.status,
                    "current_profile_type": cycle.current_profile_type,
                    "ambient_temp_c": round(cycle.ambient_temp_c, 3) if math.isfinite(cycle.ambient_temp_c) else "",
                    "record_count": len(cycle.records),
                    "capacity_ah": round(cycle.capacity_ah, 6) if cycle.capacity_ah is not None else "",
                    "dataset_id": payload["metadata"]["dataset_id"],
                    "dataset_path": str(destination),
                    "source_mat_path": str(mat_path),
                }
            )

        impedance_rows.extend(_load_impedance_summary_rows(mat_path))

    battery_summary_csv = export_root / "_nasa_ames_battery_summary.csv"
    impedance_summary_csv = export_root / "_nasa_ames_impedance_summary.csv"

    _write_csv(
        battery_summary_csv,
        battery_rows,
        [
            "battery_id",
            "cycle_index",
            "status",
            "current_profile_type",
            "ambient_temp_c",
            "record_count",
            "capacity_ah",
            "dataset_id",
            "dataset_path",
            "source_mat_path",
        ],
    )

    _write_csv(
        impedance_summary_csv,
        impedance_rows,
        [
            "battery_id",
            "cycle_index",
            "ambient_temp_c",
            "timestamp",
            "re_ohm",
            "rct_ohm",
            "source_mat_path",
        ],
    )

    return IngestionSummary(
        extracted_archives=tuple(str(path) for path in extracted_dirs),
        discovered_mat_files=tuple(str(path) for path in mat_files),
        exported_dataset_paths=tuple(exported_paths),
        skipped_battery_ids=tuple(sorted(set(skipped_ids))),
        battery_summary_csv=str(battery_summary_csv),
        impedance_summary_csv=str(impedance_summary_csv),
    )