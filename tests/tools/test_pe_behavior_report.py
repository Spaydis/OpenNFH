import struct
import unittest


def synthetic_pe() -> bytes:
    data = bytearray(0x400)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3C, 0x80)
    data[0x80:0x84] = b"PE\0\0"
    struct.pack_into("<H", data, 0x86, 1)
    struct.pack_into("<H", data, 0x94, 0xE0)
    optional = 0x98
    struct.pack_into("<H", data, optional, 0x10B)
    struct.pack_into("<I", data, optional + 28, 0x400000)
    section = optional + 0xE0
    data[section:section + 8] = b".text\0\0\0"
    struct.pack_into("<I", data, section + 8, 0x100)
    struct.pack_into("<I", data, section + 12, 0x1000)
    struct.pack_into("<I", data, section + 16, 0x100)
    struct.pack_into("<I", data, section + 20, 0x200)
    struct.pack_into("<I", data, section + 36, 0x60000020)
    data[0x210:0x210 + len("kit/anc".encode("utf-16le"))] = "kit/anc".encode("utf-16le")
    struct.pack_into("<I", data, 0x220, 0x401010)
    return bytes(data)


class PeBehaviorReportTest(unittest.TestCase):
    def test_utf16_string_has_section_address_and_code_xref(self):
        from tools.pe_behavior_report import build_report

        report = build_report({"synthetic.exe": synthetic_pe()}, {"kit/anc"})
        match = report["binaries"][0]["strings"][0]
        self.assertEqual(match["text"], "kit/anc")
        self.assertEqual(match["encoding"], "utf16le")
        self.assertEqual(match["section"], ".text")
        self.assertEqual(match["virtual_address"], 0x401010)
        self.assertEqual(match["xrefs"], [0x220])


if __name__ == "__main__":
    unittest.main()
