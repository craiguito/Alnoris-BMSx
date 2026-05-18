from __future__ import annotations

from backend.twincore.verticals import VerticalCapability, VerticalDefinition


def get_batterytwin_vertical_definition() -> VerticalDefinition:
    return VerticalDefinition(
        vertical_id="batterytwin",
        display_name="Alnoris BatteryTwin",
        description=(
            "Active reference vertical for battery systems, with BMSx treated as the "
            "BMS/control/SOC/SOH module inside BatteryTwin."
        ),
        domain="battery_systems",
        package_name="backend.batterytwin",
        template_kinds=(
            "battery_pack_preset",
            "battery_module_layout",
            "battery_energy_storage_system_later",
            "evtol_battery_pack_later",
        ),
        component_classes=(
            "battery_pack",
            "battery_module",
            "cell_group",
            "battery_cell",
            "busbar",
            "cooling_channel",
            "enclosure",
            "bms",
        ),
        scenario_types=(
            "BatteryScenario",
            "discharge_profile",
            "charge_profile_later",
            "thermal_stress_later",
            "fault_scenario_later",
            "degradation_scenario_later",
        ),
        solver_ids=("BatteryTwin.PackECM",),
        report_types=("battery_screening_summary",),
        maturity="active_reference_vertical",
        metadata={
            "legacy_solver_backend": "backend.sim_core",
            "bmsx_role": "BMS/control/SOC/SOH module inside BatteryTwin",
            "validation_posture": "screening_not_certification_grade",
        },
    )


def get_batterytwin_capabilities() -> tuple[VerticalCapability, ...]:
    return (
        VerticalCapability(
            capability_id="batterytwin.solver.pack_ecm",
            display_name="BatteryTwin.PackECM",
            capability_type="solver",
            vertical_id="batterytwin",
            status="active",
            metadata={
                "solver_id": "BatteryTwin.PackECM",
                "fidelity": "screening",
                "backend": "backend.sim_core.engine.run_simulation",
            },
        ),
        VerticalCapability(
            capability_id="batterytwin.template.presets",
            display_name="Battery preset template generation",
            capability_type="template",
            vertical_id="batterytwin",
            status="active",
            metadata={
                "template_kind": "battery_pack_preset",
                "module": "backend.batterytwin.templates",
            },
        ),
        VerticalCapability(
            capability_id="batterytwin.service.cli",
            display_name="BatteryTwin CLI",
            capability_type="service",
            vertical_id="batterytwin",
            status="active",
            metadata={"module": "backend.batterytwin.cli"},
        ),
        VerticalCapability(
            capability_id="batterytwin.service.project_summary",
            display_name="BatteryTwin project summary service",
            capability_type="service",
            vertical_id="batterytwin",
            status="active",
            metadata={"module": "backend.batterytwin.services.project_summary"},
        ),
        VerticalCapability(
            capability_id="batterytwin.adapter.legacy_sim_core",
            display_name="Legacy sim_core adapter",
            capability_type="adapter",
            vertical_id="batterytwin",
            status="legacy",
            metadata={"module": "backend.batterytwin.adapters.legacy_sim_config_adapter"},
        ),
        VerticalCapability(
            capability_id="batterytwin.solver.state_core",
            display_name="BatteryTwin.StateCore",
            capability_type="solver",
            vertical_id="batterytwin",
            status="planned",
            metadata={"solver_id": "BatteryTwin.StateCore"},
        ),
        VerticalCapability(
            capability_id="batterytwin.solver.thermal_core",
            display_name="BatteryTwin.ThermalCore",
            capability_type="solver",
            vertical_id="batterytwin",
            status="planned",
            metadata={"solver_id": "BatteryTwin.ThermalCore"},
        ),
        VerticalCapability(
            capability_id="batterytwin.solver.degradation_core",
            display_name="BatteryTwin.DegradationCore",
            capability_type="solver",
            vertical_id="batterytwin",
            status="planned",
            metadata={"solver_id": "BatteryTwin.DegradationCore"},
        ),
        VerticalCapability(
            capability_id="batterytwin.adapter.pybamm",
            display_name="PyBaMM adapter",
            capability_type="adapter",
            vertical_id="batterytwin",
            status="planned",
            metadata={"solver_id": "BatteryTwin.PyBaMMAdapter"},
        ),
    )
