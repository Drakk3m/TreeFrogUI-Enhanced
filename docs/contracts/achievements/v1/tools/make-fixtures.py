#!/usr/bin/env python3
"""Create a reproducible reference SD plus named negative overlays."""
import argparse
import copy
import hashlib
import json
import shutil
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory
from zipfile import ZIP_DEFLATED, ZipFile, ZipInfo

from validate import HOME, PIN, SD_ROOT, canon, digest, event_id

CONTRACT = HOME
ORIGINAL_SHA = 'd3e2e7b3f200dddd483904b91caa0baaa2970a3e413614cd4df9cfb9098b54ac'
GAME = '4ada71a3-fdad-4cbb-8fc9-a812f694ca1d'
PROFILE = 'b191ee8d-2287-4ce2-82a7-7c6434c707c1'
CORE = 'c318c4217bf8c0396161e9a896289d85aa92219e5ef6abf44e89161c2e4954d8'
SET = dict(namespace='diagnostic.r36sx-lab',set_id='nes-laboratory',revision=1)
FEATURES = ['byte','delta','hitcount','measured','reset-if']


def encode(obj):
    return (json.dumps(obj, ensure_ascii=False, indent=2)+'\n').encode()


def doc(kind, payload):
    obj = dict(kind=kind,version=1,payload=payload)
    if kind in ('progress','definition'):
        obj['sha256'] = digest(payload)
    return obj


def build_rom(lab):
    with TemporaryDirectory() as name:
        build = Path(name)
        script = build/'build_original.py'
        shutil.copyfile(lab/script.name,script)
        subprocess.run(['python3',str(script)],cwd=build,check=True,capture_output=True)
        rom=(build/'r36sx_achievement_lab.nes').read_bytes()
    assert hashlib.sha256(rom).hexdigest()==ORIGINAL_SHA
    return rom


def make(lab, out):
    rom = build_rom(lab)
    reference, cases = {}, []
    base = SD_ROOT.as_posix()
    rom_path='roms/FC/r36sx_achievement_lab.nes'

    def put(path, value):
        reference[path] = value if isinstance(value, bytes) else encode(value)
        return path

    def under(path, value):
        return put(f'{base}/{path}',value)

    def progress(scope, count, generation):
        ids=[str(i) for i in range(1,count+1)]
        return doc('progress',dict(scope=scope,generation=generation,
            events=[dict(achievement_id=i,event_id=event_id(scope,i),state='unlocked',
                         verification='local-runtime',timestamp_status='unknown') for i in ids]))

    def definition(content, game, achievement_set, conditions):
        return doc('definition',dict(set=achievement_set,game_id=game,content=content,
            authorization=dict(mode='original-local',license='Project-authored test fixture',
                               provenance='R36SX Achievement Laboratory, original build'),
            requires=dict(provider='nes-system-ram-v1',features=FEATURES),
            achievements=[dict(id=str(i+1),condition=value) for i,value in enumerate(conditions)]))

    def identity(content, game, path):
        return doc('identity',dict(game_id=game,platform='fc',content=content,rom_path=path))

    def ip(game, content):
        return f'identities/v1/{game}/{digest(content)}.json'

    def dp(achievement_set, content):
        return f"definitions/v1/{achievement_set['namespace']}/{achievement_set['set_id']}/{achievement_set['revision']}/{digest(content)}.json"

    content = dict(console_id=7,method='raw-sha256-v1',sha256=ORIGINAL_SHA)
    actual = (lab/'original.def').read_text().splitlines()
    assert actual[:4]==['R36SX-LAB-DEF 1','r36sx-lab','NES',ORIGINAL_SHA] and len(actual)==10
    conditions=actual[4:]
    put(rom_path,rom)
    put('roms/FC/.res/r36sx_achievement_lab.png',(lab/'artwork/r36sx_achievement_lab.png').read_bytes())
    ipath=under(ip(GAME,content),identity(content,GAME,rom_path))
    definition_doc=definition(content,GAME,SET,conditions)
    dpath=under(dp(SET,content),definition_doc)
    metadata=json.loads((lab/'metadata-example.json').read_text())
    assert metadata['sha256']==ORIGINAL_SHA and len(metadata['achievements'])==6
    rows=[]
    for row in metadata['achievements']:
        blob=(lab/'artwork'/Path(row['badge']).name).read_bytes()
        sha=hashlib.sha256(blob).hexdigest()
        under(f'assets/v1/{sha}.png',blob)
        rows.append(dict(id=row['id'].split(':')[-1],title=row['title'],
                         description=row['description'],points=row['points'],badge_sha256=sha))
    assert {row['id'] for row in rows}=={row['id'] for row in definition_doc['payload']['achievements']}
    assert sum(row['points'] for row in rows)==36
    under(f"metadata/v1/{GAME}/{SET['namespace']}/{SET['set_id']}/1.json",
          doc('metadata',dict(set=SET,game_id=GAME,source='original-local',metadata_only=True,achievements=rows)))
    under(f'players/v1/{PROFILE}.json',doc('profile',dict(profile_id=PROFILE,display_name='Offline Lab Player')))
    under('players/v1/selected.json',doc('selection',dict(profile_id=PROFILE)))
    under(f'capabilities/v1/{CORE}.json',doc('capability',dict(content_console_id=7,
          core_id='fceumm',core_sha256=CORE,provider='nes-system-ram-v1',
          regions=[dict(start=0,length=2048)],features=FEATURES,definition_versions=[1],rcheevos_revision=PIN,
          evidence='physical-legacy-only',generalized_enabled=False,
          restrictions=['Device core source revision unknown','Only the original lab passed physical tests'])))
    scope=dict(profile_id=PROFILE,set=SET,content=content,definition_sha256=definition_doc['sha256'])
    prefix=f'{base}/progress/v1/{PROFILE}/{digest(scope)}'
    a,b=f'{prefix}/a.json',f'{prefix}/b.json'
    put(a,progress(scope,0,0))

    def case(name, passed, changes, count=None, reason=None):
        manifest=dict(name=name,valid=passed,replace={},remove=[])
        if count is not None:
            manifest['expected_total']=count
        if reason:
            manifest['expected_error']=reason
        for target, value in changes.items():
            if value is None:
                manifest['remove'].append(target)
            else:
                index=len(manifest['replace'])
                path=out/'cases'/name/(f'{index:02d}-'+Path(target).name)
                path.parent.mkdir(parents=True,exist_ok=True)
                path.write_bytes(value if isinstance(value,bytes) else encode(value))
                manifest['replace'][target]=str(path.relative_to(out))
        cases.append(manifest)

    case('locked',True,{},0)
    case('partial',True,{b:progress(scope,2,1)},2)
    case('full',True,{a:progress(scope,6,2),b:progress(scope,2,1)},6)
    case('duplicate-across-slots',True,{a:progress(scope,2,1),b:progress(scope,2,2)},2)
    case('no-progress-unknown',True,{a:None},0)
    case('no-metadata',True,{next(n for n in reference if '/metadata/v1/' in n):None},0)
    duplicate=progress(scope,2,1)
    duplicate['payload']['events'].append(duplicate['payload']['events'][0])
    case('duplicate-inside-slot',False,{b:doc('progress',duplicate['payload'])},reason='duplicate identifier')
    bad_version=copy.deepcopy(definition_doc)
    bad_version['version']=2
    case('unknown-version',False,{dpath:bad_version},reason='schema')
    def change_condition(name,value,reason):
        clone=copy.deepcopy(definition_doc['payload'])
        clone['achievements'][0]['condition']=value
        updated=doc('definition',clone)
        migrated={**scope,'definition_sha256':updated['sha256']}
        dest=f'{base}/progress/v1/{PROFILE}/{digest(migrated)}/a.json'
        case(name,False,{dpath:updated,a:None,dest:progress(migrated,0,0)},reason=reason)
    change_condition('invalid-condition','0xH0010=>1','approved capability subset')
    change_condition('out-of-provider','0xH0800=1','outside provider')
    change_condition('metadata-fingerprint-as-condition','0123456789abcdef0123456789abcdef','approved capability subset')
    bad_rom = bytearray(rom)
    bad_rom[42]^=1
    case('wrong-rom',False,{rom_path:bytes(bad_rom)},reason='wrong ROM identity')
    bad_identity=identity(content,GAME,'roms/../unsafe.nes')
    case('traversal',False,{ipath:bad_identity},reason='unsafe path')
    different={**scope,'profile_id':'0206e534-6b96-4cb5-a1fe-e9c0c351a789'}
    case('unknown-profile',False,{f"{base}/progress/v1/{different['profile_id']}/{digest(different)}/a.json":progress(different,0,0)},reason='unknown progress profile')
    case('missing-definition',False,{dpath:None},reason='missing definition')
    revised={**scope,'set':{**SET,'revision':2}}
    case('revision-conflict',False,{f'{base}/progress/v1/{PROFILE}/{digest(revised)}/a.json':progress(revised,0,0)},reason='revision/identity mismatch')
    case('malformed-progress',False,{b:b'{"kind":"progress"\n'},reason='Expecting')
    damaged=progress(scope,1,1)
    damaged['sha256']='0'*64
    case('bad-integrity',False,{b:damaged},reason='integrity mismatch')
    bad_stamp=progress(scope,1,1)
    bad_stamp['payload']['events'][0]['timestamp_status']='trusted'
    case('invalid-clock',False,{b:doc('progress',bad_stamp['payload'])},reason='timestamp status mismatch')

    # A new hash and stable game ID, absent from the firmware, exercise
    # linkage and syntax checks without activating gameplay for this fixture.
    second=bytearray(rom)
    second[0x3ff0]=0xEA ^ 1
    other=dict(console_id=7,method='raw-sha256-v1',sha256=hashlib.sha256(second).hexdigest())
    assert other!=content
    game='7fd37121-c14c-4370-a205-7b3345ca5dc4'
    other_set=dict(namespace='local.contract-test',set_id='extra-game',revision=1)
    path='roms/FC/contract-second-game.nes'
    case('second-game-uncompiled',True,{path:bytes(second),
         f'{base}/{ip(game,other)}':identity(other,game,path),
         f'{base}/{dp(other_set,other)}':definition(other,game,other_set,['0xH0010>=1'])},0)

    out.mkdir(parents=True,exist_ok=True)
    (out/'cases.json').write_bytes(encode(cases))
    with ZipFile(out/'reference-sd.zip','w',ZIP_DEFLATED) as z:
        for name,value in sorted(reference.items()):
            entry=ZipInfo(name,date_time=(2026,11,28,0,0,0))
            entry.compress_type=ZIP_DEFLATED
            z.writestr(entry,value)
    for kind, src in [('legacy-locked.v1','progress-locked-example.v1'),
                      ('legacy-unlocked.v1','progress-unlocked-example.v1'),
                      ('legacy-lab.def','original.def')]:
        shutil.copyfile(lab/src,out/kind)
    sha='0123456789abcdef0123456789abcdef'
    fixture=dict(schema_version=1,console_id=1,game_id=42,ra_hash=sha,metadata_only=True,
                 achievements=[dict(id=7,title='Synthetic legacy fixture',description='Metadata test only.',
                                    points=5,badge='badge.png',definition_fingerprint_md5=sha)])
    (out/'legacy-public-gamepack.json').write_bytes(encode(fixture))


def main():
    parser=argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--laboratory',type=Path,required=True)
    parser.add_argument('--output',type=Path,default=CONTRACT/'fixtures')
    args=parser.parse_args()
    make(args.laboratory,args.output)


if __name__=='__main__':
    main()
