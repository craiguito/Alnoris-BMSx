from __future__ import annotations

import re
from uuid import uuid4


# TwinCore supports generated opaque ids such as ``alnoris:run:<uuidhex>`` and
# deterministic path ids used by template-generated assets such as
# ``batterytwin:generic_cylindrical_pack:module:0``.
_ID_PATTERN = re.compile(r"^[a-z][a-z0-9_]*(?::[a-z0-9_]+){2,}$")
_KIND_PATTERN = re.compile(r"[^a-z0-9_]+")


def normalize_kind(kind: str) -> str:
    """Normalize a TwinCore entity kind for use in generated ids."""

    normalized = _KIND_PATTERN.sub("_", kind.strip().lower()).strip("_")
    if not normalized:
        raise ValueError("TwinCore id kind must not be empty.")
    if not normalized[0].isalpha():
        normalized = f"t_{normalized}"
    return normalized


def new_id(kind: str, namespace: str = "alnoris") -> str:
    """Create a stable-shape TwinCore id.

    The id is intentionally opaque. Human names and external references belong
    on schema fields, while this value stays safe for graph edges and manifests.
    """

    normalized_namespace = normalize_kind(namespace)
    normalized_kind = normalize_kind(kind)
    return f"{normalized_namespace}:{normalized_kind}:{uuid4().hex}"


def is_valid_id(value: str) -> bool:
    return bool(_ID_PATTERN.fullmatch(value))


def require_valid_id(value: str, field_name: str = "id") -> str:
    if not is_valid_id(value):
        raise ValueError(f"{field_name} must be a TwinCore id with colon-delimited lowercase segments.")
    return value
