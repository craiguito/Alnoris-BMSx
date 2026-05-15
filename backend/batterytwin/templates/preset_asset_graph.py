from __future__ import annotations

from math import ceil

from backend.sim_core.reference_cells import REFERENCE_CELLS
from backend.sim_core.system_presets import BatterySystemPreset, validate_system_preset
from backend.twincore.schemas.asset_graph import AssetGraph, AssetNode
from backend.twincore.schemas.component import ComponentTwin
from backend.twincore.schemas.geometry import GeometryRef
from backend.twincore.schemas.identity import IdentityRef


def _asset_id(preset: BatterySystemPreset, *segments: object) -> str:
    return ":".join(["batterytwin", preset.preset_id, *(str(segment) for segment in segments)])


def _node(preset: BatterySystemPreset, node_type: str, label: str, *segments: object, **metadata: object) -> AssetNode:
    return AssetNode(
        node_type=node_type,
        label=label,
        identity=IdentityRef(kind=node_type, name=label, id=_asset_id(preset, *segments)),
        metadata=dict(metadata),
    )


def _component(
    preset: BatterySystemPreset,
    asset_id: str,
    component_class: str,
    name: str,
    *segments: object,
    component_subclass: str = "",
    **metadata: object,
) -> ComponentTwin:
    return ComponentTwin(
        component_type=component_class,
        identity=IdentityRef(kind="component", name=name, id=_asset_id(preset, "component", *segments)),
        asset_id=asset_id,
        instance_id=_asset_id(preset, "instance", *segments),
        revision_id="rev0",
        component_class=component_class,
        component_subclass=component_subclass,
        name=name,
        metadata=dict(metadata),
    )


def _shared_metadata(preset: BatterySystemPreset) -> dict[str, object]:
    return {
        "preset_id": preset.preset_id,
        "chemistry_name": preset.chemistry_name,
        "series_count": preset.pack.series_count,
        "parallel_count": preset.pack.parallel_count,
        "group_count": preset.pack.series_count,
        "thermal_zone_count": preset.thermal_zone_count,
        "recommended_virtual_tests": list(preset.recommended_virtual_tests),
        "operating_limits": dict(preset.operating_limits),
    }


def preset_to_asset_graph(preset: BatterySystemPreset, project_id: str) -> AssetGraph:
    validate_system_preset(preset)
    graph = AssetGraph(
        identity=IdentityRef(kind="asset_graph", name=f"{preset.display_name} Asset Graph", id=_asset_id(preset, "asset_graph")),
        metadata={
            "project_id": project_id,
            "preset_id": preset.preset_id,
            "representative_cell_policy": "cell_group_nodes_only",
        },
    )
    pack_node = graph.add_node(
        _node(
            preset,
            "battery_pack",
            preset.display_name,
            "pack",
            description=preset.description,
            category=preset.category,
            **_shared_metadata(preset),
        )
    )

    module_nodes: list[AssetNode] = []
    module_count = preset.module.module_count
    groups_per_module = max(1, ceil(preset.pack.series_count / module_count))
    for module_index in range(module_count):
        module_node = graph.add_node(
            _node(
                preset,
                "battery_module",
                f"{preset.display_name} Module {module_index + 1}",
                "module",
                module_index,
                module_index=module_index,
                nominal_group_start=module_index * groups_per_module,
                nominal_group_end=min((module_index + 1) * groups_per_module, preset.pack.series_count) - 1,
                **_shared_metadata(preset),
            )
        )
        graph.add_edge(pack_node.id, module_node.id)
        module_nodes.append(module_node)

    for group_index in range(preset.pack.series_count):
        module_index = min(group_index // groups_per_module, module_count - 1)
        group_node = graph.add_node(
            _node(
                preset,
                "cell_group",
                f"Cell Group {group_index + 1}",
                "group",
                group_index,
                group_index=group_index,
                series_index=group_index,
                cells_in_parallel=preset.pack.parallel_count,
                representative_cell_key=preset.cell.key,
                thermal_zone_id=min(group_index * preset.thermal_zone_count // preset.pack.series_count, preset.thermal_zone_count - 1),
                **_shared_metadata(preset),
            )
        )
        graph.add_edge(module_nodes[module_index].id, group_node.id)

    cooling_node = graph.add_node(
        _node(
            preset,
            "cooling_channel",
            f"{preset.display_name} Cooling",
            "cooling",
            cooling_coeff_w_per_k=preset.cooling_coeff_w_per_k,
            thermal_zone_count=preset.thermal_zone_count,
        )
    )
    enclosure_node = graph.add_node(
        _node(
            preset,
            "enclosure",
            f"{preset.display_name} Enclosure",
            "enclosure",
            wall_thickness_mm=preset.module.enclosure_wall_thickness_mm,
        )
    )
    bms_node = graph.add_node(_node(preset, "bms", f"{preset.display_name} BMS", "bms"))
    graph.add_edge(pack_node.id, cooling_node.id)
    graph.add_edge(pack_node.id, enclosure_node.id)
    graph.add_edge(pack_node.id, bms_node.id)
    return graph


def preset_to_component_twins(preset: BatterySystemPreset, graph: AssetGraph) -> tuple[ComponentTwin, ...]:
    validate_system_preset(preset)
    shared = _shared_metadata(preset)
    components: list[ComponentTwin] = [
        _component(
            preset,
            _asset_id(preset, "pack"),
            "battery_pack",
            preset.display_name,
            "pack",
            component_subclass=preset.category,
            description=preset.description,
            **shared,
        )
    ]
    for module_index in range(preset.module.module_count):
        components.append(
            _component(
                preset,
                _asset_id(preset, "module", module_index),
                "battery_module",
                f"Module {module_index + 1}",
                "module",
                module_index,
                **shared,
            )
        )
    for group_index in range(preset.pack.series_count):
        components.append(
            _component(
                preset,
                _asset_id(preset, "group", group_index),
                "cell_group",
                f"Cell Group {group_index + 1}",
                "group",
                group_index,
                series_index=group_index,
                representative_cell_key=preset.cell.key,
                **shared,
            )
        )
    components.extend(
        (
            _component(
                preset,
                _asset_id(preset, "cooling"),
                "cooling_channel",
                "Cooling Channels",
                "cooling",
                coolant="air",
                cooling_coeff_w_per_k=preset.cooling_coeff_w_per_k,
                **shared,
            ),
            _component(
                preset,
                _asset_id(preset, "bms"),
                "bms",
                "Battery Management System",
                "bms",
                balancing_enabled=preset.balancing_enabled,
                **shared,
            ),
            _component(
                preset,
                _asset_id(preset, "enclosure"),
                "enclosure",
                "Enclosure",
                "enclosure",
                wall_thickness_mm=preset.module.enclosure_wall_thickness_mm,
                **shared,
            ),
        )
    )
    graph_node_ids = set(graph.nodes)
    return tuple(component for component in components if component.asset_id in graph_node_ids)


def preset_to_geometry_refs(preset: BatterySystemPreset, graph: AssetGraph) -> tuple[GeometryRef, ...]:
    validate_system_preset(preset)
    cell = REFERENCE_CELLS[preset.cell.key]
    return (
        GeometryRef(
            geometry_type="generated_parametric_battery_layout",
            uri=f"generated://batterytwin/{preset.preset_id}/layout",
            identity=IdentityRef(kind="geometry", name=f"{preset.display_name} Layout", id=_asset_id(preset, "geometry", "layout")),
            metadata={
                "asset_id": _asset_id(preset, "pack"),
                "preset_id": preset.preset_id,
                "cell_preset_name": cell.name,
                "cell_form_factor": preset.cell.form_factor,
                "cell_radius_mm": preset.cell.radius_mm,
                "cell_height_mm": preset.cell.height_mm,
                "cell_width_mm": preset.cell.width_mm,
                "cell_depth_mm": preset.cell.depth_mm,
                "x_spacing_mm": preset.pack.x_spacing_mm,
                "z_spacing_mm": preset.pack.z_spacing_mm,
                "module_gap_x_mm": preset.module.module_gap_x_mm,
                "module_count": preset.module.module_count,
                "series_count": preset.pack.series_count,
                "parallel_count": preset.pack.parallel_count,
            },
        ),
    )
