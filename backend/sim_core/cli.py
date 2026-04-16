from __future__ import annotations

import json
import sys

from .bridge import (
    battery_system_preset_catalog_to_bridge_dict,
    run_simulation_from_dict,
    run_virtual_test_from_dict,
    truth_dataset_catalog_to_dict,
    vet_virtual_test_from_dict,
    virtual_test_catalog_to_dict,
)


def main() -> int:
    try:
        mode = sys.argv[1] if len(sys.argv) > 1 else "simulate"
        if mode == "list-tests":
            result = virtual_test_catalog_to_dict()
        elif mode == "list-presets":
            result = battery_system_preset_catalog_to_bridge_dict()
        elif mode == "list-truth-datasets":
            result = truth_dataset_catalog_to_dict()
        else:
            payload = json.load(sys.stdin)
            if mode == "simulate":
                result = run_simulation_from_dict(payload)
            elif mode == "vet-test":
                result = vet_virtual_test_from_dict(payload)
            elif mode == "run-test":
                result = run_virtual_test_from_dict(payload)
            else:
                raise ValueError(f"Unsupported CLI mode: {mode}")
    except Exception as exc:
        json.dump({"ok": False, "error": str(exc)}, sys.stdout)
        sys.stdout.write("\n")
        return 1

    json.dump({"ok": True, "result": result}, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
