from __future__ import annotations

from ..types import PhysicsConfig
from ..chemistry import ChemistryPreset


GENERIC_LIION_PRESET = ChemistryPreset(
    name="generic_liion",
    display_name="Generic Li-ion",
    cell_nominal_voltage=3.6,
    cell_full_voltage=4.2,
    cell_empty_voltage=3.0,
    cell_cutoff_voltage=3.0,
    ocv_curve=(),
    default_physics=PhysicsConfig(),
)
