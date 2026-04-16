# Truth Dataset Schema

The trade-study validation feature reads truth datasets from JSON files. These datasets are used to replay a current trace, compare simulated output against truth data, and generate validation scorecards for decision support.

## Purpose

Truth datasets are not a general lab-data platform. They exist to strengthen the pack trade-study cockpit:

- archetype -> layout -> flagship tests -> baseline/candidate comparison -> report
- truth data adds the trust layer for `model_validation`
- validation scorecards determine whether a pack model is suitable for comparative trade-study use

## File shape

Each file must contain a top-level JSON object with:

- `metadata`: dataset identity, trust tier, and comparison metadata
- `records`: canonical list of time-series samples

`data` is still accepted as a legacy alias for `records` so older `dataset_path` workflows continue to work.

## Metadata

Registry-managed datasets should define:

- `dataset_id`: stable id used by the registry and desktop UI
- `display_name`: user-facing name
- `description`
- `chemistry`
- `form_factor`
- `nominal_voltage_v`
- `nominal_capacity_ah`
- `temperature_range_c`: `[min_c, max_c]`
- `current_profile_type`
- `tags`: array of strings
- `source`
- `status`: `canonical`, `trusted`, `experimental`, or `deprecated`
- `created_at`
- `notes`

Legacy direct-path datasets may omit registry metadata, but they will only be valid for one-off replay and will not appear in the managed registry until the required metadata is added.

## Records

Each record object supports:

- `time_s`
- `current_a`
- `voltage_v`
- `temp_c`
- `soc`
- `extras`: optional additional fields preserved as extra record keys

## Validation rules

The loader enforces:

- `records` must be present and non-empty
- `time_s` must be strictly increasing
- numeric fields must be finite
- registry-managed datasets must include the metadata needed to identify and compare datasets
- malformed files return helpful error messages including the dataset path when available

If `soc` is omitted, the loader back-fills SOC from current integration using metadata such as `initial_soc`, `capacity_ah`, `cell_capacity_ah`, or `nominal_capacity_ah`.

## Example

```json
{
  "metadata": {
    "dataset_id": "canonical_cylindrical_baseline",
    "display_name": "Canonical Cylindrical Baseline",
    "chemistry": "generic_liion",
    "form_factor": "cylindrical",
    "nominal_voltage_v": 14.8,
    "nominal_capacity_ah": 3.2,
    "temperature_range_c": [20.0, 32.0],
    "current_profile_type": "pulse_discharge",
    "source": "Synthetic example",
    "status": "canonical"
  },
  "records": [
    {"time_s": 0, "current_a": 0.0, "voltage_v": 15.0, "temp_c": 24.8, "soc": 1.0},
    {"time_s": 5, "current_a": 2.4, "voltage_v": 14.82, "temp_c": 25.0, "soc": 0.99}
  ]
}
```

## Default bundled data

The repo now includes small synthetic example datasets in `backend/sim_core/truth_data/`. They are intentionally tiny and deterministic so the registry, desktop browser, and report export have a stable seed set on first launch.
