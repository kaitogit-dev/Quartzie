import io
import json
import struct
import tempfile
import unittest
import zipfile
from pathlib import Path
import sys

sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "tools"))
import qapp

def win32_exe(machine=0x8664):
    data = bytearray(512)
    data[:2] = b"MZ"
    struct.pack_into("<I", data, 0x3c, 128)
    data[128:132] = b"PE\x00\x00"
    struct.pack_into("<HH", data, 132, machine, 1)
    struct.pack_into("<H", data, 148, 224)
    struct.pack_into("<H", data, 152, 0x20b if machine == 0x8664 else 0x10b)
    return bytes(data)

def android_apk(manifest=True, traversal=False):
    stream = io.BytesIO()
    with zipfile.ZipFile(stream, "w") as z:
        if manifest:
            z.writestr("AndroidManifest.xml", b"\x03\x00\x08\x00")
        z.writestr("classes.dex", b"dex\n035\x00")
        if traversal:
            z.writestr("../evil.txt", "evil")
    return stream.getvalue()

class TestQuartziePackages(unittest.TestCase):
    def setUp(self):
        self.directory = tempfile.TemporaryDirectory()
        self.addCleanup(self.directory.cleanup)
        self.folder = Path(self.directory.name)

    def build(self, suffix, data):
        source = self.folder / ("application" + suffix)
        source.write_bytes(data)
        target = self.folder / "application.qapp"
        meta = qapp.pack(source, target, "Sample App", "dev.quartzie.sample", "1.2.3")
        return target, meta

    def test_exe_conversion_and_extraction(self):
        data = win32_exe()
        target, manifest = self.build(".exe", data)
        self.assertEqual(manifest["platform"], "windows")
        self.assertEqual(manifest["architecture"], "x86_64")
        self.assertEqual(qapp.verify(target), manifest)
        output = qapp.extract(target, self.folder / "installed")
        self.assertEqual(output.read_bytes(), data)
        with self.assertRaises(FileExistsError):
            qapp.extract(target, self.folder / "installed")

    def test_android_conversion_and_extraction(self):
        data = android_apk()
        target, manifest = self.build(".apk", data)
        self.assertEqual(manifest["platform"], "android")
        self.assertEqual(manifest["architecture"], "android")
        self.assertEqual(qapp.verify(target), manifest)
        output = qapp.extract(target, self.folder / "installed")
        self.assertEqual(output.read_bytes(), data)

    def test_invalid_executable_rejected(self):
        with self.assertRaises(qapp.PackageError):
            self.build(".exe", b"MZ" + bytes(256))
        with self.assertRaises(qapp.PackageError):
            self.build(".exe", win32_exe(machine=0x100))

    def test_invalid_apk_rejected(self):
        with self.assertRaises(qapp.PackageError):
            self.build(".apk", android_apk(manifest=False))
        with self.assertRaises(qapp.PackageError):
            self.build(".apk", android_apk(traversal=True))

    def test_payload_tampering_rejected(self):
        target, _ = self.build(".exe", win32_exe())
        with zipfile.ZipFile(target, "r") as source:
            original = {name: source.read(name) for name in source.namelist()}
        edited = bytearray(original["payload/application.exe"])
        edited[400] ^= 15
        original["payload/application.exe"] = bytes(edited)
        with zipfile.ZipFile(target, "w") as output:
            for name, data in original.items():
                output.writestr(qapp.zipentry(name), data)
        with self.assertRaises(qapp.PackageError):
            qapp.verify(target)

    def test_duplicate_metadata_rejected(self):
        target, _ = self.build(".exe", win32_exe())
        with zipfile.ZipFile(target, "a") as output:
            output.writestr("manifest.json", "{}")
        with self.assertRaises(qapp.PackageError):
            qapp.verify(target)

    def test_metadata_validation(self):
        with self.assertRaises(qapp.PackageError):
            self.build(".unknown", b"garbage")
        for app_id in ("../bad", "single", "Bad.Package", "app..name"):
            with self.assertRaises(qapp.PackageError):
                qapp.valid_metadata("Test", app_id, "1.0")
        for version in ("-1", "latest", "1.2 3"):
            with self.assertRaises(qapp.PackageError):
                qapp.valid_metadata("Test", "dev.quartzie.test", version)

    def test_reproducible_archives(self):
        source = self.folder / "foo.exe"
        source.write_bytes(win32_exe())
        a = self.folder / "a.qapp"
        b = self.folder / "b.qapp"
        for out in (a, b):
            qapp.pack(source, out, "Foo", "dev.quartzie.foo", "2.0")
        self.assertEqual(a.read_bytes(), b.read_bytes())

if __name__ == "__main__":
    unittest.main()
