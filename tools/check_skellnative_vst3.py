"""Strict, independent Windows x64 VST3 bundle verifier."""
from __future__ import annotations

import argparse
import struct
from pathlib import Path

from tools.normalize_moduleinfo import validate_manifest


class VerificationError(RuntimeError):
    """A native VST3 artifact did not satisfy mandatory integrity checks."""


def check_pe_x64(path: Path) -> None:
    data = path.read_bytes()
    if len(data) < 0x100 or data[:2] != b"MZ":
        raise VerificationError(f"Not a Windows PE executable: {path}")
    offset = struct.unpack_from("<I", data, 0x3C)[0]
    if offset < 0x40 or offset + 26 > len(data) or data[offset:offset + 4] != b"PE\x00\x00":
        raise VerificationError(f"Malformed PE header: {path}")
    machine = struct.unpack_from("<H", data, offset + 4)[0]
    magic = struct.unpack_from("<H", data, offset + 24)[0]
    if (machine, magic) != (0x8664, 0x20B):
        raise VerificationError(f"Expected AMD64 PE32+, got machine={machine:#x}, format={magic:#x}")


def inspect(build: Path) -> tuple[Path, Path]:
    root = build.expanduser().resolve()
    if not root.is_dir():
        raise VerificationError(f"Missing CMake build directory: {root}")
    bundles = [p for p in root.rglob("SkellNativeGain.vst3") if p.is_dir()]
    if len(bundles) != 1:
        raise VerificationError(f"Expected exactly one SkellNativeGain.vst3 bundle, got {len(bundles)}")
    bundle = bundles[0]
    if bundle.is_symlink() or any(p.is_symlink() for p in bundle.rglob("*")):
        raise VerificationError("Native VST3 bundle contains a symlink")
    binaries = [p for p in bundle.rglob("*.vst3") if p.is_file()]
    if len(binaries) != 1:
        raise VerificationError(f"Expected exactly one VST3 PE binary, got {len(binaries)}")
    check_pe_x64(binaries[0])
    validate_manifest(bundle)
    return bundle, binaries[0]


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    try:
        bundle, binary = inspect(args.build)
        print(f"PASS: VST3 bundle {bundle}")
        print(f"PASS: AMD64 PE32+ binary {binary}")
    except (OSError, VerificationError) as error:
        parser.exit(1, f"FAIL: {error}\n")
