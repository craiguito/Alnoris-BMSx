from __future__ import annotations

from dataclasses import dataclass, field
from typing import Any

from backend.twincore.schemas.identity import IdentityRef


@dataclass(frozen=True)
class BatteryCellTwin:
    name: str = "Generic Li-ion Cell"
    chemistry_name: str = "generic_liion"
    cell_nominal_voltage: float = 3.6
    cell_full_voltage: float = 4.2
    cell_empty_voltage: float = 3.0
    cell_cutoff_voltage: float = 3.0
    cell_capacity_ah: float = 3.35
    internal_resistance_ohm: float = 0.035
    mass_kg: float = 0.048
    heat_capacity_j_per_kgk: float = 900.0
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="battery_cell"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.battery_cell.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class CellGroupTwin:
    group_index: int = 0
    label: str = ""
    cells_in_series: int = 1
    cells_in_parallel: int = 1
    thermal_zone_id: int = 0
    cell: BatteryCellTwin | None = None
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="cell_group"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.cell_group.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id

    @property
    def resolved_label(self) -> str:
        return self.label or f"Group {self.group_index}"


@dataclass(frozen=True)
class BatteryModuleTwin:
    name: str = "Battery Module"
    cells_in_series: int = 1
    cells_in_parallel: int = 1
    cell_groups: tuple[CellGroupTwin, ...] = ()
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="battery_module"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.battery_module.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class BusbarTwin:
    name: str = "Busbar"
    material_id: str = ""
    resistance_ohm: float | None = None
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="busbar"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.busbar.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class CoolingChannelTwin:
    name: str = "Cooling Channel"
    coolant: str = "air"
    cooling_coeff_w_per_k: float | None = None
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="cooling_channel"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.cooling_channel.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class EnclosureTwin:
    name: str = "Enclosure"
    material_id: str = ""
    mass_kg: float = 0.0
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="enclosure"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.enclosure.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id


@dataclass(frozen=True)
class BatteryPackTwin:
    name: str = "Battery Pack"
    cell: BatteryCellTwin = field(default_factory=BatteryCellTwin)
    cells_in_series: int = 1
    cells_in_parallel: int = 1
    group_count: int | None = None
    pack_mass_kg: float | None = None
    pack_heat_capacity_j_per_kgk: float | None = None
    cooling_coeff_w_per_k: float = 0.35
    modules: tuple[BatteryModuleTwin, ...] = ()
    cell_groups: tuple[CellGroupTwin, ...] = ()
    busbars: tuple[BusbarTwin, ...] = ()
    cooling_channels: tuple[CoolingChannelTwin, ...] = ()
    enclosure: EnclosureTwin | None = None
    identity: IdentityRef = field(default_factory=lambda: IdentityRef(kind="battery_pack"))
    metadata: dict[str, Any] = field(default_factory=dict)
    schema_version: str = field(default="batterytwin.battery_pack.v1", init=False)

    @property
    def id(self) -> str:
        return self.identity.id

    def resolved_group_count(self) -> int:
        if self.group_count is not None:
            return self.group_count
        if self.cell_groups:
            return len(self.cell_groups)
        module_group_count = sum(len(module.cell_groups) for module in self.modules)
        if module_group_count:
            return module_group_count
        return self.cells_in_series

    def resolved_pack_mass_kg(self) -> float:
        if self.pack_mass_kg is not None:
            return self.pack_mass_kg
        enclosure_mass = self.enclosure.mass_kg if self.enclosure is not None else 0.0
        return self.cell.mass_kg * self.cells_in_series * self.cells_in_parallel + enclosure_mass

    def resolved_pack_heat_capacity_j_per_kgk(self) -> float:
        return self.pack_heat_capacity_j_per_kgk or self.cell.heat_capacity_j_per_kgk

    def resolved_group_labels(self) -> tuple[str, ...]:
        groups = self.cell_groups or tuple(group for module in self.modules for group in module.cell_groups)
        group_count = self.resolved_group_count()
        if len(groups) == group_count:
            return tuple(group.resolved_label for group in groups)
        return tuple(f"Group {index}" for index in range(group_count))

    def resolved_group_entity_ids(self) -> tuple[str, ...]:
        groups = self.cell_groups or tuple(group for module in self.modules for group in module.cell_groups)
        group_count = self.resolved_group_count()
        if len(groups) == group_count:
            return tuple(group.id for group in groups)
        return tuple(f"{self.id}:group:{index}" for index in range(group_count))

    def resolved_group_zone_assignments(self) -> tuple[int, ...]:
        groups = self.cell_groups or tuple(group for module in self.modules for group in module.cell_groups)
        group_count = self.resolved_group_count()
        if len(groups) == group_count:
            return tuple(group.thermal_zone_id for group in groups)
        return tuple(0 for _ in range(group_count))
