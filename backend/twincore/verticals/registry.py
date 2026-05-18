from __future__ import annotations

from .base import VerticalCapability, VerticalDefinition
from .future import get_future_vertical_definitions


class VerticalRegistry:
    def __init__(self) -> None:
        self._definitions: dict[str, VerticalDefinition] = {}
        self._capabilities: dict[str, VerticalCapability] = {}

    def register(self, definition: VerticalDefinition) -> None:
        self._definitions[definition.vertical_id] = definition

    def get(self, vertical_id: str) -> VerticalDefinition | None:
        return self._definitions.get(vertical_id)

    def list_definitions(self) -> tuple[VerticalDefinition, ...]:
        return tuple(self._definitions.values())

    def register_capability(self, capability: VerticalCapability) -> None:
        if capability.vertical_id not in self._definitions:
            raise ValueError(f"Cannot register capability for unknown vertical: {capability.vertical_id}")
        self._capabilities[capability.capability_id] = capability

    def list_capabilities(self, vertical_id: str | None = None) -> tuple[VerticalCapability, ...]:
        capabilities = tuple(self._capabilities.values())
        if vertical_id is None:
            return capabilities
        return tuple(capability for capability in capabilities if capability.vertical_id == vertical_id)


def default_vertical_registry() -> VerticalRegistry:
    from backend.batterytwin.vertical import (
        get_batterytwin_capabilities,
        get_batterytwin_vertical_definition,
    )

    registry = VerticalRegistry()
    registry.register(get_batterytwin_vertical_definition())
    for definition in get_future_vertical_definitions():
        registry.register(definition)
    for capability in get_batterytwin_capabilities():
        registry.register_capability(capability)
    return registry
