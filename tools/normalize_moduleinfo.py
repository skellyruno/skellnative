"""Fix malformed trailing commas in Steinberg-generated VST3 moduleinfo.

Only trailing commas outside JSON strings are removed. All remaining JSON must
parse strictly, and the original gain prototype class IDs must match.
This runs as a VST3 target POST_BUILD step: bad metadata fails the build.
"""
from __future__ import annotations

import argparse
import json
from pathlib import Path

EXPECTED_CLASSES = frozenset({
    "D74FB65BC388486FA8ACC80F23B6DA4E",
    "79BFB11FA5BD4C509602ED4EC4E515D8",
})


class MetadataError(ValueError):
    """Strict module metadata or product identity validation failed."""


def strip_trailing_commas(source: str) -> str:
    result: list[str] = []
    in_string = False
    escaped = False
    for index, character in enumerate(source):
        if in_string:
            result.append(character)
            if escaped:
                escaped = False
            elif character == "\\":
                escaped = True
            elif character == '"':
                in_string = False
            continue
        if character == '"':
            in_string = True
            result.append(character)
            continue
        if character == ',':
            lookahead = index + 1
            while lookahead < len(source) and source[lookahead].isspace():
                lookahead += 1
            if lookahead < len(source) and source[lookahead] in "]}":
                continue
        result.append(character)
    return "".join(result)


def check_product_identity(document: object) -> None:
    if not isinstance(document, dict):
        raise MetadataError("moduleinfo must be a JSON object")
    if document.get("Name") != "SkellNativeGain" or document.get("Version") != "0.1.0":
        raise MetadataError("Unexpected SkellNative gain product name or version")
    classes = document.get("Classes")
    if not isinstance(classes, list) or len(classes) != 2 or not all(isinstance(c, dict) for c in classes):
        raise MetadataError("Gain VST3 must declare processor and controller classes")
    ids = frozenset(str(c.get("CID", "")).upper() for c in classes)
    if ids != EXPECTED_CLASSES:
        raise MetadataError("Processor/controller class IDs do not match the pinned native prototype")
    categories = {c.get("Category") for c in classes}
    if categories != {"Audio Module Class", "Component Controller Class"}:
        raise MetadataError("Expected one audio processor and one component controller")


def normalize_text(source: str) -> str:
    try:
        doc = json.loads(source)
    except json.JSONDecodeError:
        doc = json.loads(strip_trailing_commas(source))
    check_product_identity(doc)
    return json.dumps(doc, indent=2, ensure_ascii=False) + "\n"


def validate_manifest(bundle: Path) -> None:
    path = bundle / "Contents" / "Resources" / "moduleinfo.json"
    doc = json.loads(path.read_text(encoding="utf-8"))
    check_product_identity(doc)


def normalize_build(build: Path) -> Path:
    root = build.expanduser().resolve()
    bundles = [p for p in root.rglob("SkellNativeGain.vst3") if p.is_dir()]
    if len(bundles) != 1:
        raise MetadataError(f"Expected exactly one gain VST3 bundle in {root}; found {len(bundles)}")
    bundle = bundles[0]
    path = bundle / "Contents" / "Resources" / "moduleinfo.json"
    original = path.read_text(encoding="utf-8")
    normalized = normalize_text(original)
    if normalized != original:
        path.write_text(normalized, encoding="utf-8", newline="\n")
    validate_manifest(bundle)
    return path


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("build", type=Path)
    args = parser.parse_args()
    try:
        manifest = normalize_build(args.build)
        print(f"PASS: strict JSON module metadata and preserved native class IDs: {manifest}")
    except (OSError, ValueError) as error:
        parser.exit(1, f"FAIL: moduleinfo normalization or validation: {error}\n")
