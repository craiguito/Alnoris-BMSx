from __future__ import annotations

import unittest

from backend.batterytwin.schemas import BatteryCellTwin, BatteryPackTwin, BatteryScenario, BatterySimulationResult
from backend.batterytwin.solvers.pack_ecm import BatteryPackECMSolverPlugin
from backend.twincore.schemas import RunManifest


def make_simple_scenario() -> BatteryScenario:
    return BatteryScenario(
        pack=BatteryPackTwin(
            cell=BatteryCellTwin(),
            cells_in_series=4,
            cells_in_parallel=1,
            group_count=4,
        ),
        discharge_current_a=1.0,
        duration_s=5,
        time_step_s=1,
        initial_soc=1.0,
    )


class BatteryTwinSolverPluginTests(unittest.TestCase):
    def test_battery_pack_ecm_solver_plugin_runs(self) -> None:
        plugin = BatteryPackECMSolverPlugin()
        package = plugin.run(RunManifest(scenario=make_simple_scenario()))
        artifact = package.first_artifact("battery_simulation_result")

        self.assertEqual(package.solver_id, "BatteryTwin.PackECM")
        self.assertEqual(package.solver_version, "0.1.0")
        self.assertIsNotNone(package.credibility_card)
        self.assertIn("final_soc_avg", package.summary)
        self.assertIsInstance(artifact.payload, BatterySimulationResult)

    def test_result_package_contains_provenance_and_credibility(self) -> None:
        package = BatteryPackECMSolverPlugin().run(RunManifest(scenario=make_simple_scenario()))

        self.assertIsNotNone(package.provenance)
        self.assertEqual(package.provenance.activity_type, "battery_pack_ecm_simulation")
        self.assertEqual(package.credibility_card.model_class, "screening")
        self.assertEqual(package.credibility_card.validation_tier, "regression_tested")
        self.assertEqual(package.credibility_card.uncertainty_class, "engineering_screening")
        self.assertIn("not certification-grade", package.credibility_card.approved_use_range)


if __name__ == "__main__":
    unittest.main()
