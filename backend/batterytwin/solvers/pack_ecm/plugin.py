from __future__ import annotations

from dataclasses import replace

from backend.batterytwin.adapters.legacy_sim_config_adapter import (
    battery_scenario_to_legacy_simulation_config,
    legacy_simulation_result_to_result_package,
)
from backend.batterytwin.schemas.scenarios import BatteryScenario
from backend.sim_core.engine import run_simulation
from backend.twincore.schemas.scenario import Scenario
from backend.twincore.schemas.simulation import ResultPackage, RunManifest


class BatteryPackECMSolverPlugin:
    solver_id = "BatteryTwin.PackECM"
    solver_version = "0.1.0"
    fidelity = "screening"

    def run(self, manifest: RunManifest) -> ResultPackage:
        scenario = manifest.scenario
        if isinstance(scenario, Scenario) and isinstance(scenario.payload, BatteryScenario):
            scenario = scenario.payload
            manifest = replace(manifest, scenario=scenario)
        if not isinstance(scenario, BatteryScenario):
            raise TypeError("BatteryPackECMSolverPlugin requires a BatteryScenario in RunManifest.scenario.")

        solver_manifest = replace(
            manifest,
            solver_id=self.solver_id,
            metadata={
                **manifest.metadata,
                "solver_version": self.solver_version,
                "fidelity": self.fidelity,
            },
        )
        config = battery_scenario_to_legacy_simulation_config(scenario)
        legacy_result = run_simulation(config)
        return legacy_simulation_result_to_result_package(
            legacy_result,
            solver_manifest,
            solver_id=self.solver_id,
            solver_version=self.solver_version,
            fidelity=self.fidelity,
        )
