#!/usr/bin/env python3
"""Capture, rather than replace, audited Studio and FrogUI registry facts."""
import argparse
import importlib
import json
import re
import sys
from pathlib import Path

ap = argparse.ArgumentParser(description=__doc__)
ap.add_argument('studio', type=Path)
ap.add_argument('frogui', type=Path)
ap.add_argument('output', type=Path)
a = ap.parse_args()
sys.path.insert(0, str(a.studio.resolve()))
library = importlib.import_module('universal_library')
profiles = importlib.import_module('platform_profiles')
inventory = importlib.import_module('sd_migration_inventory')
ra = importlib.import_module('ra_auto')

source = a.frogui.read_text()
table = source.split('static const ConsoleMapping console_mappings[] = {',1)[1].split('};',1)[0]
cores = {}
for folder, target in re.findall(r'\{"([^"]+)"\s*,\s*([^}]+)\}', table):
    path = re.fullmatch(r'CORES_PATH\s*"/([^"]+)"', target.strip())
    cores.setdefault(folder.lower(), path[1] if path else target.strip())

systems = (set(profiles.TREEFROG_PLATFORMS) | set(profiles.CUBE_PLATFORMS) |
           set(inventory.TREEFROG_ALIASES.values()) | set(library.ALIASES.values()) |
           set(ra.CONSOLES))
pending = {
    'atari': 'Family spans multiple RA console IDs; extension alone does not qualify an adapter.',
    'neogeo': 'Studio inventory neogeo maps to MAME; geolith is a separate route.',
    'mame': 'Arcade grouping is not a universal core compatibility declaration.',
}
items = []
empty_fixture = a.output.parent / '_absent_contract_probe_root'
for label in sorted(systems):
    canonical = label.lower()
    cube = sorted(profiles.CUBE_PLATFORMS.get(label,()))
    tree = sorted(profiles.TREEFROG_PLATFORMS.get(label,()))
    tf_aliases = {x for x,y in inventory.TREEFROG_ALIASES.items() if y==label}
    legacy = {x.lower() for x,y in library.ALIASES.items() if y==label}
    candidates = {canonical} | tf_aliases | legacy
    core_routes = {x:cores[x] for x in sorted(candidates & cores.keys())}
    items.append({
        'canonical': canonical,
        'studio_id': label,
        'legacy_aliases': sorted(legacy),
        'cube': {'folder':label if cube else None, 'extensions':cube},
        'treefrog': {'default_folder':library.TREEFROG_FOLDERS.get(label,canonical) if tree else None,
                     'preferred_add_rom_folder':profiles.preferred_folder(label,empty_fixture).name if tree else None,
                     'inventory_aliases':sorted(tf_aliases), 'extensions':tree,
                     'core_associations_source':core_routes},
        'ra_console_id':ra.CONSOLES.get(label),
        'ra_console_by_extension':ra.ATARI_IDS if label=='ATARI' else {},
        'runtime_evidence':'original NES laboratory with exact FCEUmm test-card binary only' if label=='FC' else None,
        'ambiguity':pending.get(canonical)
    })
known = {x for i in items for x in (i['canonical'],*i['treefrog']['inventory_aliases'],*i['legacy_aliases'])}
result = {'schema_version':1,'origin':{
    'studio':'88.3.26 f0368d6a34234d3e7a5d0c3925f851aeeb02d0170019f2ae75c63e31220447ad',
    'frogui':'15fbea977ee4e7e01d2a1659b8fa4a2e981aba9b',
    'rcheevos':'fd57e900758a9e3de343e7b0316b8a6059dce228'},
    'platforms':items,
    'quarantined_aliases':{'fds':'Separate FDS media/core route; v1 NES cartridge reconciliation does not infer equivalence.',
        'fcf':'Studio inventory association lacks matching explicit FrogUI core route.',
        'neogeo':'Inventory resolves to MAME while the canonical Studio ID also means NEOGEO.',
        'pcesgx':'SuperGrafx and PCE grouped by inventory only.',
        'a26':'Atari family uses distinct console IDs; require an approved qualified adapter.',
        'a78':'Atari family uses distinct console IDs; require an approved qualified adapter.'},
    'frogui_only_routes':{k:v for k,v in cores.items() if k not in known},
    'studio_inventory_only_folders':sorted(set(inventory.TREEFROG_OFFICIAL_FOLDERS) - known)}
a.output.write_text(json.dumps(result,ensure_ascii=False,indent=2)+'\n')
