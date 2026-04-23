# Low-SOC Validation Strategy

`BMSx v1` still struggles most near the tail of discharge on the NASA room-temperature envelope. The product question is not whether the infrastructure exists anymore. The question is whether the reduced-order model is electrically trustworthy in the last 10-20% SOC across early-, mid-, and late-life single-cell datasets.

## Why Low-SOC Matters

The room canonical pack can still warn or fail even when overall energy error looks acceptable because comparative trade-study confidence depends heavily on:

- voltage RMSE staying controlled through the discharge tail
- final voltage error not collapsing at cutoff
- aged cells not drifting into visibly worse end-of-discharge mismatch

## Segmented Metrics

Validation scorecards now compute electrical error by segment:

- `high_soc`: `0.66-1.00`
- `mid_soc`: `0.33-0.66`
- `low_soc`: `0.00-0.33`

Preferred basis:

- truth-dataset SOC when the trace spans enough SOC and stays monotonic enough

Fallback basis:

- normalized discharge progress when SOC is too flat or unreliable

Each segment records:

- `sample_count`
- `voltage_rmse_v`
- `max_abs_voltage_error_v`
- `average_voltage_error_v`

Tail-specific metrics also include:

- `last_10_percent_voltage_rmse_v`
- `final_voltage_error_v`
- `cutoff_neighborhood_voltage_rmse_v` when enough tail samples exist

## Aging-Stage Grouping

Diagnostics summarize room-envelope results by aging stage using `source_cycle_index`:

- `early_life`: cycle `<= 200`
- `mid_life`: cycle `201-500`
- `late_life`: cycle `> 500`

If cycle metadata is missing, the dataset is marked `unknown`.

Each aging-stage summary reports:

- average voltage RMSE
- average low-SOC voltage RMSE
- average last-10% voltage RMSE
- average final voltage error
- average energy error
- pass/warn/fail counts

## Tail-Guarded Profile

Use `electrical_tail_guarded` when the product need is specifically end-of-discharge electrical fit:

- overall voltage RMSE: `0.22`
- low-SOC voltage RMSE: `0.28`
- last-10% voltage RMSE: `0.18`
- final voltage error: `0.22`
- energy error: `0.08`
- temperature RMSE: `0.02`

This profile keeps the workflow narrow:

- no new test catalog breadth
- no new battery product area
- no new solver class

## Low-SOC Model Changes

The reduced-order fitter now improves low-SOC handling without changing the architecture:

- nonuniform OCV SOC knots are denser near empty SOC
- nonuniform resistance-vs-SOC knots are denser near empty SOC
- resistance fitting combines step-derived and under-load estimates instead of stopping at the first step estimate
- reference resistance is anchored in mid-SOC rather than letting tail behavior inflate the whole resistance level
- RC low-SOC scaling is inferred and applied through the existing `rc_state_dependence_enabled` path

## Reading The Benchmark

`build/room_envelope_benchmark.json` and `.md` now show:

- average overall voltage RMSE
- average low-SOC voltage RMSE
- average last-10% voltage RMSE
- average final voltage error
- early/mid/late-life pass-warn-fail counts
- whether the aggregate-best candidate is also the tail-best candidate

If the aggregate-best candidate and tail-best candidate differ, the workflow is telling us the room-envelope global objective is still trading away some tail behavior.

## Running It

Fast room-envelope run with tail focus:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile electrical_tail_guarded
```

Broader offline tail investigation:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile electrical_tail_guarded --search-plan exhaustive_debug
```

## Current Limits

- This is still a single-cell validation envelope, not direct pack-layout validation.
- The fitter is still heuristic and candidate-based rather than a full global optimizer.
- Tail diagnostics can expose aged-cell failure modes clearly, but strong PASS still depends on how much fidelity the current reduced-order structure can represent across battery families and aging states.
