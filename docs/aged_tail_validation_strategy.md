# Aged-Tail Validation Strategy

The room-temperature envelope is no longer blocked mainly by generic fitting infrastructure. The dominant remaining weakness is aged or late-life end-of-discharge behavior:

- late-life datasets still carry the highest room-envelope tail error
- low-SOC voltage collapse remains harder to match than mid-pack behavior
- aggregate-best and tail-best candidates can diverge

This document keeps the response narrow: improve late-life single-cell tail fidelity for the existing trade-study cockpit.

## What `aged_tail_guarded` Is For

`aged_tail_guarded` is the product-facing profile for late-life low-SOC work.

Metric weights:

- Voltage RMSE: `0.14`
- Final voltage error: `0.16`
- Energy error: `0.08`
- Temperature RMSE: `0.02`
- Low-SOC voltage RMSE: `0.22`
- Last-10%-of-discharge voltage RMSE: `0.18`
- Cutoff-neighborhood voltage RMSE: `0.20`

Aging-stage weights:

- Early-life: `0.55`
- Mid-life: `1.00`
- Late-life: `1.85`
- Unknown: `0.75`

Intent:

- prioritize late-life low-SOC fidelity directly
- make cutoff-neighborhood behavior visible in candidate ranking
- keep some protection against early-life regression without letting early-life dominate the objective

## How Stage Weighting Works

For profiles with non-uniform aging-stage weights, scorecard observations are weighted by inferred stage before metric averages are normalized by threshold.

Practical effect:

- late-life scorecards count more toward anchor ranking
- late-life pain influences screening and final candidate selection more strongly
- artifacts explicitly show when stage weighting was active and which stage weights were used

If `source_cycle_index` is unavailable:

- the dataset falls back to `unknown`
- the `unknown` stage weight is used
- the run remains deterministic

## How Aging Stage Is Inferred

The current heuristic stays intentionally narrow and deterministic:

- `early_life`: `source_cycle_index <= 200`
- `mid_life`: `201-500`
- `late_life`: `> 500`
- `unknown`: cycle index missing or invalid

This is diagnostic grouping only. It is not a broad lifecycle productization feature.

## Age-Conditioned Tail Handling

The reduced-order model is still the same model family. The change is local and optional.

When calibration extracts enough tail signal, it now carries three optional age-conditioned tail terms:

- low-SOC resistance gain
- low-SOC OCV drop
- low-SOC RC multiplier gain

These terms are only applied in validation-time configs and only below the configured low-SOC threshold. Stage scales then determine how strongly the adjustment is applied for early-, mid-, or late-life datasets.

This keeps the behavior:

- explainable
- bounded
- compatible with the current reduced-order architecture

It does not introduce a new electrochemical solver or a new product area.

## New Benchmark Views

`room_envelope_benchmark.json` and `room_envelope_benchmark.md` now contain:

- baseline + best-candidate comparison rows
- stage-aware tables for `early_life`, `mid_life`, `late_life`, and `unknown`
- average voltage RMSE
- average low-SOC voltage RMSE
- average last-10% voltage RMSE
- average final voltage error
- average energy error fraction
- pass/warn/fail counts by stage

The diagnostics artifact also adds:

- late-life improvement leaderboard
- late-life remaining-error leaderboard
- early-life regression leaderboard
- aggregate-best vs tail-best vs aged-tail-best comparison

## Running It

Fast product default:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile aged_tail_guarded
```

Broader debug run:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile aged_tail_guarded --search-plan exhaustive_debug
```

## Current Limits

- This remains a single-cell validation envelope, not direct pack-layout validation.
- The bounded candidate search can still select `baseline` when no candidate clears the stricter late-life tail objective.
- Late-life tail improvements can still trade off against early-life smoothness because the model is still one reduced-order parameter family, not a broad aging platform.
- The current stage heuristic is cycle-index based; it is useful for diagnostics and weighting, but it is not a full lifecycle state estimator.
