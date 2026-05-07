#!/usr/bin/env python3
"""
Growsandbox item registry encoder.

Reads script/items.txt (backslash-separated source format) and writes
bin/items.dat (binary, magic="GSBX", little-endian).

Usage:
    py script/encode_items.py [--source FILE] [--output FILE] [--expected-version N]
"""
import argparse
import os
import struct
import sys

EXPECTED_VERSION = 2
MAGIC = b"GSBX"

LAYER_FG = 0
LAYER_BG = 1

MAX_NAME_LEN = 64
MAX_ASSET_LEN = 128
MAX_DESC_LEN = 255

FIELD_NAMES = [
    "id", "name", "asset", "layer", "maxHp",
    "solid", "description", "stack_max", "breakable",
    "spread_type", "anchor_col", "anchor_row",
]


def fail(line_no, reason):
    sys.stderr.write(f"error: line {line_no}: {reason}\n")
    sys.exit(1)


def parse_source(path):
    """Returns (items: list[dict], version: int, item_count_declared: int)."""
    items = []
    version = None
    count_declared = None

    with open(path, "r", encoding="utf-8", newline=None) as f:
        for raw_line_no, raw in enumerate(f, start=1):
            line = raw.rstrip("\r\n").rstrip()
            if not line or line.startswith("//"):
                continue
            parts = line.split("\\")
            head = parts[0]

            if head == "version":
                if len(parts) != 2:
                    fail(raw_line_no, "version line malformed")
                try:
                    version = int(parts[1])
                except ValueError:
                    fail(raw_line_no, f"version not integer: {parts[1]!r}")
            elif head == "itemCount":
                if len(parts) != 2:
                    fail(raw_line_no, "itemCount line malformed")
                try:
                    count_declared = int(parts[1])
                except ValueError:
                    fail(raw_line_no, f"itemCount not integer: {parts[1]!r}")
            elif head == "add_item":
                if len(parts) != 1 + len(FIELD_NAMES):
                    fail(raw_line_no,
                         f"add_item expects {len(FIELD_NAMES)} fields, got {len(parts) - 1}")
                fields = dict(zip(FIELD_NAMES, parts[1:]))
                fields["_line"] = raw_line_no
                items.append(fields)
            else:
                fail(raw_line_no, f"unknown directive: {head!r}")

    if version is None:
        fail(0, "missing version header")
    if count_declared is None:
        fail(0, "missing itemCount header")
    return items, version, count_declared


def validate(items, version, count_declared, expected_version):
    if version != expected_version:
        sys.stderr.write(
            f"error: version {version} does not match expected {expected_version}\n")
        sys.exit(1)
    if count_declared != len(items):
        sys.stderr.write(
            f"error: itemCount={count_declared} but found {len(items)} add_item lines\n")
        sys.exit(1)
    for expected_id, item in enumerate(items):
        line_no = item["_line"]
        try:
            id_val = int(item["id"])
        except ValueError:
            fail(line_no, f"id not integer: {item['id']!r}")
        if id_val != expected_id:
            fail(line_no,
                 f"id={id_val} but expected sequential id={expected_id}")
        if not (0 <= id_val <= 255):
            fail(line_no, f"id out of u8 range: {id_val}")

        name = item["name"]
        if len(name) > MAX_NAME_LEN:
            fail(line_no, f"name too long ({len(name)} > {MAX_NAME_LEN})")

        asset = item["asset"]
        if len(asset) > MAX_ASSET_LEN:
            fail(line_no, f"asset too long ({len(asset)} > {MAX_ASSET_LEN})")

        layer = item["layer"]
        if layer not in ("FG", "BG"):
            fail(line_no, f"layer must be FG or BG, got {layer!r}")

        try:
            max_hp = int(item["maxHp"])
        except ValueError:
            fail(line_no, f"maxHp not integer: {item['maxHp']!r}")
        if not (0 <= max_hp <= 255):
            fail(line_no, f"maxHp out of u8 range: {max_hp}")

        solid = item["solid"]
        if solid not in ("0", "1"):
            fail(line_no, f"solid must be 0 or 1, got {solid!r}")

        desc = item["description"]
        if len(desc) > MAX_DESC_LEN:
            fail(line_no,
                 f"description too long ({len(desc)} > {MAX_DESC_LEN})")

        try:
            stack_max = int(item["stack_max"])
        except ValueError:
            fail(line_no, f"stack_max not integer: {item['stack_max']!r}")
        if not (0 <= stack_max <= 65535):
            fail(line_no, f"stack_max out of u16 range: {stack_max}")

        breakable = item["breakable"]
        if breakable not in ("0", "1"):
            fail(line_no, f"breakable must be 0 or 1, got {breakable!r}")

        spread_type = item["spread_type"]
        if spread_type not in ("1", "2"):
            fail(line_no, f"spread_type must be 1 or 2, got {spread_type!r}")

        try:
            anchor_col = int(item["anchor_col"])
        except ValueError:
            fail(line_no, f"anchor_col not integer: {item['anchor_col']!r}")
        if not (0 <= anchor_col <= 31):
            fail(line_no, f"anchor_col out of range 0..31: {anchor_col}")

        try:
            anchor_row = int(item["anchor_row"])
        except ValueError:
            fail(line_no, f"anchor_row not integer: {item['anchor_row']!r}")
        if not (0 <= anchor_row <= 31):
            fail(line_no, f"anchor_row out of range 0..31: {anchor_row}")


def write_binary(items, version, output_path):
    os.makedirs(os.path.dirname(output_path) or ".", exist_ok=True)
    with open(output_path, "wb") as f:
        f.write(struct.pack("<4sHI", MAGIC, version, len(items)))
        for item in items:
            id_val = int(item["id"])
            name = item["name"].encode("ascii")
            asset = item["asset"].encode("ascii")
            layer = LAYER_FG if item["layer"] == "FG" else LAYER_BG
            max_hp = int(item["maxHp"])
            solid = int(item["solid"])
            desc = item["description"].encode("ascii")
            stack_max = int(item["stack_max"])
            breakable = int(item["breakable"])

            f.write(struct.pack("<B", id_val))
            f.write(struct.pack("<B", len(name)))
            f.write(name)
            f.write(struct.pack("<B", len(asset)))
            f.write(asset)
            f.write(struct.pack("<BBB", layer, max_hp, solid))
            f.write(struct.pack("<B", len(desc)))
            f.write(desc)
            f.write(struct.pack("<HB", stack_max, breakable))
            spread_type = int(item["spread_type"])
            anchor_col = int(item["anchor_col"])
            anchor_row = int(item["anchor_row"])
            f.write(struct.pack("<BBB", spread_type, anchor_col, anchor_row))


def main():
    parser = argparse.ArgumentParser(description="Growsandbox item registry encoder")
    parser.add_argument("--source", default="script/items.txt")
    parser.add_argument("--output", default="bin/items.dat")
    parser.add_argument("--expected-version", type=int, default=EXPECTED_VERSION)
    args = parser.parse_args()

    items, version, count_declared = parse_source(args.source)
    validate(items, version, count_declared, args.expected_version)
    write_binary(items, version, args.output)

    size = os.path.getsize(args.output)
    print(f"OK: encoded {len(items)} items, version {version}, {size} bytes -> {args.output}")


if __name__ == "__main__":
    main()
