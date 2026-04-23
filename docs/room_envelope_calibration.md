# Room-Envelope Calibration

The room-envelope workflow is the narrow product-facing path for improving trust in `BMSx v1` against curated NASA Ames room-temperature single-cell data.

Primary manifest:

- `nasa_room_canonical`

Battery families:

- `B0005`
- `B0006`
- `B0007`
- `B0018`

This is still a single-cell validation envelope for comparative trade-study support. It is not direct pack-layout validation.

## Profile Vs Search Plan

Calibration profile:

- defines what the optimizer cares about
- examples: `electrical_first`, `balanced_electro_thermal`, `electrical_tail_guarded`

Search plan:

- defines how many candidate structures are explored
- examples: `fast_product_default`, `balanced_default`, `exhaustive_debug`

## Search Plans

### `fast_product_default`

Default product workflow.

- top weighted room anchors only
- `rc_branch_count` in `{1, 2}`
- two pragmatic blend recipes
- small screening subset
- tight full-validation cap

Use when:

- iterating quickly
- deciding whether electrical fidelity improved enough to keep moving

### `balanced_default`

- still pruned
- slightly broader blend coverage
- intended for electro-thermal comparison work

Use when:

- comparing `electrical_first` vs `balanced_electro_thermal`
- inspecting whether thermal improvements are worth electrical compromise

### `exhaustive_debug`

- broader offline tuning plan
- larger candidate grid
- still deterministic and screened before full validation

Use when:

- debugging stubborn room-envelope failures
- doing slower offline investigation

## Running It

Fast product default:

```powershell
python scripts/calibrate_room_envelope.py
```

Balanced default:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile balanced_electro_thermal --search-plan balanced_default
```

Tail-guarded room-envelope run:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile electrical_tail_guarded
```

Exhaustive debug:

```powershell
python scripts/calibrate_room_envelope.py --calibration-profile electrical_first --search-plan exhaustive_debug
```

## Artifacts

Primary outputs:

- `build/room_envelope_calibration.json`
- `build/room_envelope_calibration.md`
- `build/room_envelope_parameters.json`
- `build/room_envelope_benchmark.json`
- `build/room_envelope_benchmark.md`

What they mean:

- calibration JSON: full run artifact, metadata, summary object, filtered candidates, diagnostics
- diagnostics markdown: concise per-dataset baseline vs candidate comparison
- parameters JSON: winning candidate metadata and calibrated parameter artifact
- benchmark JSON / markdown: baseline + profile comparison rows for quick inspection, including low-SOC metrics and aging-stage pass/warn/fail counts

## Reading Pass/Warn/Fail Deltas

Per-dataset diagnostics show:

- baseline status
- candidate status
- status transition such as `FAIL->WARN`
- biggest metric improvement
- biggest metric regression
- low-SOC voltage RMSE before vs after
- last-10% voltage RMSE before vs after
- inferred aging stage (`early_life`, `mid_life`, `late_life`)

Aggregate improvement summary shows:

- improved dataset count
- regressed dataset count
- status upgrades
- status downgrades
- most improved dataset
- most worsened dataset

## Current Limits

- The workflow is still built on heuristic per-dataset reduced-order fits plus candidate assembly.
- Weighting now influences anchor ranking and candidate assembly, but it is not yet a full global solver.
- Strong PASS results on the room canonical pack are still limited mainly by end-of-discharge electrical fidelity across aging states.
- Segmented diagnostics identify where the tail fails, but they do not by themselves guarantee a globally optimal low-SOC parameterization.
