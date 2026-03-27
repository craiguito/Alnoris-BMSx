from __future__ import annotations

import json
import sys

from .bridge import run_simulation_from_dict


def main() -> int:
    try:
        payload = json.load(sys.stdin)
        result = run_simulation_from_dict(payload)
    except Exception as exc:
        json.dump({"ok": False, "error": str(exc)}, sys.stdout)
        sys.stdout.write("\n")
        return 1

    json.dump({"ok": True, "result": result}, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
