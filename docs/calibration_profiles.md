# Calibration Profiles

`BMSx v1` now supports product-facing calibration profiles for the single-cell truth-data workflow. The immediate target is the curated NASA Ames room-temperature canonical envelope, because that is the first validation basis that directly supports the trade-study wedge.

## Objective Formula

Calibration candidate selection uses a normalized weighted objective:

`objective = sum(weight_i * (avg_metric_i / threshold_i)) / sum(weight_i)`

Where:

- `avg_metric_i` is the manifest-average value for a validation metric across the selected datasets
- `threshold_i` is the active validation threshold for that metric
- lower objective is better

This keeps voltage, energy, and temperature terms on a comparable scale and makes the weight choices explicit in artifacts.

## Profiles

### `electrical_first`

Default product-facing profile.

- Voltage RMSE weight: `0.45`
- Final voltage error weight: `0.30`
- Energy error weight: `0.20`
- Temperature RMSE weight: `0.05`

Intent:

- prioritize electrical fidelity first
- improve room-temperature comparative trust before broader thermal envelopes
- allow only light thermal blending so temperature fitting does not dominate voltage behavior

### `balanced_electro_thermal`

- Voltage RMSE weight: `0.35`
- Final voltage error weight: `0.25`
- Energy error weight: `0.20`
- Temperature RMSE weight: `0.20`

Intent:

- still care about voltage first
- permit more thermal compromise when temperature fit matters more

## Why Room Temperature First

The first operational envelope is the curated NASA Ames room canonical pack:

- `B0005`
- `B0006`
- `B0007`
- `B0018`

This is the narrowest product-relevant validation basis for `BMSx v1`:

- single-cell only
- canonical truth data
- directly tied to current trade-study decision support

## Running The Flow

Run the room-envelope calibration search:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile electrical_first
```

Key outputs:

- `build/room_envelope_calibration.json`
- `build/room_envelope_calibration.md`
- `build/room_envelope_parameters.json`

The JSON artifact includes:

- threshold profile used
- calibration profile used
- objective weights
- baseline validation summary
- selected post-calibration validation summary
- candidate scores
- per-dataset diagnostics

## Reading Diagnostics

Per-dataset diagnostics show:

- pre-calibration overall status
- post-calibration overall status
- metric deltas
- most improved metric
- most worsened metric

Use that artifact to answer:

- which datasets improved electrically
- where final voltage error is still blocking PASS
- whether thermal gains were bought by voltage regressions
- which anchor candidate the room-envelope search selected

## Current Limits

This remains a reduced-order single-cell calibration workflow.

- It is not a direct pack-layout validation method.
- It does not certify all ambient-temperature envelopes at once.
- A lower objective does not automatically mean the manifest now passes; it means the selected candidate is better under the chosen product-facing priorities.
