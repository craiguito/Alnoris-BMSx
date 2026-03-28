from __future__ import annotations

import unittest

from backend.sim_core.bridge import run_simulation_from_dict, run_virtual_test_from_dict
from backend.tests.helpers import make_config


class ResultSchemaTests(unittest.TestCase):
    def _payload(self) -> dict[str, object]:
        config = make_config(cells_in_series=4, group_count=4)
        return {
            "cell_nominal_voltage": config.cell_nominal_voltage,
            "cell_full_voltage": config.cell_full_voltage,
            "cell_empty_voltage": config.cell_empty_voltage,
            "cell_cutoff_voltage": config.cell_cutoff_voltage,
            "cell_capacity_ah": config.cell_capacity_ah,
            "cells_in_series": config.cells_in_series,
            "cells_in_parallel": config.cells_in_parallel,
            "internal_resistance_ohm_per_cell": config.internal_resistance_ohm_per_cell,
            "ambient_temp_c": config.ambient_temp_c,
            "discharge_current_a": config.discharge_current_a,
            "duration_s": config.duration_s,
            "time_step_s": config.time_step_s,
            "initial_soc": config.initial_soc,
            "pack_mass_kg": config.pack_mass_kg,
            "pack_heat_capacity_j_per_kgk": config.pack_heat_capacity_j_per_kgk,
            "cooling_coeff_w_per_k": config.cooling_coeff_w_per_k,
            "group_count": config.group_count,
        }

    def test_simulation_serialization_uses_canonical_fields(self) -> None:
        result = run_simulation_from_dict(self._payload())
        point = result["time_series"][-1]
        summary = result["summary"]

        self.assertIn("pack_voltage_v", point)
        self.assertIn("pack_temp_max_c", point)
        self.assertIn("group_voltage_v", point)
        self.assertIn("group_core_temp_c", point)
        self.assertIn("group_surface_temp_c", point)
        self.assertIn("group_effective_resistance_ohm", point)
        self.assertIn("chemistry_name", summary)
        self.assertIn("max_core_temp_c", summary)
        self.assertIn("nonlinear_features_enabled", summary)

    def test_legacy_aliases_remain_available(self) -> None:
        result = run_simulation_from_dict(self._payload())
        point = result["time_series"][-1]
        summary = result["summary"]

        self.assertIn("terminal_voltage_v", point)
        self.assertIn("group_temp", point)
        self.assertIn("peak_temp_c", summary)
        self.assertIn("min_terminal_voltage_v", summary)

    def test_virtual_test_result_remains_serializable(self) -> None:
        payload = {
            "test_id": "constant_current_discharge",
            "base_config": self._payload(),
            "parameters": {
                "current_a": 2.0,
                "initial_soc": 1.0,
                "ambient_temp_c": 25.0,
                "time_step_s": 1.0,
                "max_duration_s": 120.0,
            },
        }
        result = run_virtual_test_from_dict(payload)
        self.assertIn("primary_result", result)
        self.assertIn("summary", result["primary_result"])
        self.assertIn("time_series", result["primary_result"])


if __name__ == "__main__":
    unittest.main()
