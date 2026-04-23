# Calibration Profiles

`BMSx v1` uses calibration profiles to express objective philosophy, not search breadth.

Profile:

- decides which validation errors matter most
- controls the weighted normalized objective
- influences anchor scoring, weighted family blending, and final candidate ranking

Search plan:

- decides how many candidate structures and blend recipes to try
- controls pruning, screening, and full-validation breadth

## Objective Formula

Candidate quality is scored with:

`objective = sum(weight_i * (avg_metric_i / threshold_i)) / sum(weight_i)`

Where:

- `avg_metric_i` is the average manifest metric value
- `threshold_i` is the active validation threshold for that metric
- lower is better

This keeps electrical and thermal terms on a comparable scale and makes the weighting visible in artifacts.

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
- keep temperature fit from pulling the model away from voltage fidelity

Default search plan: `fast_product_default`

### `balanced_electro_thermal`

- Voltage RMSE weight: `0.35`
- Final voltage error weight: `0.25`
- Energy error weight: `0.20`
- Temperature RMSE weight: `0.20`

Intent:

- still care about voltage first
- allow more compromise to inspect electro-thermal tradeoffs

Default search plan: `balanced_default`

## Where Weighting Matters

Weighting now affects more than final candidate selection.

- Per-dataset baseline scoring: used to rank room-temperature anchor datasets
- Weighted family blending: higher-priority anchors get more influence in combined parameter sets
- Screening and final ranking: candidates are kept or dropped by the same weighted normalized objective

What weighting does not do yet:

- It does not rewrite the underlying reduced-order fitter into a full global optimization solver
- It still operates on top of heuristic per-dataset parameter extraction and candidate assembly
