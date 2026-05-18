from __future__ import annotations

from .base import VerticalDefinition


def get_fusiontwin_vertical_definition() -> VerticalDefinition:
    return VerticalDefinition(
        vertical_id="fusiontwin",
        display_name="Alnoris FusionTwin",
        description="Planned vertical for fusion energy system digital twins.",
        domain="fusion_energy",
        package_name="backend.fusiontwin",
        template_kinds=(
            "fusion_device_layout",
            "first_wall_configuration",
            "coolant_loop_template",
            "tritium_system_template",
        ),
        component_classes=(
            "vacuum_vessel",
            "first_wall_panel",
            "divertor_tile",
            "blanket_module",
            "magnet_coil",
            "coolant_loop",
            "tritium_system",
            "diagnostic_sensor",
        ),
        scenario_types=(
            "heat_flux_scenario",
            "neutron_load_scenario",
            "tritium_inventory_scenario",
            "coolant_loss_scenario",
            "disruption_load_scenario",
        ),
        solver_ids=(
            "FusionTwin.LoadCore",
            "FusionTwin.ThermalCore",
            "FusionTwin.StressCore",
            "FusionTwin.DamageCore",
            "FusionTwin.TritiumCoreLite",
            "FusionTwin.CoolantNet",
            "FusionTwin.OpenMCAdapter",
        ),
        report_types=("fusion_screening_summary",),
        maturity="planned",
        metadata={
            "implementation_status": "definition_only",
            "notes": "No FusionTwin package or physics implementation exists yet.",
        },
    )


def get_fissiontwin_vertical_definition() -> VerticalDefinition:
    return VerticalDefinition(
        vertical_id="fissiontwin",
        display_name="Alnoris FissionTwin",
        description="Planned vertical for advanced nuclear fission system digital twins.",
        domain="advanced_nuclear_fission",
        package_name="backend.fissiontwin",
        template_kinds=(
            "reactor_core_template",
            "fuel_assembly_template",
            "thermal_hydraulic_channel_template",
            "shielding_template",
        ),
        component_classes=(
            "reactor_vessel",
            "core",
            "fuel_assembly",
            "fuel_pin",
            "control_rod",
            "coolant_channel",
            "heat_exchanger",
            "shielding",
            "sensor",
        ),
        scenario_types=(
            "normal_operation",
            "shutdown_decay_heat",
            "loss_of_flow",
            "power_ramp",
            "fuel_health_scenario",
            "neutronics_run",
        ),
        solver_ids=(
            "FissionTwin.CoreLite",
            "FissionTwin.DecayHeatCore",
            "FissionTwin.ThermalHydraulicsLite",
            "FissionTwin.FuelHealthCore",
            "FissionTwin.OpenMCAdapter",
            "FissionTwin.MOOSEAdapter",
        ),
        report_types=("fission_screening_summary",),
        maturity="planned",
        metadata={
            "implementation_status": "definition_only",
            "notes": "No FissionTwin package or physics implementation exists yet.",
        },
    )


def get_gridtwin_vertical_definition() -> VerticalDefinition:
    return VerticalDefinition(
        vertical_id="gridtwin",
        display_name="Alnoris GridTwin",
        description="Planned vertical for power grid and microgrid digital twins.",
        domain="power_grid_and_microgrid",
        package_name="backend.gridtwin",
        template_kinds=(
            "microgrid_template",
            "feeder_template",
            "substation_template",
            "bess_dispatch_template",
        ),
        component_classes=(
            "substation",
            "transformer",
            "feeder",
            "line",
            "breaker",
            "relay",
            "inverter",
            "load",
            "bess",
            "solar_plant",
            "wind_plant",
            "ev_charging_site",
        ),
        scenario_types=(
            "peak_load",
            "battery_dispatch",
            "outage",
            "islanded_microgrid",
            "renewable_curtailment",
            "data_center_load_growth",
        ),
        solver_ids=(
            "GridTwin.PowerFlowLite",
            "GridTwin.BatteryDispatchCore",
            "GridTwin.AssetHealthCore",
            "GridTwin.OpenDSSAdapter",
            "GridTwin.PandapowerAdapter",
        ),
        report_types=("grid_screening_summary",),
        maturity="planned",
        metadata={
            "implementation_status": "definition_only",
            "notes": "No GridTwin package or power-flow implementation exists yet.",
        },
    )


def get_aerotwin_vertical_definition() -> VerticalDefinition:
    return VerticalDefinition(
        vertical_id="aerotwin",
        display_name="Alnoris AeroTwin",
        description="Planned vertical for aerospace, propulsion, and mission digital twins.",
        domain="aerospace_and_propulsion",
        package_name="backend.aerotwin",
        template_kinds=(
            "aircraft_template",
            "evtol_mission_template",
            "propulsion_system_template",
            "thermal_loop_template",
        ),
        component_classes=(
            "aircraft",
            "airframe",
            "wing",
            "fuselage",
            "propulsion_unit",
            "motor",
            "propeller",
            "battery_system",
            "thermal_loop",
            "actuator",
            "avionics",
            "sensor",
        ),
        scenario_types=(
            "mission_profile",
            "hover_segment",
            "climb",
            "cruise",
            "battery_thermal_stress",
            "propulsion_failure",
            "gust_load",
            "landing_impact",
        ),
        solver_ids=(
            "AeroTwin.MissionCore",
            "AeroTwin.PropulsionCore",
            "AeroTwin.ThermalCore",
            "AeroTwin.LoadCaseCore",
            "AeroTwin.BatteryTwinAdapter",
            "AeroTwin.SU2Adapter",
        ),
        report_types=("aero_screening_summary",),
        maturity="planned",
        metadata={
            "implementation_status": "definition_only",
            "notes": "No AeroTwin package or mission solver implementation exists yet.",
        },
    )


def get_future_vertical_definitions() -> tuple[VerticalDefinition, ...]:
    return (
        get_fusiontwin_vertical_definition(),
        get_fissiontwin_vertical_definition(),
        get_gridtwin_vertical_definition(),
        get_aerotwin_vertical_definition(),
    )
