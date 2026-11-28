"""Host reference for the shared platform binding rule; no launch or writes."""
import json
import re
from collections import defaultdict
from pathlib import Path

REGISTRY = json.loads((Path(__file__).resolve().parent.parent/'schemas/platform-mapping-v1.json').read_text())
PLATFORMS = {row['canonical']: row for row in REGISTRY['platforms']}
BLOCKED = REGISTRY['quarantined_aliases']
TOKEN = re.compile(r'[A-Za-z0-9][A-Za-z0-9._-]{0,63}', re.ASCII)
ALIASES = defaultdict(set)
FOLDERS = {name: defaultdict(set) for name in ('cubegm','treefrogui')}
for canonical,row in PLATFORMS.items():
    for name in (canonical,*row['legacy_aliases'],*row['treefrog']['inventory_aliases']):
        ALIASES[name.lower()].add(canonical)
    if row['cube']['folder']:
        FOLDERS['cubegm'][row['cube']['folder'].lower()].add(canonical)
    if row['treefrog']['extensions']:
        tree=row['treefrog']
        for name in {canonical,tree['default_folder'],tree['preferred_add_rom_folder'],
                     *tree['inventory_aliases']} - {None}:
            FOLDERS['treefrogui'][name.lower()].add(canonical)

def lookup(value, index):
    if not isinstance(value,str) or not TOKEN.fullmatch(value):
        raise ValueError('invalid platform token')
    folded=value.lower()
    if folded in BLOCKED:
        raise ValueError('ambiguous or unqualified platform token: '+folded)
    choices=index.get(folded,())
    if len(choices)!=1:
        raise ValueError('unknown or ambiguous platform token: '+folded)
    return next(iter(choices))

def bind(platform, locator, console_id, *, legacy=False):
    """Return canonical identity. Caller must still validate exact path and ROM bytes."""
    parts=locator.split('/')
    if parts[0]=='roms' and len(parts)>=3:
        mode,folder='treefrogui',parts[1]
    elif len(parts)>=2 and parts[0].lower()!='roms':
        mode,folder='cubegm',parts[0]
    else:
        raise ValueError('invalid ROM locator')
    canonical=lookup(platform,ALIASES)
    physical=lookup(folder,FOLDERS[mode])
    if canonical!=physical:
        raise ValueError('cross-platform locator mismatch')
    if not legacy and platform!=canonical:
        raise ValueError('v1 platform must use canonical identifier')
    expected=PLATFORMS[canonical]['ra_console_id']
    if type(console_id) is not int or expected is None or console_id!=expected:
        raise ValueError('console ID conflicts with normalized platform or is not qualified')
    return canonical
