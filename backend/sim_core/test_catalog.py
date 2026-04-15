from __future__ import annotations

from .tests_framework import TestParameterDefinition, VirtualTestDefinition


def _param(
    key: str,
    label: str,
    param_type: str,
    default_value: object,
    *,
    required: bool = True,
    min_value: float | None = None,
    max_value: float | None = None,
    unit: str = "",
    tooltip: str = "",
) -> TestParameterDefinition:
    return TestParameterDefinition(
        key=key,
        label=label,
        param_type=param_type,
        default_value=default_value,
        required=required,
        min_value=min_value,
        max_value=max_value,
        unit=unit,
        tooltip=tooltip,
    )


PRIMARY_TEST_CATALOG: tuple[VirtualTestDefinition, ...] = (
    VirtualTestDefinition(
        test_id="rate_capability",
        display_name="Rate Capability",
        description="Compare delivered capacity and energy across multiple discharge rates for fast architecture trade studies.",
        category="Flagship Trade Study",
        difficulty="intermediate",
        recommended_for_pack_level=True,
        parameters=(
            _param(
                "current_list_a",
                "Currents",
                "list_float",
                [2.0, 5.0, 8.0],
                unit="A",
                tooltip="Comma-separated list is accepted by the desktop UI.",
            ),
            _param("initial_soc", "Initial SOC", "float", 1.0, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param("time_step_s", "Time step", "int", 1, min_value=1, unit="s"),
            _param("max_duration_s", "Max duration", "float", 3600.0, min_value=1.0, unit="s"),
        ),
    ),
    VirtualTestDefinition(
        test_id="thermal_stress",
        display_name="Thermal Stress",
        description="Run a sustained load to quantify heating margin, hottest-group behavior, and thermal headroom.",
        category="Flagship Trade Study",
        difficulty="basic",
        recommended_for_pack_level=True,
        supports_thermal_zones=True,
        parameters=(
            _param("current_a", "Load current", "float", 8.0, min_value=0.01, unit="A"),
            _param("duration_s", "Duration", "float", 1800.0, min_value=1.0, unit="s"),
            _param("initial_soc", "Initial SOC", "float", 0.9, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                35.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param("time_step_s", "Time step", "int", 1, min_value=1, unit="s"),
        ),
    ),
    VirtualTestDefinition(
        test_id="thermal_zone_comparison",
        display_name="Thermal Zone Comparison",
        description="Compare zone-to-zone response using CAD-derived thermal mappings or supplied cooling-band assumptions.",
        category="Flagship Trade Study",
        difficulty="advanced",
        recommended_for_pack_level=True,
        supports_thermal_zones=True,
        parameters=(
            _param("current_a", "Load current", "float", 6.0, min_value=0.01, unit="A"),
            _param("duration_s", "Duration", "float", 1800.0, min_value=1.0, unit="s"),
            _param("initial_soc", "Initial SOC", "float", 0.9, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param(
                "zone_cooling_multipliers",
                "Zone cooling multipliers",
                "list_float",
                [0.8, 1.0, 1.2],
                tooltip="Used if the base config has no explicit thermal zones.",
            ),
        ),
    ),
    VirtualTestDefinition(
        test_id="model_validation",
        display_name="Model Validation",
        description="Replay a truth-set current trace and compare simulated voltage, energy, and temperature against trusted data.",
        category="Flagship Trade Study",
        difficulty="advanced",
        recommended_for_pack_level=True,
        supports_profile_input=True,
        parameters=(
            _param("dataset_path", "Truth dataset path", "string", "", tooltip="Path to the truth-set JSON file."),
            _param(
                "metrics",
                "Metrics",
                "json",
                ["rmse_voltage", "energy_error", "temp_rmse"],
                tooltip="JSON list of metric ids: rmse_voltage, energy_error, temp_rmse.",
            ),
            _param("max_voltage_rmse_v", "Max voltage RMSE", "float", 1.0, min_value=0.0, unit="V"),
            _param(
                "max_energy_error_fraction",
                "Max energy error",
                "float",
                0.05,
                min_value=0.0,
                unit="fraction",
            ),
            _param("max_temp_rmse_c", "Max temp RMSE", "float", 2.0, min_value=0.0, unit="C"),
        ),
    ),
)


EXPERIMENTAL_TEST_CATALOG: tuple[VirtualTestDefinition, ...] = (
    VirtualTestDefinition(
        test_id="constant_current_discharge",
        display_name="Constant Current Discharge Test",
        description="Run a constant-current discharge until cutoff or duration limit.",
        category="Experimental / Advanced",
        difficulty="basic",
        recommended_for_pack_level=True,
        parameters=(
            _param("current_a", "Discharge current", "float", 5.0, min_value=0.01, unit="A"),
            _param("initial_soc", "Initial SOC", "float", 1.0, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param("time_step_s", "Time step", "int", 1, min_value=1, unit="s"),
            _param("max_duration_s", "Max duration", "float", 3600.0, min_value=1.0, unit="s"),
        ),
    ),
    VirtualTestDefinition(
        test_id="constant_current_charge",
        display_name="Constant Current Charge Test",
        description="Run a constant-current charge until SOC ceiling or duration limit.",
        category="Experimental / Advanced",
        difficulty="basic",
        recommended_for_pack_level=True,
        parameters=(
            _param("current_a", "Charge current", "float", 3.0, min_value=0.01, unit="A"),
            _param("initial_soc", "Initial SOC", "float", 0.2, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param("time_step_s", "Time step", "int", 1, min_value=1, unit="s"),
            _param("max_duration_s", "Max duration", "float", 3600.0, min_value=1.0, unit="s"),
        ),
    ),
    VirtualTestDefinition(
        test_id="pulse_power",
        display_name="Pulse Power Test",
        description="Apply repeated load pulses with rest periods to examine sag and recovery.",
        category="Experimental / Advanced",
        difficulty="intermediate",
        recommended_for_pack_level=True,
        parameters=(
            _param("pulse_current_a", "Pulse current", "float", 10.0, min_value=0.01, unit="A"),
            _param("pulse_duration_s", "Pulse duration", "float", 10.0, min_value=0.1, unit="s"),
            _param("rest_duration_s", "Rest duration", "float", 20.0, min_value=0.1, unit="s"),
            _param("pulse_count", "Pulse count", "int", 5, min_value=1),
            _param("initial_soc", "Initial SOC", "float", 0.8, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param("time_step_s", "Time step", "int", 1, min_value=1, unit="s"),
        ),
    ),
    VirtualTestDefinition(
        test_id="ocv_relaxation",
        display_name="OCV Relaxation Test",
        description="Apply preload current then rest to observe RC relaxation and recovered voltage.",
        category="Experimental / Advanced",
        difficulty="intermediate",
        recommended_for_pack_level=True,
        parameters=(
            _param("preload_current_a", "Preload current", "float", 6.0, min_value=0.01, unit="A"),
            _param("preload_duration_s", "Preload duration", "float", 30.0, min_value=0.1, unit="s"),
            _param("rest_duration_s", "Rest duration", "float", 300.0, min_value=1.0, unit="s"),
            _param("initial_soc", "Initial SOC", "float", 0.8, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param("time_step_s", "Time step", "int", 1, min_value=1, unit="s"),
        ),
    ),
    VirtualTestDefinition(
        test_id="storage_self_discharge",
        display_name="Storage / Self-Discharge Test",
        description="Simulate long storage with self-discharge and calendar aging.",
        category="Experimental / Advanced",
        difficulty="advanced",
        recommended_for_pack_level=True,
        parameters=(
            _param("storage_duration_s", "Storage duration", "float", 86400.0 * 7.0, min_value=60.0, unit="s"),
            _param("initial_soc", "Initial SOC", "float", 0.8, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                35.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param(
                "self_discharge_per_day",
                "Self-discharge",
                "float",
                0.0005,
                min_value=0.0,
                max_value=0.05,
                unit="fraction/day",
            ),
            _param("time_step_s", "Time step", "int", 3600, min_value=1, unit="s"),
        ),
    ),
    VirtualTestDefinition(
        test_id="fault_response",
        display_name="Fault Response Test",
        description="Run a load case with one or more injected faults and inspect weakest-group behavior.",
        category="Experimental / Advanced",
        difficulty="advanced",
        recommended_for_pack_level=True,
        supports_faults=True,
        parameters=(
            _param("current_a", "Load current", "float", 6.0, min_value=0.01, unit="A"),
            _param("duration_s", "Duration", "float", 1200.0, min_value=1.0, unit="s"),
            _param("initial_soc", "Initial SOC", "float", 0.95, min_value=0.0, max_value=1.0),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param(
                "fault_specs",
                "Fault specs",
                "json",
                [{"fault_type": "high_resistance_group", "group_index": 0, "factor": 2.0}],
                tooltip="JSON list of fault specs.",
            ),
        ),
    ),
    VirtualTestDefinition(
        test_id="balancing_effectiveness",
        display_name="Balancing Effectiveness Test",
        description="Compare SOC spread reduction with passive balancing enabled.",
        category="Experimental / Advanced",
        difficulty="advanced",
        recommended_for_pack_level=True,
        supports_balancing=True,
        parameters=(
            _param("initial_soc", "Center SOC", "float", 0.92, min_value=0.0, max_value=1.0),
            _param("initial_soc_spread", "Initial spread", "float", 0.06, min_value=0.0, max_value=0.5),
            _param("run_duration_s", "Run duration", "float", 1800.0, min_value=1.0, unit="s"),
            _param(
                "ambient_temp_c",
                "Ambient temperature",
                "float",
                25.0,
                min_value=-40.0,
                max_value=120.0,
                unit="C",
            ),
            _param("bleed_current_a", "Bleed current", "float", 0.25, min_value=0.001, unit="A"),
            _param("soc_threshold", "SOC threshold", "float", 0.9, min_value=0.0, max_value=1.0),
        ),
    ),
)

ALL_TEST_CATALOG: tuple[VirtualTestDefinition, ...] = PRIMARY_TEST_CATALOG + EXPERIMENTAL_TEST_CATALOG


def build_experimental_test_catalog() -> tuple[VirtualTestDefinition, ...]:
    return EXPERIMENTAL_TEST_CATALOG


def build_test_catalog(*, include_experimental: bool = False) -> tuple[VirtualTestDefinition, ...]:
    if include_experimental:
        return ALL_TEST_CATALOG
    return PRIMARY_TEST_CATALOG


def get_test_definition(test_id: str) -> VirtualTestDefinition:
    for definition in ALL_TEST_CATALOG:
        if definition.test_id == test_id:
            return definition
    raise ValueError(f"Unknown virtual test id: {test_id}")
