from __future__ import annotations

import io
import json
import sqlite3
import subprocess
import sys
import tempfile
import unittest
from contextlib import redirect_stdout
from pathlib import Path
from unittest.mock import patch

from backend.batterytwin import cli as batterytwin_cli
from backend.sim_core.system_presets import BatterySystemPreset, CellPresetRef, ModulePreset, PackPreset


def make_fast_preset() -> BatterySystemPreset:
    return BatterySystemPreset(
        preset_id="generic_cylindrical_pack",
        display_name="Generic Cylindrical Pack",
        description="Fast test preset.",
        category="Tests",
        chemistry_name="generic_liion",
        chemistry_display_name="Generic Li-ion",
        cell=CellPresetRef(key="samsung_30q", form_factor="cylindrical", radius_mm=10.5, height_mm=70.0, width_mm=21.0, depth_mm=21.0),
        module=ModulePreset(module_count=2),
        pack=PackPreset(series_count=4, parallel_count=1, x_spacing_mm=28.0, z_spacing_mm=28.0),
        ambient_temp_c=25.0,
        cooling_coeff_w_per_k=1.0,
        pack_mass_kg=1.0,
        pack_heat_capacity_j_per_kgk=900.0,
        discharge_current_a=1.0,
        duration_s=5,
        time_step_s=1,
        initial_soc=1.0,
        thermal_zone_count=2,
        thermal_zone_cooling_multipliers=(1.0, 1.0),
        recommended_virtual_tests=("constant_current_discharge",),
        operating_limits={"max_discharge_current_a": 2.0},
    )


class BatteryTwinCliTests(unittest.TestCase):
    def test_run_preset_cli_creates_records(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = batterytwin_cli.run_command(
                ["create-project", "--db", db_path, "--name", "CLI Test"]
            )
            with patch("backend.batterytwin.cli.get_system_preset", return_value=make_fast_preset()):
                result = batterytwin_cli.run_command(
                    [
                        "run-preset",
                        "--db",
                        db_path,
                        "--project-id",
                        project["project_id"],
                        "--preset-id",
                        "generic_cylindrical_pack",
                    ]
                )

            connection = sqlite3.connect(db_path)
            counts = {
                table: connection.execute(f"SELECT COUNT(*) FROM {table}").fetchone()[0]
                for table in ("simulation_runs", "provenance_records", "validation_records", "reports", "assets", "components")
            }
            connection.close()

        self.assertTrue(result["run_id"])
        self.assertGreater(counts["assets"], 0)
        self.assertGreater(counts["components"], 0)
        self.assertEqual(counts["simulation_runs"], 1)
        self.assertEqual(counts["provenance_records"], 1)
        self.assertEqual(counts["validation_records"], 1)
        self.assertEqual(counts["reports"], 1)

    def test_new_cli_error_format(self) -> None:
        with tempfile.TemporaryDirectory() as temp_dir:
            db_path = str(Path(temp_dir) / "twincore.sqlite")
            project = batterytwin_cli.run_command(["create-project", "--db", db_path, "--name", "CLI Error Test"])
            stream = io.StringIO()
            with redirect_stdout(stream):
                exit_code = batterytwin_cli.main(
                    [
                        "run-preset",
                        "--db",
                        db_path,
                        "--project-id",
                        project["project_id"],
                        "--preset-id",
                        "does_not_exist",
                    ]
                )

        payload = json.loads(stream.getvalue())
        self.assertEqual(exit_code, 1)
        self.assertFalse(payload["ok"])
        self.assertIn("Unknown system preset", payload["error"])

    def test_legacy_sim_core_cli_still_works(self) -> None:
        payload = {
            "cell_capacity_ah": 3.35,
            "cells_in_series": 1,
            "cells_in_parallel": 1,
            "internal_resistance_ohm_per_cell": 0.035,
            "ambient_temp_c": 25.0,
            "discharge_current_a": 1.0,
            "duration_s": 2,
            "time_step_s": 1,
            "initial_soc": 1.0,
            "pack_mass_kg": 0.048,
            "pack_heat_capacity_j_per_kgk": 900.0,
            "cooling_coeff_w_per_k": 0.35,
            "include_time_series": False,
        }
        completed = subprocess.run(
            [sys.executable, "-m", "backend.sim_core.cli"],
            input=json.dumps(payload),
            text=True,
            capture_output=True,
            check=False,
        )

        self.assertEqual(completed.returncode, 0, completed.stderr)
        self.assertTrue(json.loads(completed.stdout)["ok"])


if __name__ == "__main__":
    unittest.main()
