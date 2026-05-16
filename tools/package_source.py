from __future__ import annotations

import argparse
import fnmatch
import os
import zipfile
from pathlib import Path


DEFAULT_OUT = Path("dist/Alnoris_BatteryTwin_source_clean.zip")
INCLUDE_ROOTS = ("backend", "docs", "examples")
INCLUDE_FILES = ("README.md", "pyproject.toml", ".gitignore")
EXCLUDED_DIRS = {
    ".git",
    ".pytest_cache",
    ".mypy_cache",
    ".ruff_cache",
    "__pycache__",
    ".venv",
    "venv",
    "archive",
    "data",
    "Data",
    "build",
    "dist",
    "build-desktop",
    "build-desktop-test-artifacts",
    "node_modules",
    "_tmp",
}
EXCLUDED_PATTERNS = (
    "*.pyc",
    "*.pyo",
    "*.pyd",
    "*.sqlite",
    "*.sqlite-wal",
    "*.sqlite-shm",
    "*.log",
    "*.tmp",
    "*.temp",
    "*.zip",
    "*.tar",
    "*.gz",
    "*.rar",
)


def _is_excluded(path: Path) -> bool:
    parts = set(path.parts)
    if parts & EXCLUDED_DIRS:
        return True
    if any(part.startswith("cmake-build-") for part in path.parts):
        return True
    return any(fnmatch.fnmatch(path.name, pattern) for pattern in EXCLUDED_PATTERNS)


def collect_files(repo_root: Path) -> tuple[list[Path], int]:
    included: list[Path] = []
    excluded_count = 0
    candidates: list[Path] = []
    for root_name in INCLUDE_ROOTS:
        root = repo_root / root_name
        if root.exists():
            for current_root, dir_names, file_names in os.walk(root):
                current = Path(current_root)
                relative_current = current.relative_to(repo_root)
                kept_dirs = []
                for dir_name in dir_names:
                    relative_dir = relative_current / dir_name
                    if _is_excluded(relative_dir):
                        excluded_count += 1
                    else:
                        kept_dirs.append(dir_name)
                dir_names[:] = kept_dirs
                candidates.extend(current / file_name for file_name in file_names)
    for file_name in INCLUDE_FILES:
        path = repo_root / file_name
        if path.exists() and path.is_file():
            candidates.append(path)

    seen: set[Path] = set()
    for path in sorted(candidates):
        relative = path.relative_to(repo_root)
        if relative in seen:
            continue
        seen.add(relative)
        if _is_excluded(relative):
            excluded_count += 1
            continue
        included.append(path)
    return included, excluded_count


def main() -> int:
    parser = argparse.ArgumentParser(description="Create a clean Alnoris BatteryTwin source package.")
    parser.add_argument("--dry-run", action="store_true", help="Print included/excluded counts without writing a zip.")
    parser.add_argument("--out", default=str(DEFAULT_OUT), help="Output zip path.")
    args = parser.parse_args()

    repo_root = Path(__file__).resolve().parents[1]
    included, excluded_count = collect_files(repo_root)
    output_path = (repo_root / args.out).resolve()

    if args.dry_run:
        print(
            {
                "dry_run": True,
                "included_count": len(included),
                "excluded_count": excluded_count,
                "output": str(output_path.relative_to(repo_root)),
            }
        )
        return 0

    output_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(output_path, "w", compression=zipfile.ZIP_DEFLATED) as archive:
        for path in included:
            archive.write(path, path.relative_to(repo_root).as_posix())
    print(
        {
            "dry_run": False,
            "included_count": len(included),
            "excluded_count": excluded_count,
            "output": str(output_path.relative_to(repo_root)),
        }
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
