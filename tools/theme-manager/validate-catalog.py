#!/usr/bin/env python3
"""Check that the ROM Studio catalog stays aligned with FrogUI manifest declarations."""
import json
import re
from pathlib import Path

tool_dir = Path(__file__).resolve().parent
root = tool_dir.parents[1]
catalog = json.loads((tool_dir / "catalog.json").read_text(encoding="utf-8"))
source = (root / "theme_resource.c").read_text(encoding="utf-8")
runtime = source.split("static ThemeResourceKind resource_kind", 1)[1].split("static const ManifestResource", 1)[0]
assert catalog["contract_version"] == 1 and catalog["manifest_version"] == 1
supported_types = catalog["resource_types"]
known_ids, ids, editable = set(), set(), set()
for row in catalog["resources"]:
    id = row["id"]
    assert id not in ids, id
    ids.add(id)
    assert row["type"] in supported_types, id
    for field in ("display_name", "category", "consumers", "classification", "since",
                  "default_reference", "fallback", "configuration_keys", "optional"):
        assert field in row, (id, field)
    assert row["editable"] == (row["classification"] == "themeable"), id
    if row["editable"]:
        editable.add(id)
        known_ids.update([id, *row["aliases"]])
    else:
        assert row["type"] == "protected" and not row["aliases"], id
assert known_ids - {"platform.favourites.icon"} == set(re.findall(r'strcmp\(id, "([^"]+)"\)', runtime)) | {
    prefix + "<key>" + suffix
    for prefix, suffix in re.findall(r'scoped_id\(id, "([^"]+)", "([^"]+)"\)', runtime)
}, known_ids
for row in catalog["resources"]:
    if "reference" not in row:
        continue
    identifier, path, style = row["reference"]
    pattern = re.escape(row["id"]).replace(re.escape("<key>"), r"[a-z0-9_-]+")
    assert row["id"] in editable and re.fullmatch(pattern, identifier), identifier
    assert not Path(path).is_absolute() and ".." not in Path(path).parts and style

parser = source.split("static bool parse_presentation", 1)[1].split("static bool parse_theme", 1)[0]
assert set(catalog["presentation"]) == set(re.findall(r'strcmp\(key, "([^"]+)"\)', parser))
ranges = {
    key: (int(low), int(high))
    for low, high, key in re.findall(r'number\(value, (\d+), (\d+), &p->(\w+)\)', parser)
}
for key, limits in catalog["presentation"].items():
    if key in ranges:
        assert ranges[key] == (limits["min"], limits["max"]), key
        assert re.search(rf'\.{key}\s*=\s*{limits["default"]}\b', source), key
    else:
        assert limits == {"values": ["off", "on"], "default": "off"}, key

header = (root / "theme_resource.h").read_text(encoding="utf-8")
for define, expected in [
    ("THEME_BACKGROUND_MAX_WIDTH", supported_types["background"]["maximum_dimensions"][0]),
    ("THEME_BACKGROUND_MAX_HEIGHT", supported_types["background"]["maximum_dimensions"][1]),
    ("THEME_IMAGE_MAX_EDGE", supported_types["icon"]["maximum_dimensions"][0]),
]:
    assert re.search(rf"#define {define} {expected}\b", header), define
print(f"PASS catalog: {len(editable)} editable IDs, {len(ids - editable)} protected roles, {len(ranges)} bounds")
