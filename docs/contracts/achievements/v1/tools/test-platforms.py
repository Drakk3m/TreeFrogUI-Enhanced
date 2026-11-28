#!/usr/bin/env python3
"""Validate migration and the real Studio public export with the proposed rule."""
from hashlib import sha256
from pathlib import Path
from zipfile import ZipFile
import json

from platform_binding import bind
from validate import HOME, safe_path

def check(label, platform, locator, console, expected, *, legacy=False):
    safe_path(locator)
    try:
        actual=bind(platform, locator, console, legacy=legacy)
    except ValueError as error:
        assert expected is None,(label,error)
    else:
        assert actual==expected,(label,actual)
    print('PASS',label)

for title,platform,path,console,result,legacy in [
    ('legacy FC directory','FC','roms/FC/lab.nes',7,'fc',True),
    ('legacy FC on nes','FC','roms/nes/lab.nes',7,'fc',True),
    ('TreeFrogUI native nes alias','nes','roms/nes/lab.nes',7,'fc',True),
    ('CubeGM native FC export','FC','FC/lab.nes',7,'fc',True),
    ('generalized TreeFrogUI canonical export','fc','roms/nes/lab.nes',7,'fc',False),
    ('generalized CubeGM canonical identity','fc','FC/lab.nes',7,'fc',False),
    ('alias normalized independently of case','NESQ','roms/nesq/new.nes',7,'fc',True),
    ('literal alias disallowed as v1 canonical identity','nes','roms/nes/new.nes',7,None,False),
    ('SFC/snes','SFC','roms/snes/game.sfc',3,'sfc',True),
    ('MD/sega','MD','roms/sega/game.md',1,'md',True),
    ('unknown token','Mystery','roms/nes/shared.nes',7,None,True),
    ('ambiguous media alias','FC','roms/fds/shared.nes',7,None,True),
    ('conflicting console','FC','roms/nes/shared.nes',3,None,True),
    ('cross-platform mismatch','FC','roms/snes/shared.nes',7,None,True),
    ('same filename NES','FC','roms/nes/shared.nes',7,'fc',True),
    ('same filename SNES','SFC','roms/snes/shared.nes',3,'sfc',True),
]:
    check(title,platform,path,console,result,legacy=legacy)

# Relocations of the same exact content preserve the immutable variant and definition.
# The independent host bundle validation tests a one-byte ROM mismatch.
with ZipFile(HOME/'fixtures/reference-sd.zip') as ref:
    file='roms/FC/r36sx_achievement_lab.nes'
    original=ref.read(file)
    left={'locator':'FC/shared.nes','platform':bind('fc','FC/shared.nes',7),'sha':sha256(original).hexdigest()}
    right={'locator':'roms/nes/new.nes','platform':bind('fc','roms/nes/new.nes',7),'sha':sha256(original).hexdigest()}
    assert left['locator']!=right['locator'] and (left['platform'],left['sha'])==(right['platform'],right['sha'])
    assert sha256(original[:-1]+bytes([original[-1]^1])).hexdigest()!=right['sha']
    print('PASS relocation and launcher migration preserve canonical platform and exact content')
    legacy_defs=(HOME/'fixtures/legacy-lab.def').read_text().splitlines()[4:]
    definition=next(json.loads(ref.read(n)) for n in ref.namelist() if '/definitions/v1/' in n)
    assert [r['condition'] for r in definition['payload']['achievements']]==legacy_defs

with ZipFile(HOME/'fixtures/RA-02.1-Studio-88.3.26-Sanitized-Export.zip') as export:
    for member in export.namelist():
        safe_path(member)
    root='cubegm/enhanced/ra/'
    (locator,identity),=json.loads(export.read(root+'rom_identity.json')).items()
    assert locator=='roms/nes/audit.nes' and identity['platform']=='FC'
    assert bind(identity['platform'],locator,identity['console_id'],legacy=True)=='fc'
    assert identity['metadata_only'] is True and identity['runtime_ready'] is False
    pack=json.loads(export.read(root+f"cache/{identity['console_id']}/{identity['ra_hash']}/gamepack.json"))
    assert pack['metadata_only'] is True and len(pack['achievements'])==6
    for row in pack['achievements']:
        assert not {'definition','conditions','trigger','MemAddr'} & row.keys()
        assert root+'badges/'+row['badge']+'.png' in export.namelist()
    assert len(pack['achievements'][0]['definition_fingerprint_md5'])==32
    assert not any('/definitions/' in n or '/progress/' in n for n in export.namelist())
    print('PASS actual Studio 88.3.26 artifact resolves under proposed alias rule; remains metadata-only')
