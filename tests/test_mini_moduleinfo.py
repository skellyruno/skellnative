import json
import unittest
from tools.normalize_moduleinfo import PRODUCT_CLASSES, MetadataError, normalize_text


class MiniMetadataTests(unittest.TestCase):
    def test_mini_identity_is_checked_before_rewriting(self):
        name = 'Skell Mini Native DEV'
        ids = sorted(PRODUCT_CLASSES[name])
        doc = {'Name': name, 'Version': '0.1.0', 'Classes': [
            {'CID': ids[0], 'Category': 'Audio Module Class'},
            {'CID': ids[1], 'Category': 'Component Controller Class'}]}
        malformed = json.dumps(doc)[:-1] + ',}'
        self.assertEqual(json.loads(normalize_text(malformed, name)), doc)
        with self.assertRaises(MetadataError):
            normalize_text(malformed)  # Gain remains the default; never infer identity.
        doc['Classes'][0]['CID'] = '0' * 32
        with self.assertRaises(MetadataError):
            normalize_text(json.dumps(doc), name)
