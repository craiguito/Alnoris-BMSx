from __future__ import annotations

from dataclasses import dataclass, field


VOLT = "V"
AMPERE = "A"
WATT = "W"
WATT_HOUR = "Wh"
AMPERE_HOUR = "Ah"
SECOND = "s"
CELSIUS = "degC"
KILOGRAM = "kg"
OHM = "ohm"


@dataclass(frozen=True)
class UnitValue:
    value: float
    unit: str
    schema_version: str = field(default="twincore.unit_value.v1", init=False)

    def __post_init__(self) -> None:
        if not self.unit:
            raise ValueError("unit must not be empty.")
