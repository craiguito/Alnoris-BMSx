from __future__ import annotations

import unittest

from backend.batterytwin.adapters.legacy_sim_config_adapter import battery_scenario_to_legacy_simulation_config
from backend.batterytwin.schemas import BatteryCellTwin, BatteryPackTwin, BatteryScenario, BatterySimulationResult
from backend.batterytwin.solvers.pack_ecm import BatteryPackECMSolverPlugin
from backend.sim_core.types import SimulationConfig
from backend.twincore.ids import is_valid_id
from backend.twincore.schemas import AssetGraph, AssetNode, IdentityRef, ResultPackage, RunManifest


def make_simple_battery_scenario() -> BatteryScenario:
    cell = BatteryCellTwin(
        name="Phase 1 Test Cell",
        cell_nominal_voltage=3.6,
        cell_full_voltage=4.2,
        cell_empty_voltage=3.0,
        cell_cutoff_voltage=3.0,
        cell_capacity_ah=3.35,
        internal_resistance_ohm=0.035,
        mass_kg=0.048,
        heat_capacity_j_per_kgk=900.0,
    )
    pack = BatteryPackTwin(
        name="Phase 1 Pack",
        cell=cell,
        cells_in_series=4,
        cells_in_parallel=1,
        group_count=4,
        cooling_coeff_w_per_k=0.35,
    )
    return BatteryScenario(
        name="Short constant-current discharge",
        pack=pack,
        ambient_temp_c=25.0,
        discharge_current_a=1.0,
        duration_s=5,
        time_step_s=1,
        initial_soc=1.0,
    )


class TwinCoreBatteryTwinPhase1Tests(unittest.TestCase):
    def test_identity_ref_creates_valid_ids(self) -> None:
        identity = IdentityRef(kind="battery_pack", name="Pack A")

        self.assertTrue(is_valid_id(identity.id))
        self.assertIn(":battery_pack:", identity.id)
        self.assertEqual(identity.schema_version, "twincore.identity.v1")

    def test_asset_graph_can_add_pack_module_and_cell_nodes(self) -> None:
        graph = AssetGraph()
        pack = graph.add_node(AssetNode(node_type="battery_pack", label="Pack"))
        module = graph.add_node(AssetNode(node_type="battery_module", label="Module 1"))
        cell = graph.add_node(AssetNode(node_type="battery_cell", label="Cell 1"))

        graph.add_edge(pack.id, module.id)
        graph.add_edge(module.id, cell.id)

        self.assertEqual(graph.node_ids_by_type("battery_pack"), (pack.id,))
        self.assertEqual(graph.children_of(pack.id), (module,))
        self.assertEqual(graph.children_of(module.id), (cell,))

    def test_battery_scenario_converts_to_legacy_simulation_config(self) -> None:
        scenario = make_simple_battery_scenario()

        config = battery_scenario_to_legacy_simulation_config(scenario)

        self.assertIsInstance(config, SimulationConfig)
        self.assertEqual(config.cells_in_series, 4)
        self.assertEqual(config.cells_in_parallel, 1)
        self.assertEqual(config.group_count, 4)
        self.assertEqual(config.duration_s, 5)
        self.assertEqual(config.time_step_s, 1)
        self.assertEqual(config.group_labels, ("Group 0", "Group 1", "Group 2", "Group 3"))

    def test_battery_pack_ecm_solver_plugin_runs_simple_scenario(self) -> None:
        scenario = make_simple_battery_scenario()
        manifest = RunManifest(scenario=scenario)

        package = BatteryPackECMSolverPlugin().run(manifest)
        artifact = package.first_artifact("battery_simulation_result")

        self.assertIsInstance(package, ResultPackage)
        self.assertIsNotNone(artifact)
        self.assertIsInstance(artifact.payload, BatterySimulationResult)
        self.assertEqual(package.run_id, manifest.run_id)
        self.assertEqual(package.solver_id, "BatteryTwin.PackECM")
        self.assertEqual(package.solver_version, "0.1.0")
        self.assertGreater(artifact.payload.pack_nominal_voltage_v, 0.0)
        self.assertGreater(len(artifact.payload.time_series), 1)

    def test_result_package_includes_traceability_and_credibility(self) -> None:
        package = BatteryPackECMSolverPlugin().run(RunManifest(scenario=make_simple_battery_scenario()))

        self.assertTrue(is_valid_id(package.run_id))
        self.assertEqual(package.solver_id, BatteryPackECMSolverPlugin.solver_id)
        self.assertEqual(package.solver_version, BatteryPackECMSolverPlugin.solver_version)
        self.assertTrue(is_valid_id(package.provenance_id))
        self.assertIsNotNone(package.provenance)
        self.assertIsNotNone(package.credibility_card)
        self.assertEqual(package.credibility_card.credibility_level, "screening")


if __name__ == "__main__":
    unittest.main()
