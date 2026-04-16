from __future__ import annotations

import os
from dataclasses import dataclass, field
from pathlib import Path
from typing import Any

from .calibration import TRUTH_DATASET_STATUSES, TruthDataset, load_truth_dataset


DEFAULT_TRUTH_DATA_DIR = Path(__file__).resolve().parent / "truth_data"


@dataclass(frozen=True)
class TruthDatasetDescriptor:
    dataset_id: str
    display_name: str
    description: str
    chemistry: str
    form_factor: str
    nominal_voltage_v: float | None
    nominal_capacity_ah: float | None
    temperature_range_c: tuple[float, float] = ()
    current_profile_type: str = ""
    tags: tuple[str, ...] = ()
    source: str = ""
    status: str = "experimental"
    created_at: str = ""
    notes: str = ""
    dataset_path: str = ""
    record_count: int = 0
    metadata: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class TruthDatasetValidationResult:
    is_valid: bool
    dataset_path: str
    registry_ready: bool
    dataset_id: str | None = None
    display_name: str | None = None
    record_count: int = 0
    errors: tuple[str, ...] = ()
    warnings: tuple[str, ...] = ()
    metadata: dict[str, Any] = field(default_factory=dict)


@dataclass(frozen=True)
class ResolvedTruthDatasetInput:
    dataset_id: str
    dataset_display_name: str
    dataset_path: str
    source_type: str
    dataset: TruthDataset
    metadata: dict[str, Any] = field(default_factory=dict)


def _normalize_truth_data_dir(truth_data_dir: str | Path | None) -> Path:
    raw_value = truth_data_dir or os.environ.get("ALNORIS_TRUTH_DATA_DIR") or DEFAULT_TRUTH_DATA_DIR
    return Path(raw_value).expanduser().resolve()


def _status_rank(status: str) -> int:
    try:
        return TRUTH_DATASET_STATUSES.index(status)
    except ValueError:
        return len(TRUTH_DATASET_STATUSES)


def _descriptor_from_dataset(path: Path, dataset: TruthDataset) -> TruthDatasetDescriptor:
    metadata = dict(dataset.metadata)
    return TruthDatasetDescriptor(
        dataset_id=str(metadata.get("dataset_id", path.stem)),
        display_name=str(metadata.get("display_name", path.stem)),
        description=str(metadata.get("description", "")),
        chemistry=str(metadata.get("chemistry", "")),
        form_factor=str(metadata.get("form_factor", "")),
        nominal_voltage_v=float(metadata["nominal_voltage_v"]) if metadata.get("nominal_voltage_v") is not None else None,
        nominal_capacity_ah=float(metadata["nominal_capacity_ah"]) if metadata.get("nominal_capacity_ah") is not None else None,
        temperature_range_c=tuple(float(value) for value in metadata.get("temperature_range_c", ())[:2]),
        current_profile_type=str(metadata.get("current_profile_type", "")),
        tags=tuple(str(tag) for tag in metadata.get("tags", ())),
        source=str(metadata.get("source", "")),
        status=str(metadata.get("status", "experimental")),
        created_at=str(metadata.get("created_at", "")),
        notes=str(metadata.get("notes", "")),
        dataset_path=str(path),
        record_count=len(dataset.records),
        metadata=metadata,
    )


class TruthDatasetRegistry:
    def __init__(self, truth_data_dir: str | Path | None = None) -> None:
        self.truth_data_dir = _normalize_truth_data_dir(truth_data_dir)
        self.scan_errors: list[str] = []
        self._entries: dict[str, TruthDatasetDescriptor] = {}
        self._datasets: dict[str, TruthDataset] = {}
        self.refresh()

    def refresh(self) -> None:
        self.scan_errors = []
        self._entries = {}
        self._datasets = {}

        if not self.truth_data_dir.exists():
            return

        for path in sorted(self.truth_data_dir.rglob("*.json")):
            try:
                dataset = load_truth_dataset(str(path), strict_metadata=True)
                descriptor = _descriptor_from_dataset(path, dataset)
            except Exception as exc:
                self.scan_errors.append(f"{path}: {exc}")
                continue

            existing = self._entries.get(descriptor.dataset_id)
            if existing is not None:
                self.scan_errors.append(
                    f"Duplicate truth dataset_id '{descriptor.dataset_id}' found in {path} and {existing.dataset_path}."
                )
                continue

            self._entries[descriptor.dataset_id] = descriptor
            self._datasets[descriptor.dataset_id] = dataset

    def list_truth_datasets(self) -> list[TruthDatasetDescriptor]:
        return sorted(
            self._entries.values(),
            key=lambda item: (_status_rank(item.status), item.display_name.lower(), item.dataset_id.lower()),
        )

    def get_truth_dataset(self, dataset_id: str) -> ResolvedTruthDatasetInput:
        dataset_key = str(dataset_id).strip()
        descriptor = self._entries.get(dataset_key)
        dataset = self._datasets.get(dataset_key)
        if descriptor is None or dataset is None:
            available_ids = ", ".join(item.dataset_id for item in self.list_truth_datasets())
            raise ValueError(
                f"Unknown truth dataset_id '{dataset_key}'. Available datasets: {available_ids or 'none'}."
            )
        return ResolvedTruthDatasetInput(
            dataset_id=descriptor.dataset_id,
            dataset_display_name=descriptor.display_name,
            dataset_path=descriptor.dataset_path,
            source_type="registry",
            dataset=dataset,
            metadata=dict(descriptor.metadata),
        )

    def validate_truth_dataset_file(self, path: str | Path) -> TruthDatasetValidationResult:
        dataset_path = Path(path).expanduser().resolve()
        try:
            dataset = load_truth_dataset(str(dataset_path), strict_metadata=False)
        except Exception as exc:
            return TruthDatasetValidationResult(
                is_valid=False,
                dataset_path=str(dataset_path),
                registry_ready=False,
                errors=(str(exc),),
            )

        warnings: list[str] = []
        registry_ready = True
        try:
            load_truth_dataset(str(dataset_path), strict_metadata=True)
        except Exception as exc:
            registry_ready = False
            warnings.append(
                "Dataset is valid for direct-path replay but not yet registry-ready: "
                + str(exc)
            )

        status = str(dataset.metadata.get("status", "experimental"))
        if status == "experimental":
            warnings.append("Dataset is marked experimental and should be used with caution for decision support.")
        if status == "deprecated":
            warnings.append("Dataset is marked deprecated and should not be used as a primary validation reference.")

        return TruthDatasetValidationResult(
            is_valid=True,
            dataset_path=str(dataset_path),
            registry_ready=registry_ready,
            dataset_id=str(dataset.metadata.get("dataset_id", dataset_path.stem)),
            display_name=str(dataset.metadata.get("display_name", dataset_path.stem)),
            record_count=len(dataset.records),
            warnings=tuple(warnings),
            metadata=dict(dataset.metadata),
        )

    def resolve_truth_dataset_input(self, dataset_id_or_path: str) -> ResolvedTruthDatasetInput:
        raw_value = str(dataset_id_or_path).strip()
        if not raw_value:
            raise ValueError("Truth dataset input must be a dataset_id or dataset path.")

        candidate_path = Path(raw_value).expanduser()
        looks_like_path = candidate_path.suffix.lower() == ".json" or any(sep in raw_value for sep in ("/", "\\"))
        if candidate_path.exists() or looks_like_path:
            resolved_path = candidate_path.resolve()
            if not resolved_path.exists():
                raise ValueError(f"Truth dataset path does not exist: {resolved_path}")
            dataset = load_truth_dataset(str(resolved_path), strict_metadata=False)
            return ResolvedTruthDatasetInput(
                dataset_id=str(dataset.metadata.get("dataset_id", resolved_path.stem)),
                dataset_display_name=str(dataset.metadata.get("display_name", resolved_path.stem)),
                dataset_path=str(resolved_path),
                source_type="path",
                dataset=dataset,
                metadata=dict(dataset.metadata),
            )

        return self.get_truth_dataset(raw_value)


def list_truth_datasets(truth_data_dir: str | Path | None = None) -> list[TruthDatasetDescriptor]:
    return TruthDatasetRegistry(truth_data_dir).list_truth_datasets()


def get_truth_dataset(dataset_id: str, truth_data_dir: str | Path | None = None) -> ResolvedTruthDatasetInput:
    return TruthDatasetRegistry(truth_data_dir).get_truth_dataset(dataset_id)


def validate_truth_dataset_file(path: str | Path) -> TruthDatasetValidationResult:
    return TruthDatasetRegistry().validate_truth_dataset_file(path)


def resolve_truth_dataset_input(
    dataset_id_or_path: str,
    truth_data_dir: str | Path | None = None,
) -> ResolvedTruthDatasetInput:
    return TruthDatasetRegistry(truth_data_dir).resolve_truth_dataset_input(dataset_id_or_path)
