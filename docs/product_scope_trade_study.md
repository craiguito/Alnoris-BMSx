# Battery Pack Trade Study Cockpit

## What The Product Is

Alnoris BMSx is a fast desktop trade-study tool for battery pack architecture and cooling decisions.

The core wedge is:

1. Choose an archetype.
2. Generate a parametric pack layout.
3. Map CAD-derived thermal zones into the simulation model.
4. Run the flagship virtual tests.
5. Compare candidate results against a baseline.
6. Export reportable outputs using the canonical result schema.

## What The Product Is Not

The product is not:

- a broad battery simulation suite
- a full BMS validation platform
- a multiphysics CAE competitor
- a full manual CAD editor

Non-core simulation workflows, deep per-entity CAD editing, theme customization, balancing-first stories, fault-first stories, and degradation-first stories are not part of the primary product narrative.

## Primary Archetypes

The primary preset catalog now exposes only these three trade-study archetypes:

- `generic_cylindrical_pack`
- `generic_prismatic_pack`
- `high_power_pack`

These are the only archetypes surfaced by the main desktop UI and preset bridge payload.

## Flagship Virtual Tests

The primary virtual test catalog now exposes only these four product-facing workflows:

- `rate_capability`
- `thermal_stress`
- `thermal_zone_comparison`
- `model_validation`

`model_validation` stays in the flagship set because trust, calibration, and validation remain essential to the wedge.

## Primary Workflow

The intended workflow is:

1. Pick an archetype.
2. Adjust pack layout parameters and operating assumptions.
3. Review the generated workspace preview and thermal-zone mapping.
4. Run a flagship virtual test or a baseline simulation.
5. Compare the candidate against the baseline using decision-support metrics.
6. Save the project or export the canonical result payload for reporting.

## Default Results Focus

The default results experience emphasizes:

- pack voltage
- delivered energy and pack power
- max or core temperature
- SOC spread
- weakest and hottest group summaries
- baseline versus candidate comparison

Solver-debug style plots and micro-inspection views are intentionally de-emphasized in the default UI.

## Experimental / Archived Surface Area

The following remain available in archived or experimental form, but are no longer primary product surface area:

- non-primary presets outside the three archetypes
- `constant_current_discharge`
- `constant_current_charge`
- `pulse_power`
- `ocv_relaxation`
- `storage_self_discharge`
- `fault_response`
- `balancing_effectiveness`
- deep per-entity CAD editing flows
- theme customization
- editor-style undo or redo framing as a headline workflow

These capabilities are preserved to protect compatibility, validation work, and technical exploration, but they are no longer the product story.
