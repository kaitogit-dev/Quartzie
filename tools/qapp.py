import argparse
import hashlib
import io
import json
import os
import re
import struct
import tempfile
import zipfile
from pathlib import Path

MAX_PAYLOAD = 512 * 1024 * 1024
MAX_ICON = 2 * 1024 * 1024
SCHEMA = "quartzie.app.package"
ENTRY_PATHS = {"windows": "payload/application.exe", "android": "payload/application.apk"}

class PackageError(ValueError):
    pass

def valid_metadata(name, app_id, version):
    if not isinstance(name, str) or not 1 <= len(name) <= 80 or any(ord(c) < 32 for c in name):
        raise PackageError("Invalid application name")
    if not isinstance(app_id, str) or not re.fullmatch(r"[a-z][a-z0-9_]*(?:\.[a-z][a-z0-9_]*){1,12}", app_id):
        raise PackageError("Invalid application ID")
    if not isinstance(version, str) or not re.fullmatch(r"[0-9]+(?:\.[0-9]+){0,3}(?:-[A-Za-z0-9._-]+)?", version) or len(version) > 64:
        raise PackageError("Invalid application version")

def validate_exe(data):
    if len(data) < 0x80 or data[:2] != b"MZ":
        raise PackageError("Not a Windows executable")
    offset = struct.unpack_from("<I", data, 0x3c)[0]
    if offset > len(data) - 26 or data[offset:offset + 4] != b"PE\x00\x00":
        raise PackageError("Invalid PE header")
    machine, sections = struct.unpack_from("<HH", data, offset + 4)
    optional_size = struct.unpack_from("<H", data, offset + 20)[0]
    if machine not in (0x14c, 0x8664) or not 1 <= sections <= 96:
        raise PackageError("Unsupported executable architecture")
    if optional_size < 2 or offset + 24 + optional_size > len(data):
        raise PackageError("Incomplete PE optional header")
    magic = struct.unpack_from("<H", data, offset + 24)[0]
    if (machine == 0x14c and magic != 0x10b) or (machine == 0x8664 and magic != 0x20b):
        raise PackageError("Invalid PE format")
    return "x86" if machine == 0x14c else "x86_64"

def validate_apk(data):
    try:
        with zipfile.ZipFile(io.BytesIO(data)) as source:
            files = source.infolist()
            seen = set()
            total = 0
            for entry in files:
                path = entry.filename
                parts = path.split("/")
                if not path or path.startswith("/") or "\\" in path or any(p in (".", "..") for p in parts):
                    raise PackageError("Unsafe Android package path")
                if path in seen:
                    raise PackageError("Duplicate Android package entry")
                seen.add(path)
                if (entry.external_attr >> 16) & 0o170000 == 0o120000:
                    raise PackageError("Android package contains a symbolic link")
                total += entry.file_size
                if entry.file_size > MAX_PAYLOAD or total > 2 * MAX_PAYLOAD:
                    raise PackageError("Android package exceeds size limits")
            if "AndroidManifest.xml" not in seen:
                raise PackageError("Missing Android manifest")
            if not any(x.startswith("classes") and x.endswith(".dex") for x in seen) and not any(x.startswith("lib/") and x.endswith(".so") for x in seen):
                raise PackageError("Android package contains no executable code")
            if source.testzip() is not None:
                raise PackageError("Corrupt Android package entry")
    except (zipfile.BadZipFile, EOFError, OSError, RuntimeError) as error:
        raise PackageError("Invalid Android APK") from error
    return "android"

def validate_payload(data, platform):
    if not 1 <= len(data) <= MAX_PAYLOAD:
        raise PackageError("Application exceeds package size limits")
    if platform == "windows":
        return validate_exe(data)
    if platform == "android":
        return validate_apk(data)
    raise PackageError("Unsupported application platform")

def zipentry(name):
    entry = zipfile.ZipInfo(name, date_time=(1980, 1, 1, 0, 0, 0))
    entry.compress_type = zipfile.ZIP_STORED
    entry.external_attr = 0o100644 << 16
    return entry

def pack(source_path, destination, name, app_id, version, icon_path=None):
    valid_metadata(name, app_id, version)
    path = Path(source_path)
    ext = path.suffix.lower()
    platform = {".exe": "windows", ".apk": "android"}.get(ext)
    if not platform:
        raise PackageError("Expected an EXE or APK input")
    if not path.is_file() or path.stat().st_size > MAX_PAYLOAD:
        raise PackageError("Invalid input size")
    payload = path.read_bytes()
    architecture = validate_payload(payload, platform)
    manifest = {
        "schema": SCHEMA,
        "schema_version": 1,
        "id": app_id,
        "name": name,
        "version": version,
        "platform": platform,
        "architecture": architecture,
        "payload": ENTRY_PATHS[platform],
        "size": len(payload),
        "sha256": hashlib.sha256(payload).hexdigest(),
        "icon": None,
    }
    icon = None
    if icon_path:
        icon_path = Path(icon_path)
        if not icon_path.is_file() or icon_path.stat().st_size > MAX_ICON:
            raise PackageError("Invalid application icon")
        icon = icon_path.read_bytes()
        if len(icon) < 24 or icon[:8] != b"\x89PNG\r\n\x1a\n":
            raise PackageError("Icon must be a PNG")
        if struct.unpack_from(">II", icon, 16)[0] > 4096 or struct.unpack_from(">II", icon, 16)[1] > 4096:
            raise PackageError("Icon dimensions are too large")
        manifest["icon"] = "assets/icon.png"
        manifest["icon_sha256"] = hashlib.sha256(icon).hexdigest()
    manifest_data = json.dumps(manifest, sort_keys=True, separators=(",", ":"), ensure_ascii=False).encode("utf-8")
    output = Path(destination)
    output.parent.mkdir(parents=True, exist_ok=True)
    temp_name = None
    try:
        with tempfile.NamedTemporaryFile(prefix=".qapp-", suffix=".tmp", dir=output.parent, delete=False) as handle:
            temp_name = handle.name
        with zipfile.ZipFile(temp_name, "w") as archive:
            archive.writestr(zipentry("manifest.json"), manifest_data)
            archive.writestr(zipentry(manifest["payload"]), payload)
            if icon is not None:
                archive.writestr(zipentry("assets/icon.png"), icon)
        verify(temp_name)
        os.replace(temp_name, output)
    finally:
        if temp_name and os.path.exists(temp_name):
            os.unlink(temp_name)
    return manifest

def verify(path):
    try:
        with zipfile.ZipFile(path) as archive:
            entries = archive.infolist()
            names = [entry.filename for entry in entries]
            if len(names) not in (2, 3) or len(set(names)) != len(names) or "manifest.json" not in names:
                raise PackageError("Invalid Quartzie package entries")
            if archive.getinfo("manifest.json").file_size > 16384:
                raise PackageError("Oversized package manifest")
            manifest = json.loads(archive.read("manifest.json"))
            if not isinstance(manifest, dict) or manifest.get("schema") != SCHEMA or manifest.get("schema_version") != 1:
                raise PackageError("Unsupported Quartzie package")
            valid_metadata(manifest.get("name"), manifest.get("id"), manifest.get("version"))
            platform = manifest.get("platform")
            if platform not in ENTRY_PATHS or manifest.get("payload") != ENTRY_PATHS[platform]:
                raise PackageError("Invalid package payload")
            icon_path = manifest.get("icon")
            expected = {"manifest.json", ENTRY_PATHS[platform]}
            if icon_path is not None:
                if icon_path != "assets/icon.png":
                    raise PackageError("Invalid icon path")
                expected.add(icon_path)
            if set(names) != expected:
                raise PackageError("Unexpected package contents")
            for entry in entries:
                if entry.file_size > MAX_PAYLOAD or entry.compress_type != zipfile.ZIP_STORED:
                    raise PackageError("Unsupported package entry")
            payload = archive.read(ENTRY_PATHS[platform])
            if manifest.get("size") != len(payload) or manifest.get("sha256") != hashlib.sha256(payload).hexdigest():
                raise PackageError("Application payload failed verification")
            architecture = validate_payload(payload, platform)
            if manifest.get("architecture") != architecture:
                raise PackageError("Application architecture mismatch")
            if icon_path is not None:
                icon = archive.read(icon_path)
                if len(icon) > MAX_ICON or icon[:8] != b"\x89PNG\r\n\x1a\n" or manifest.get("icon_sha256") != hashlib.sha256(icon).hexdigest():
                    raise PackageError("Application icon failed verification")
            return manifest
    except (zipfile.BadZipFile, json.JSONDecodeError, UnicodeDecodeError, KeyError, OSError, RuntimeError) as error:
        raise PackageError("Invalid or corrupt Quartzie package") from error

def extract(path, destination):
    manifest = verify(path)
    output = Path(destination)
    output.mkdir(parents=True, exist_ok=True)
    target = output / Path(manifest["payload"]).name
    with zipfile.ZipFile(path) as archive:
        with target.open("xb") as handle:
            handle.write(archive.read(manifest["payload"]))
    return target

def main():
    parser = argparse.ArgumentParser(prog="qapp", description="Quartzie application package converter")
    commands = parser.add_subparsers(dest="command", required=True)
    create = commands.add_parser("pack")
    create.add_argument("source")
    create.add_argument("-o", "--output", required=True)
    create.add_argument("--name", required=True)
    create.add_argument("--id", required=True)
    create.add_argument("--version", required=True)
    create.add_argument("--icon")
    for command in ("verify", "inspect", "extract"):
        action = commands.add_parser(command)
        action.add_argument("package")
        if command == "extract":
            action.add_argument("-o", "--output", required=True)
    args = parser.parse_args()
    try:
        if args.command == "pack":
            result = pack(args.source, args.output, args.name, args.id, args.version, args.icon)
        elif args.command == "extract":
            result = {"extracted": str(extract(args.package, args.output))}
        else:
            result = verify(args.package)
        print(json.dumps(result, indent=2, sort_keys=True, ensure_ascii=False))
    except (PackageError, FileExistsError) as error:
        parser.exit(1, str(error) + "\n")

if __name__ == "__main__":
    main()
