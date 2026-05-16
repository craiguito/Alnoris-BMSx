from __future__ import annotations

from typing import Any


BATTERY_TYPE_LABELS = {
    "battery_pack": "Pack",
    "battery_module": "Module",
    "cell_group": "Cell Group",
    "cooling_channel": "Cooling",
    "bms": "BMS",
    "enclosure": "Enclosure",
}


def battery_component_display(payload: dict[str, Any]) -> dict[str, Any]:
    asset = payload.get("asset") if isinstance(payload.get("asset"), dict) else {}
    display = payload.get("display") if isinstance(payload.get("display"), dict) else {}
    node_type = str(asset.get("node_type", ""))
    badges = list(display.get("badges", [])) if isinstance(display.get("badges", []), list) else []
    label = BATTERY_TYPE_LABELS.get(node_type)
    if label and label not in badges:
        badges.insert(0, label)
    return {
        **payload,
        "display": {
            **display,
            "badges": badges,
            "battery_type_label": label or node_type.replace("_", " ").title(),
        },
    }
