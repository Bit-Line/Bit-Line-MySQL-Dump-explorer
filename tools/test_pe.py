#!/usr/bin/env python3
"""Regression tests for this project's Windows loader metadata; does not run Windows.
Run after tools/build.py. Deliberately mutated PE copies are temporary and never shipped.
"""
from __future__ import annotations
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location('verify_pe', ROOT / 'tools/verify_pe.py')
verify = importlib.util.module_from_spec(spec)
spec.loader.exec_module(verify)

class LoaderMetadataTests(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        path = ROOT / 'build/windows/BitLineDumpBrowser.exe'
        if not path.is_file():
            raise RuntimeError('Build the application with tools/build.py first.')
        cls.original = path.read_bytes()
        cls.pe = struct.unpack_from('<I', cls.original, 0x3c)[0]
        cls.opt = cls.pe + 24

    def inspect_bytes(self, contents):
        with tempfile.TemporaryDirectory(prefix='bitline-pe-test-') as td:
            path = Path(td) / 'test.exe'
            path.write_bytes(contents)
            return verify.inspect(path)

    def changed_u16(self, offset, value):
        data = bytearray(self.original)
        struct.pack_into('<H', data, offset, value)
        return data

    def test_release_metadata(self):
        result = self.inspect_bytes(self.original)
        self.assertEqual(result['minimum_subsystem'], [6, 0])
        self.assertEqual(result['minimum_os_header'], [6, 0])
        self.assertEqual(result['format'], 'PE32+ x64 Windows GUI')
        self.assertFalse(result['executed_on_windows'])
        self.assertEqual(len(result['imports']), 8)

    def test_original_100_loader_configuration_is_rejected(self):
        data = self.changed_u16(self.opt + 48, 10)
        struct.pack_into('<H', data, self.opt + 40, 10)
        with self.assertRaisesRegex(ValueError, '0xC000007B'):
            self.inspect_bytes(data)

    def test_subsystem_63_without_load_config_is_rejected(self):
        data = self.changed_u16(self.opt + 50, 3)
        with self.assertRaisesRegex(ValueError, '0xC000007B'):
            self.inspect_bytes(data)

    def test_unexpected_os_header_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Unexpected loader version'):
            self.inspect_bytes(self.changed_u16(self.opt + 40, 10))

    def test_x86_machine_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Not x64'):
            self.inspect_bytes(self.changed_u16(self.pe + 4, 0x14c))

    def test_pe32_optional_header_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Not PE32\\+'):
            self.inspect_bytes(self.changed_u16(self.opt, 0x10b))

    def test_console_subsystem_is_rejected(self):
        with self.assertRaisesRegex(ValueError, 'Not Windows GUI'):
            self.inspect_bytes(self.changed_u16(self.opt + 68, 3))

    def test_missing_dep_is_rejected(self):
        flags = struct.unpack_from('<H', self.original, self.opt + 70)[0]
        with self.assertRaisesRegex(ValueError, 'DEP'):
            self.inspect_bytes(self.changed_u16(self.opt + 70, flags & ~0x100))

    def test_missing_aslr_is_rejected(self):
        flags = struct.unpack_from('<H', self.original, self.opt + 70)[0]
        with self.assertRaisesRegex(ValueError, 'ASLR'):
            self.inspect_bytes(self.changed_u16(self.opt + 70, flags & ~0x40))

    def test_manifest_application_architecture_mismatch_is_rejected(self):
        data = self.original.replace(b'processorArchitecture="amd64"', b'processorArchitecture="x86  "')
        self.assertNotEqual(data, self.original)
        with self.assertRaisesRegex(ValueError, 'manifest must target amd64'):
            self.inspect_bytes(data)

    def test_manifest_dependency_architecture_mismatch_is_rejected(self):
        data = self.original.replace(b'processorArchitecture="*"', b'processorArchitecture="x"')
        self.assertNotEqual(data, self.original)
        with self.assertRaisesRegex(ValueError, 'dependency architecture mismatch'):
            self.inspect_bytes(data)

    def test_unexpected_runtime_dependency_is_rejected(self):
        data = self.original.replace(b'msvcrt.dll\0', b'evil32.dll\0')
        self.assertNotEqual(data, self.original)
        with self.assertRaisesRegex(ValueError, 'Unexpected runtime dependency'):
            self.inspect_bytes(data)

if __name__ == '__main__':
    unittest.main(verbosity=2)
