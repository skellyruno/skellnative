"""Strictly verify every experimental native VST3 after CMake POST_BUILD.

Unlike the Steinberg validator, do not tolerate malformed moduleinfo.json.
Also reject symlinks, wrong-arch PE, missing VST3 or wrong class identities.
"""
from __future__ import annotations

import argparse
from pathlib import Path
from tools.check_skellnative_vst3 import VerificationError, check_pe_x64
from tools.normalize_moduleinfo import validate_manifest, PRODUCT_CLASSES


PILOTS = ("SkellNativeGain", "Skell Mini Native DEV", "Skell Spacer Native DEV")


def verify(directory: Path) -> None:
    root = directory.resolve(strict=True)
    if not root.is_dir():
        raise VerificationError("VST3 build output must be a directory")
    for product in PILOTS:
        bundle = root / (product + ".vst3")
        if bundle.is_symlink() or not bundle.is_dir():
            raise VerificationError(f"Expected native bundle: {bundle}")
        if any(p.is_symlink() for p in bundle.rglob("*")):
            raise VerificationError(f"Linked content inside bundle: {bundle}")
        # Strict JSON parse plus exactly pinned processor/controller UUIDs.
        validate_manifest(bundle, product)
        binaries = [p for p in bundle.rglob("*.vst3") if p.is_file()]
        if len(binaries) != 1:
            raise VerificationError(f"Expected exactly one VST3 DLL in {bundle}, got {len(binaries)}")
        check_pe_x64(binaries[0])
        print(f"PASS: strict metadata, {len(PRODUCT_CLASSES[product])} class IDs and AMD64 PE32+: {bundle}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    try:
        verify(args.build)
    except (OSError, ValueError, VerificationError) as error:
        parser.exit(1, f"FAIL: native VST3 bundle contract: {error}\n")
