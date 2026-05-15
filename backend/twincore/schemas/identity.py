from __future__ import annotations

from dataclasses import dataclass, field

from backend.twincore.ids import new_id, require_valid_id


@dataclass(frozen=True)
class IdentityRef:
    kind: str = "identity"
    name: str = ""
    id: str = ""
    external_ids: dict[str, str] = field(default_factory=dict)
    schema_version: str = field(default="twincore.identity.v1", init=False)

    def __post_init__(self) -> None:
        identity_id = self.id or new_id(self.kind)
        require_valid_id(identity_id)
        object.__setattr__(self, "id", identity_id)

    @classmethod
    def create(cls, kind: str, name: str = "", **external_ids: str) -> "IdentityRef":
        return cls(kind=kind, name=name, external_ids=dict(external_ids))
