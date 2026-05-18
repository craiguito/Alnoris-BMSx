from __future__ import annotations

import io
import json
from contextlib import redirect_stdout

from backend.twincore import cli as twincore_cli
from backend.twincore.verticals import default_vertical_registry


def test_batterytwin_vertical_definition_registered() -> None:
    registry = default_vertical_registry()
    definition = registry.get("batterytwin")

    assert definition is not None
    assert definition.maturity == "active_reference_vertical"
    assert "BatteryTwin.PackECM" in definition.solver_ids
    assert "battery_pack" in definition.component_classes


def test_future_verticals_registered_as_planned() -> None:
    registry = default_vertical_registry()

    for vertical_id in ("fusiontwin", "fissiontwin", "gridtwin", "aerotwin"):
        definition = registry.get(vertical_id)
        assert definition is not None
        assert definition.maturity == "planned"
        assert definition.metadata["implementation_status"] == "definition_only"
        assert definition.package_name.startswith("backend.")


def test_vertical_capabilities() -> None:
    registry = default_vertical_registry()
    capabilities = registry.list_capabilities("batterytwin")
    capability_ids = {capability.capability_id for capability in capabilities}
    active_solvers = {
        capability.metadata.get("solver_id")
        for capability in capabilities
        if capability.capability_type == "solver" and capability.status == "active"
    }

    assert "batterytwin.solver.pack_ecm" in capability_ids
    assert "BatteryTwin.PackECM" in active_solvers
    assert "batterytwin.solver.state_core" in capability_ids
    assert "batterytwin.adapter.pybamm" in capability_ids


def test_twincore_cli_list_verticals() -> None:
    result = twincore_cli.run_command(["list-verticals"])
    vertical_ids = {item["vertical_id"] for item in result["verticals"]}

    assert vertical_ids == {"batterytwin", "fusiontwin", "fissiontwin", "gridtwin", "aerotwin"}


def test_twincore_cli_vertical_detail() -> None:
    fission = twincore_cli.run_command(["vertical", "--vertical-id", "fissiontwin"])
    grid = twincore_cli.run_command(["vertical", "--vertical-id", "gridtwin"])
    aero = twincore_cli.run_command(["vertical", "--vertical-id", "aerotwin"])
    fusion = twincore_cli.run_command(["vertical", "--vertical-id", "fusiontwin"])

    assert "fuel_assembly" in fission["component_classes"]
    assert "FissionTwin.DecayHeatCore" in fission["solver_ids"]
    assert "bess" in grid["component_classes"]
    assert "GridTwin.PowerFlowLite" in grid["solver_ids"]
    assert "propulsion_unit" in aero["component_classes"]
    assert "AeroTwin.MissionCore" in aero["solver_ids"]
    assert "divertor_tile" in fusion["component_classes"]
    assert "FusionTwin.TritiumCoreLite" in fusion["solver_ids"]


def test_twincore_cli_capabilities_json_envelope() -> None:
    buffer = io.StringIO()
    with redirect_stdout(buffer):
        exit_code = twincore_cli.main(["capabilities", "--vertical-id", "batterytwin"])

    payload = json.loads(buffer.getvalue())
    capability_ids = {item["capability_id"] for item in payload["result"]["capabilities"]}

    assert exit_code == 0
    assert payload["ok"] is True
    assert "batterytwin.solver.pack_ecm" in capability_ids
    assert "batterytwin.adapter.pybamm" in capability_ids
