#!/usr/bin/env python3
"""Repack the audited board initrd with matching early modules; never install it."""
import argparse
import gzip
import hashlib
import io
from pathlib import Path, PurePosixPath
import shutil
import stat
import struct
import subprocess
import tempfile
import zlib

BASE_SHA256 = "a866ace9754b86b9fd552ecfe16f14de5386246f8bf2e537b28634916d55768a"
RELEASE = "6.1.99-rk3576-car-bsp"
EARLY_MODULE = "kernel/drivers/net/usb/cdc_eem.ko"
MAX_ARCHIVE = 64 * 1024 * 1024


def unpack_image(image):
    if len(image) < 64:
        raise ValueError("Truncated legacy image header")
    fields = struct.unpack(">7I4B32s", image[:64])
    if fields[0] != 0x27051956 or fields[9:11] != (3, 1):
        raise ValueError("Expected legacy ramdisk image with gzip compression")
    header = bytearray(image[:64])
    header[4:8] = bytes(4)
    if zlib.crc32(header) != fields[1]:
        raise ValueError("Legacy header CRC mismatch")
    payload = image[64:]
    if len(payload) != fields[3] or zlib.crc32(payload) != fields[6]:
        raise ValueError("Legacy payload size/CRC mismatch")
    with gzip.GzipFile(fileobj=io.BytesIO(payload)) as stream:
        archive = stream.read(MAX_ARCHIVE + 1)
    if len(archive) > MAX_ARCHIVE:
        raise ValueError("Decompressed archive exceeds size limit")
    return fields, archive


def parse_archive(archive):
    """Read newc records in memory, preserving original metadata and hard links."""
    entries = []
    names = set()
    offset = 0
    while offset + 110 <= len(archive):
        if archive[offset:offset + 6] != b"070701":
            raise ValueError("Expected newc header")
        try:
            fields = [int(archive[offset + 6 + i * 8:offset + 14 + i * 8], 16) for i in range(13)]
        except ValueError as error:
            raise ValueError("Invalid newc numeric field") from error
        size, namesize = fields[6], fields[11]
        name_end = offset + 110 + namesize
        if not namesize or name_end > len(archive) or archive[name_end - 1] != 0:
            raise ValueError("Truncated or unterminated newc name")
        encoded = archive[offset + 110:name_end - 1]
        if b"\0" in encoded:
            raise ValueError("Embedded NUL in newc name")
        name = encoded.decode("utf-8")
        path = PurePosixPath(name)
        if path.is_absolute() or ".." in path.parts:
            raise ValueError("Unsafe newc name")
        name = str(path)
        data_start = (name_end + 3) & ~3
        end = (data_start + size + 3) & ~3
        if end > len(archive):
            raise ValueError("Truncated newc data")
        if name == "TRAILER!!!":
            if size or any(archive[end:]):
                raise ValueError("Unexpected data after newc trailer")
            return entries
        if name in names:
            raise ValueError("Duplicate newc name")
        names.add(name)
        entries.append({"name": name, "fields": fields,
                        "data": archive[data_start:data_start + size],
                        "raw": archive[offset:end]})
        offset = end
    raise ValueError("Missing newc trailer")


def record(name, mode, data, inode):
    encoded = name.encode() + b"\0"
    fields = [inode, mode, 0, 0, 2 if stat.S_ISDIR(mode) else 1, 0, len(data),
              0, 0, 0, 0, len(encoded), 0]
    header = b"070701" + b"".join(f"{value:08x}".encode() for value in fields)
    output = header + encoded
    output += bytes((-len(output)) % 4)
    output += data
    return output + bytes((-len(output)) % 4)


def wrap_image(fields, archive):
    payload = gzip.compress(archive, compresslevel=9, mtime=0)
    fields = list(fields)
    fields[1] = 0
    fields[3] = len(payload)
    fields[6] = zlib.crc32(payload)
    fields[11] = b"car-bsp initrd".ljust(32, b"\0")
    header = struct.pack(">7I4B32s", *fields)
    fields[1] = zlib.crc32(header)
    return struct.pack(">7I4B32s", *fields) + payload


def replace_modules(entries, module_root):
    removed = [entry for entry in entries if entry["name"].startswith("usr/lib/modules/")]
    if [entry["name"] for entry in removed if entry["name"].endswith(".ko")] != [
            "usr/lib/modules/6.1.84/" + EARLY_MODULE]:
        raise ValueError("Base initrd module set differs from audited input")
    retained = [entry for entry in entries if not entry["name"].startswith("usr/lib/modules/")]
    if not any(entry["name"] == "init" and stat.S_ISREG(entry["fields"][1]) for entry in retained):
        raise ValueError("Missing original init program")
    if not any(entry["name"] == "lib" and entry["data"] == b"usr/lib" for entry in retained):
        raise ValueError("Expected original /lib -> usr/lib layout")
    inode = max(entry["fields"][0] for entry in entries) + 1
    parts = [entry["raw"] for entry in retained]
    for path in [module_root] + sorted(module_root.rglob("*")):
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise ValueError("Only regular module files and directories are supported")
        name = "usr/lib/modules/" + RELEASE
        if path != module_root:
            name += "/" + path.relative_to(module_root).as_posix()
        mode = stat.S_IFDIR | 0o755 if path.is_dir() else stat.S_IFREG | 0o644
        data = b"" if path.is_dir() else path.read_bytes()
        if inode >= 0xFFFFFFFF:
            raise ValueError("newc inode number overflow")
        parts.append(record(name, mode, data, inode))
        inode += 1
    parts.append(record("TRAILER!!!", 0, b"", inode))
    result = b"".join(parts)
    result += bytes((-len(result)) % 512)
    return result


def prepare(source, modules, output, depmod):
    image = source.read_bytes()
    if hashlib.sha256(image).hexdigest() != BASE_SHA256:
        raise ValueError("Input SHA256 differs from audited board initrd")
    if output.exists():
        raise ValueError("Output exists; refuse to overwrite")
    fields, archive = unpack_image(image)
    entries = parse_archive(archive)
    module = (modules / EARLY_MODULE).read_bytes()
    if len(module) < 20 or module[:6] != b"\x7fELF\x02\x01" or struct.unpack_from("<H", module, 18)[0] != 183:
        raise ValueError("Early module must be ELF64 AArch64")
    if b"vermagic=" + RELEASE.encode() + b" " not in module:
        raise ValueError("Early module vermagic does not match new kernel")
    dependency = [line for line in (modules / "modules.dep").read_text().splitlines()
                  if line.startswith(EARLY_MODULE + ":")]
    if dependency != [EARLY_MODULE + ":"]:
        raise ValueError("Early module has missing or unexpected loadable dependencies")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix="initrd-modules-", dir=output.parent) as temp:
        root = Path(temp)
        target = root / "lib/modules" / RELEASE
        (target / EARLY_MODULE).parent.mkdir(parents=True)
        (target / EARLY_MODULE).write_bytes(module)
        for name in ("modules.builtin", "modules.builtin.modinfo"):
            shutil.copyfile(modules / name, target / name)
        (target / "modules.order").write_text(EARLY_MODULE + "\n")
        subprocess.run([depmod, "-b", str(root.resolve()), RELEASE], check=True,
                       capture_output=True, text=True, timeout=30)
        new_archive = replace_modules(entries, target)
    new_image = wrap_image(fields, new_archive)
    # Verify the actual serialized output, including byte-preservation of all other entries.
    new_fields, decoded = unpack_image(new_image)
    new_entries = parse_archive(decoded)
    preserved = {entry["name"]: entry["raw"] for entry in entries
                 if not entry["name"].startswith("usr/lib/modules/")}
    actual = {entry["name"]: entry["raw"] for entry in new_entries if entry["name"] in preserved}
    if actual != preserved:
        raise ValueError("Non-module entries changed")
    module_names = [entry["name"] for entry in new_entries if entry["name"].endswith(".ko")]
    if module_names != ["usr/lib/modules/" + RELEASE + "/" + EARLY_MODULE]:
        raise ValueError("Unexpected output module set")
    output.write_bytes(new_image)
    output.chmod(0o644)
    digest = hashlib.sha256(new_image).hexdigest()
    output.with_name(output.name + ".sha256").write_text(f"{digest}  {output.name}\n")
    print(f"PASS: header/payload CRC, gzip/newc, {len(preserved)} preserved entries, matching early module.")
    print(f"Output: {output} ({len(new_image)} bytes); SHA256: {digest}")
    print(f"Legacy architecture field retained: {new_fields[8]} (vendor convention); payload binaries/module remain AArch64.")
    print("Not installed or boot-tested; original Image/initrd and boot selection unchanged.")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("source", type=Path)
    parser.add_argument("modules", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--depmod", default="depmod")
    args = parser.parse_args()
    try:
        prepare(args.source, args.modules, args.output, args.depmod)
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        parser.exit(1, f"STOP: {error}\n")


if __name__ == "__main__":
    main()
