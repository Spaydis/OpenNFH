import unittest


class IdentifierAuditTest(unittest.TestCase):
    def test_utf16le_identifier_offsets_are_reported(self):
        from tools.identifier_audit import scan_utf16le_strings

        blob = b"x" + "kit/anc".encode("utf-16le") + b"\x00\x00"
        self.assertEqual(scan_utf16le_strings(blob, "kit/anc"), [1])

    def test_utf16le_xml_declaration_is_removed_before_fragment_wrap(self):
        import tempfile
        import zipfile
        from pathlib import Path

        from tools.identifier_audit import _iter_xml_contexts

        with tempfile.TemporaryDirectory() as temporary:
            data_root = Path(temporary)
            with zipfile.ZipFile(data_root / "gamedata.bnd", "w") as archive:
                payload = b"\xff\xfe" + '<?xml version="1.0"?>\n<object name="kit/anc"/>'.encode("utf-16le")
                archive.writestr("test.xml", payload)
            _, contexts, warnings = _iter_xml_contexts(data_root, {"kit/anc"})

        self.assertEqual(warnings, [])
        self.assertEqual(len(contexts["kit/anc"]), 1)

    def test_legacy_single_byte_xml_is_decoded_for_contexts(self):
        import tempfile
        import zipfile
        from pathlib import Path

        from tools.identifier_audit import _iter_xml_contexts

        with tempfile.TemporaryDirectory() as temporary:
            data_root = Path(temporary)
            with zipfile.ZipFile(data_root / "gamedata.bnd", "w") as archive:
                archive.writestr("test.xml", b'<object name="kit/anc" note="neighbor\xb4s"/>')
            _, contexts, warnings = _iter_xml_contexts(data_root, {"kit/anc"})

        self.assertEqual(warnings, [])
        self.assertEqual(len(contexts["kit/anc"]), 1)

if __name__ == "__main__":
    unittest.main()
