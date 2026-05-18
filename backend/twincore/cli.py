from __future__ import annotations

import argparse
import json
import sys
from typing import Any

from backend.twincore.serialization import dataclass_to_dict
from backend.twincore.verticals import default_vertical_registry


def _parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(prog="python -m backend.twincore.cli")
    subparsers = parser.add_subparsers(dest="command", required=True)

    subparsers.add_parser("list-verticals")

    vertical = subparsers.add_parser("vertical")
    vertical.add_argument("--vertical-id", required=True)

    capabilities = subparsers.add_parser("capabilities")
    capabilities.add_argument("--vertical-id", required=False)

    return parser


def run_command(argv: list[str] | None = None) -> dict[str, Any]:
    args = _parser().parse_args(argv)
    registry = default_vertical_registry()

    if args.command == "list-verticals":
        return {
            "verticals": [
                {
                    "vertical_id": definition.vertical_id,
                    "display_name": definition.display_name,
                    "domain": definition.domain,
                    "package_name": definition.package_name,
                    "maturity": definition.maturity,
                    "solver_ids": list(definition.solver_ids),
                }
                for definition in registry.list_definitions()
            ]
        }
    if args.command == "vertical":
        definition = registry.get(args.vertical_id)
        if definition is None:
            raise ValueError(f"Unknown vertical_id: {args.vertical_id}")
        result = dataclass_to_dict(definition)
        result["capabilities"] = [
            dataclass_to_dict(capability)
            for capability in registry.list_capabilities(args.vertical_id)
        ]
        return result
    if args.command == "capabilities":
        return {
            "capabilities": [
                dataclass_to_dict(capability)
                for capability in registry.list_capabilities(args.vertical_id)
            ]
        }

    raise ValueError(f"Unsupported command: {args.command}")


def main(argv: list[str] | None = None) -> int:
    try:
        result = run_command(argv)
    except Exception as exc:
        json.dump({"ok": False, "error": str(exc)}, sys.stdout)
        sys.stdout.write("\n")
        return 1
    json.dump({"ok": True, "result": result}, sys.stdout)
    sys.stdout.write("\n")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
