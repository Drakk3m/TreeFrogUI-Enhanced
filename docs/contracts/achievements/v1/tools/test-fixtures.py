#!/usr/bin/env python3
"""Run all fixture overlays against the strict reference validator."""
import argparse
import hashlib
import json
import re
import subprocess
import sys
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZipFile

from validate import HOME, safe_path

parser=argparse.ArgumentParser(description=__doc__)
parser.add_argument('--checker',required=True,type=Path)
args=parser.parse_args()
fixtures=HOME/'fixtures'
cases=json.loads((fixtures/'cases.json').read_text())
with ZipFile(fixtures/'reference-sd.zip') as archive:
    for member in archive.namelist():
        safe_path(member)
    for case in cases:
        with TemporaryDirectory(prefix='ra-v1-') as temp:
            sd=Path(temp)
            archive.extractall(sd)
            for name in case['remove']:
                safe_path(name)
                (sd/name).unlink()
            for name,source in case['replace'].items():
                safe_path(name)
                safe_path(source)
                destination=sd/name
                destination.parent.mkdir(parents=True,exist_ok=True)
                destination.write_bytes((fixtures/source).read_bytes())
            result=subprocess.run([sys.executable,str(HOME/'tools/validate.py'),str(sd),
                                   '--checker',str(args.checker.resolve())],capture_output=True,text=True)
            if case['valid']:
                expect=f"{case['expected_total']} distinct durable unlocks"
                assert result.returncode==0 and expect in result.stdout,(case['name'],result.stderr)
            else:
                assert result.returncode==1 and case['expected_error'] in result.stderr,(
                    case['name'],result.stderr)
            print(f"PASS {case['name']}")

    defs=[json.loads(archive.read(name)) for name in archive.namelist() if '/definitions/v1/' in name]
    assert len(defs)==1
    legacy=(fixtures/'legacy-lab.def').read_text().splitlines()
    assert len(legacy)==10 and legacy[0]=='R36SX-LAB-DEF 1'
    assert [a['condition'] for a in defs[0]['payload']['achievements']]==legacy[4:]
    assert defs[0]['payload']['content']['sha256']==legacy[3]
    assert hashlib.sha256(archive.read('roms/FC/r36sx_achievement_lab.nes')).hexdigest()==legacy[3]
    packs=[json.loads(archive.read(name)) for name in archive.namelist() if '/metadata/v1/' in name]
    assert len(packs)==1 and sum(a['points'] for a in packs[0]['payload']['achievements'])==36
    for achievement in packs[0]['payload']['achievements']:
        assert any(name.endswith('/'+achievement['badge_sha256']+'.png') for name in archive.namelist())
    for name,slots in [('legacy-locked.v1',0),('legacy-unlocked.v1',63)]:
        data=(fixtures/name).read_text().splitlines()
        assert data[:6]==['R36SX-LAB-PROGRESS 1','local-player','r36sx-lab','NES',legacy[3],'1']
        assert len(data)==7 and re.fullmatch('[0-3][0-9a-f]',data[6]) and int(data[6],16)==slots
    old=json.loads((fixtures/'legacy-public-gamepack.json').read_text())
    assert old['schema_version']==1 and old['metadata_only'] is True
    assert re.fullmatch('[0-9a-f]{32}',old['achievements'][0]['definition_fingerprint_md5'])
    assert 'condition' not in old['achievements'][0]
    print('PASS original ROM SHA, six exact conditions, badge IDs and 36 points; legacy v1 files and metadata-only fixture')
