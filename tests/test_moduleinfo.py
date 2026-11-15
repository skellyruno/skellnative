"""Regression tests: preserve product identity, reject malformed module info."""
import json
import tempfile
import unittest
from pathlib import Path

from tools.normalize_moduleinfo import (
    EXPECTED_CLASSES, MetadataError, check_product_identity, normalize_text,
    strip_trailing_commas, normalize_build, validate_manifest,
)


class ModuleInfoTests(unittest.TestCase):
    @staticmethod
    def doc():
        return {
            "Name": "SkellNativeGain", "Version": "0.1.0",
            "Classes": [
                {"CID": "D74FB65BC388486FA8ACC80F23B6DA4E", "Category": "Audio Module Class"},
                {"CID": "79BFB11FA5BD4C509602ED4EC4E515D8", "Category": "Component Controller Class"},
            ],
        }

    def test_strict_round_trip(self):
        source = json.dumps(self.doc(), indent=2)
        self.assertEqual(json.loads(normalize_text(source)), self.doc())

    def test_steinberg_trailing_commas(self):
        source = '{"Name":"SkellNativeGain","Version":"0.1.0", "Classes":[{"CID":"D74FB65BC388486FA8ACC80F23B6DA4E","Category":"Audio Module Class",},{"CID":"79BFB11FA5BD4C509602ED4EC4E515D8","Category":"Component Controller Class",},],}'
        self.assertEqual(json.loads(normalize_text(source)), self.doc())

    def test_json_strings_not_modified(self):
        source = '{"x":"a,}\\\"b","other":[1,]}'
        self.assertEqual(json.loads(strip_trailing_commas(source)), {"x":'a,}"b', "other": [1]})

    def test_reject_wrong_class_id(self):
        document = self.doc()
        document["Classes"][0]["CID"] = "DEADBEEF"
        with self.assertRaises(MetadataError):
            normalize_text(json.dumps(document))

    def test_reject_missing_controller(self):
        document = self.doc()
        document["Classes"].pop()
        with self.assertRaises(MetadataError):
            check_product_identity(document)

    def test_reject_invalid_json_other_than_trailing_commas(self):
        with self.assertRaises(json.JSONDecodeError):
            normalize_text('{"Name":"SkellNativeGain","Classes": oops}')

    def test_reject_wrong_name(self):
        document = self.doc()
        document["Name"] = "FakeGain"
        with self.assertRaises(MetadataError):
            normalize_text(json.dumps(document))

    def test_normalized_manifest_on_disk(self):
        with tempfile.TemporaryDirectory() as tmp:
            bundle = Path(tmp) / "SkellNativeGain.vst3"
            resource = bundle / "Contents" / "Resources"
            resource.mkdir(parents=True)
            metadata = resource / "moduleinfo.json"
            metadata.write_text(json.dumps(self.doc())[:-1] + ",}", encoding="utf-8")
            self.assertEqual(normalize_build(Path(tmp)), metadata)
            validate_manifest(bundle)
            self.assertEqual(json.loads(metadata.read_text(encoding="utf-8")), self.doc())

    def test_reject_missing_metadata(self):
        with tempfile.TemporaryDirectory() as tmp:
            (Path(tmp) / "SkellNativeGain.vst3").mkdir()
            with self.assertRaises(FileNotFoundError):
                normalize_build(Path(tmp))


if __name__ == "__main__":
    unittest.main()
