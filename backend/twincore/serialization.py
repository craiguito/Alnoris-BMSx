from __future__ import annotations

import dataclasses
import hashlib
import json
from datetime import datetime, timezone
from typing import Any


def utc_now_iso() -> str:
    return datetime.now(timezone.utc).replace(microsecond=0).isoformat().replace("+00:00", "Z")


def dataclass_to_dict(obj: Any) -> Any:
    if dataclasses.is_dataclass(obj) and not isinstance(obj, type):
        return {
            field.name: dataclass_to_dict(getattr(obj, field.name))
            for field in dataclasses.fields(obj)
        }
    if isinstance(obj, tuple):
        return [dataclass_to_dict(item) for item in obj]
    if isinstance(obj, list):
        return [dataclass_to_dict(item) for item in obj]
    if isinstance(obj, dict):
        return {str(key): dataclass_to_dict(value) for key, value in obj.items()}
    if obj is None or isinstance(obj, (str, int, float, bool)):
        return obj
    return str(obj)


def stable_json_dumps(value: Any) -> str:
    return json.dumps(dataclass_to_dict(value), sort_keys=True, separators=(",", ":"), ensure_ascii=False)


def dataclass_to_json(obj: Any) -> str:
    return stable_json_dumps(obj)


def json_to_dict(text: str) -> dict[str, Any]:
    value = json.loads(text)
    if not isinstance(value, dict):
        raise ValueError("JSON payload must decode to an object.")
    return value


def stable_hash(value: Any, length: int = 24) -> str:
    digest = hashlib.sha256(stable_json_dumps(value).encode("utf-8")).hexdigest()
    return digest[:length]
