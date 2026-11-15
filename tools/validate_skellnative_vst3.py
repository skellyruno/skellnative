"""Run the real Steinberg VST3 validator against the Windows plugin."""
from __future__ import annotations

import argparse
import subprocess
from pathlib import Path

from tools.check_skellnative_vst3 import VerificationError, inspect


def validate(build: Path, timeout: int = 180) -> None:
    root = build.expanduser().resolve()
    bundle, _ = inspect(root)
    matches = [p for p in root.rglob("validator.exe") if p.is_file() and not p.is_symlink()]
    if len(matches) != 1:
        raise VerificationError(f"Expected one Steinberg validator.exe, got {len(matches)}")
    print(f"Steinberg validator: {matches[0]}", flush=True)
    print(f"Native gain bundle: {bundle}", flush=True)
    try:
        result = subprocess.run(
            [str(matches[0]), str(bundle)], cwd=str(root),
            capture_output=True, text=True, encoding="utf-8",
            errors="replace", timeout=timeout, check=False
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise VerificationError(f"Steinberg validation incomplete: {error}") from error
    print(result.stdout[-24000:], flush=True)
    if result.stderr:
        print(result.stderr[-5000:], flush=True)
    if result.returncode != 0:
        raise VerificationError(f"Steinberg validation failed with code {result.returncode}")
    print("PASS: official Steinberg VST3 validator", flush=True)


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    try:
        validate(args.build)
    except (OSError, VerificationError) as error:
        parser.exit(1, f"FAIL: {error}\n")
