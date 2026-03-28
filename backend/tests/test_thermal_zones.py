from __future__ import annotations

import unittest

from backend.sim_core.bridge import simulation_config_from_dict
from backend.sim_core.engine import run_simulation
from backend.sim_core.types import ThermalZoneConfig
from backend.tests.helpers import make_config


class ThermalZoneTests(unittest.TestCase):
    def test_zone_assignments_parse_correctly(self) -> None:
        payload = {
            "cell_nominal_voltage": 3.6,
            "cell_full_voltage": 4.2,
            "cell_empty_voltage": 3.0,
            "cell_cutoff_voltage": 3.0,
            "cell_capacity_ah": 3.35,
            "cells_in_series": 4,
            "cells_in_parallel": 1,
            "internal_resistance_ohm_per_cell": 0.035,
            "ambient_temp_c": 25.0,
            "discharge_current_a": 5.0,
            "duration_s": 30,
            "time_step_s": 1,
            "initial_soc": 0.9,
            "pack_mass_kg": 1.0,
            "pack_heat_capacity_j_per_kgk": 900.0,
            "cooling_coeff_w_per_k": 1.0,
            "group_count": 4,
            "thermal_zones": [
                {"zone_id": 1, "name": "Cold Plate", "cooling_coeff_multiplier": 1.5},
                {"zone_id": 2, "name": "Hot Corner", "ambient_temp_c": 32.0},
            ],
            "group_zone_assignments": [1, 1, 2, 0],
            "group_labels": ["G1", "G2", "G3", "G4"],
            "group_entity_ids": ["1", "2", "3", "4"],
        }

        config = simulation_config_from_dict(payload)
        self.assertEqual(config.group_zone_assignments, (1, 1, 2, 0))
        self.assertEqual(config.group_labels[2], "G3")
        self.assertEqual(config.group_entity_ids[3], "4")

    def test_invalid_zone_assignment_raises(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            thermal_zones=(ThermalZoneConfig(zone_id=2, name="Remote"),),
            group_zone_assignments=(2, 2, 9, 2),
        )
        with self.assertRaises(ValueError):
            run_simulation(config)

    def test_lower_cooling_zone_runs_hotter(self) -> None:
        config = make_config(
            cells_in_series=4,
            group_count=4,
            duration_s=400,
            discharge_current_a=5.0,
            thermal_zones=(
                ThermalZoneConfig(zone_id=1, name="Low Cooling", cooling_coeff_multiplier=0.25),
            ),
            group_zone_assignments=(0, 0, 1, 1),
        )
        result = run_simulation(config)
        point = result.time_series[-1]
        self.assertGreater(point.group_temp[2], point.group_temp[0])
        self.assertEqual(point.group_zone_ids[2], 1)

    def test_ambient_override_affects_only_assigned_groups(self) -> None:
        config = make_config(
            cells_in_series=2,
            group_count=2,
            duration_s=300,
            discharge_current_a=1.5,
            thermal_zones=(
                ThermalZoneConfig(zone_id=1, name="Warm Bay", ambient_temp_c=40.0),
            ),
            group_zone_assignments=(0, 1),
            group_labels=("Default", "Warm"),
            group_entity_ids=("cell-a", "cell-b"),
        )
        result = run_simulation(config)
        point = result.time_series[-1]
        self.assertGreater(point.group_temp[1], point.group_temp[0])
        self.assertEqual(result.summary.hottest_zone_id, 1)
        self.assertGreaterEqual(result.summary.max_zone_temp_c, point.group_temp[1])


if __name__ == "__main__":
    unittest.main()
