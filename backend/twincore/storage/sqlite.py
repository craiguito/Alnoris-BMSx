from __future__ import annotations

import sqlite3
from contextlib import contextmanager
from pathlib import Path
from typing import Iterator


DEFAULT_DB_PATH = Path("data") / "twincore" / "twincore.sqlite"


def connect(db_path: str | Path | None = None) -> sqlite3.Connection:
    path = Path(db_path) if db_path is not None else DEFAULT_DB_PATH
    if path != Path(":memory:"):
        path.parent.mkdir(parents=True, exist_ok=True)
    connection = sqlite3.connect(str(path))
    connection.row_factory = sqlite3.Row
    return connection


def execute_schema(connection: sqlite3.Connection) -> None:
    schema_path = Path(__file__).with_name("schema.sql")
    connection.executescript(schema_path.read_text(encoding="utf-8"))
    connection.commit()


def initialize_database(db_path: str | Path | None = None) -> sqlite3.Connection:
    connection = connect(db_path)
    execute_schema(connection)
    return connection


@contextmanager
def transaction(connection: sqlite3.Connection) -> Iterator[sqlite3.Connection]:
    try:
        connection.execute("BEGIN")
        yield connection
        connection.commit()
    except Exception:
        connection.rollback()
        raise


def row_to_dict(row: sqlite3.Row | None) -> dict[str, object] | None:
    if row is None:
        return None
    return {key: row[key] for key in row.keys()}
