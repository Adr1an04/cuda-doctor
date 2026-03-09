from __future__ import annotations

import argparse
import shutil
import subprocess
from pathlib import Path
from typing import Sequence


def resolve_core_binary() -> Path:
    local = Path(__file__).resolve().parents[1] / ".dist" / "cuda-doctor-core"
    if local.is_file():
        return local

    discovered = shutil.which("cuda-doctor-core")
    if discovered:
        return Path(discovered)

    raise FileNotFoundError("cuda-doctor-core was not found. Build the C++ binary first.")


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(prog="cuda-doctor")
    parser.add_argument("command", choices=["check", "doctor"])
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args(argv)

    binary = resolve_core_binary()
    forwarded = [str(binary), args.command]
    if args.json:
        forwarded.append("--json")

    completed = subprocess.run(forwarded, check=False)
    return completed.returncode


if __name__ == "__main__":
    raise SystemExit(main())
