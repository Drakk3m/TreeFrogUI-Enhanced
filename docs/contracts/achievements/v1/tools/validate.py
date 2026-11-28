#!/usr/bin/env python3
"""Validate a proposed contract checkout or one self-contained fixture SD tree."""
import argparse
import hashlib
import json
import re
import struct
import subprocess
import sys
import unicodedata
import zlib
from pathlib import Path

from jsonschema import Draft202012Validator

HOME = Path(__file__).resolve().parent.parent
PIN = 'fd57e900758a9e3de343e7b0316b8a6059dce228'
LIMIT = 192 * 1024
SD_ROOT = Path('cubegm/enhanced/ra')
SCHEMA = json.loads((HOME/'schemas/contract-v1.schema.json').read_text())
Draft202012Validator.check_schema(SCHEMA)
VALIDATOR = Draft202012Validator(SCHEMA)
CONDITION = re.compile(r'(?:(M|R):)?(d?0xH[0-9a-fA-F]{1,8})(>=|<=|!=|=|>|<)(d?0xH[0-9a-fA-F]{1,8}|[0-9]{1,3})(?:\.([1-9][0-9]{0,3})\.)?')


def require(ok, message):
    if not ok:
        raise ValueError(message)


def unique(pairs):
    result = {}
    for k, v in pairs:
        require(k not in result, f'duplicate JSON key: {k}')
        result[k] = v
    return result


def walk(value, depth=0):
    require(depth <= 10, 'nesting limit')
    if isinstance(value, dict):
        for key, child in value.items():
            walk(key, depth + 1)
            walk(child, depth + 1)
    if isinstance(value, list):
        for child in value:
            walk(child, depth + 1)
    if isinstance(value, str):
        require(unicodedata.normalize('NFC', value) == value and
                all(ord(c) >= 32 and not 127 <= ord(c) <= 159 and ord(c) & 0xffff not in (0xfffe, 0xffff) for c in value),
                'non-NFC/control string')
        value.encode('utf-8', errors='strict')


def reject_number(value):
    raise ValueError(f'noninteger numeric value: {value}')


def parse_integer(value):
    require(value != '-0' and len(value) <= 16, 'integer representation limit')
    return int(value)


def load(path):
    require(path.is_file() and not path.is_symlink() and path.stat().st_size <= LIMIT,
            f'file limit/type: {path}')
    text = path.read_bytes().decode('utf-8')
    obj = json.loads(text, object_pairs_hook=unique, parse_float=reject_number, parse_int=parse_integer,
                     parse_constant=reject_number)
    walk(obj)
    return obj


def canon(obj):
    return json.dumps(obj, sort_keys=True, ensure_ascii=False, allow_nan=False,
                      separators=(',', ':')).encode('utf-8')


def digest(obj):
    return hashlib.sha256(canon(obj)).hexdigest()


def event_id(scope, achievement):
    return digest({'achievement_id': achievement, 'scope': scope})


def set_key(item):
    return item['namespace'], item['set_id'], item['revision']


def content_key(content):
    return content['console_id'], content['method'], content['sha256']


def safe_path(value):
    require(0 < len(value.encode('utf-8')) <= 512, 'path byte length')
    reserved = {'con', 'prn', 'aux', 'nul'} | {
        f'{kind}{index}' for kind in ('com','lpt') for index in range(1,10)
    }
    for part in value.split('/'):
        require(bool(part) and part not in {'.','..'} and
                len(part.encode('utf-8')) <= 255 and part[-1] not in ' .' and
                part.split('.', 1)[0].lower() not in reserved and
                not any(c in part for c in '\\:*?"<>|'),
                f'unsafe path: {value}')


def inside(sd, path):
    safe_path(str(path))
    target = sd
    for component in path.parts:
        target = target / component
        require(not target.is_symlink(), f'symlink path: {target}')
    return target


def rom_hash(path, maximum=256*1024*1024):
    require(path.is_file() and path.stat().st_size <= maximum, f'ROM file/type: {path}')
    h = hashlib.sha256()
    with path.open('rb') as f:
        while data := f.read(65536):
            h.update(data)
    return h.hexdigest()


def asset(sd, sha, role):
    p = inside(sd, SD_ROOT / 'assets/v1' / (sha+'.png'))
    require(rom_hash(p, 128*1024) == sha, 'asset digest')
    data = p.read_bytes()
    require(len(data) >= 45 and data.startswith(b'\x89PNG\r\n\x1a\n'), 'asset signature')
    size, kind, w, h, depth, color, compression, filter_method, interlace = struct.unpack(
        '>I4sIIBBBBB', data[8:29])
    edge = 96 if role == 'avatar' else 64
    require(size == 13 and kind == b'IHDR' and 0 < w <= edge and 0 < h <= edge and
            depth == 8 and color in (2, 6) and (compression, filter_method, interlace) == (0,0,0),
            'asset dimensions/encoding')
    channels = 4 if color == 6 else 3
    positions, compressed, terminated = 8, bytearray(), False
    while positions + 12 <= len(data):
        n, k = struct.unpack_from('>I4s', data, positions)
        end = positions + 8 + n
        require(end + 4 <= len(data), 'PNG chunk bounds')
        body = data[positions+8:end]
        require(zlib.crc32(k+body) == struct.unpack_from('>I', data, end)[0], 'PNG chunk CRC')
        positions = end + 4
        if k == b'IDAT':
            compressed.extend(body)
        if k == b'IEND':
            terminated = not body
            break
        require(k in (b'IHDR',b'IDAT'), 'unsupported PNG chunk')
    require(terminated and positions == len(data), 'PNG termination')
    z = zlib.decompressobj()
    stride = w * channels + 1
    pixels = z.decompress(compressed, stride*h+1)
    require(len(pixels) == stride*h and z.eof and not z.unused_data and
            all(pixels[row*stride] <= 4 for row in range(h)), 'PNG pixel payload')


def examine(condition, caps, requires, checker):
    parts = condition.split('_')
    require(len(parts) <= 32, 'condition term budget')
    for term in parts:
        match = CONDITION.fullmatch(term)
        require(match is not None, 'condition outside the approved capability subset')
        flag, left, _, right, hits = match.groups()
        actual = {'byte'}
        if flag:
            actual.add({'M':'measured','R':'reset-if'}[flag])
        if hits:
            actual.add('hitcount')
        for part in (left, right):
            if part.startswith('d'):
                actual.add('delta')
            if '0xH' not in part:
                require(int(part) <= 255, 'byte literal')
                continue
            address = int(part.split('0xH')[1], 16)
            require(any(r['start'] <= address < r['start']+r['length'] for r in caps['regions']),
                    'memory address outside provider regions')
        require(actual <= requires, 'condition uses undeclared feature')
    result = subprocess.run([str(checker), condition], capture_output=True, text=True, timeout=5)
    require(result.returncode == 0 and result.stdout.strip().isdigit(),
            f'rcheevos rejected condition: {result.stderr}')
    return int(result.stdout)


def names(values):
    require(len(set(values)) == len(values), 'duplicate identifier')
    return set(values)


def validate(sd, checker):
    base = inside(sd, SD_ROOT)
    paths = set()
    groups = {k:[] for k in ('identity','metadata','definition','profile','selection','capability','progress')}
    for folder, kind in [('identities','identity'),('metadata','metadata'),('definitions','definition'),
                         ('players','profile'),('capabilities','capability'),('progress','progress')]:
        tree = inside(sd, SD_ROOT / folder / 'v1')
        for p in sorted(tree.rglob('*.json')):
            require(len(groups[kind]) < 4096, 'reference scan limit')
            require(inside(sd, p.relative_to(sd)) == p, 'outside SD')
            folded = p.relative_to(sd).as_posix().casefold()
            require(folded not in paths, 'FAT path collision')
            paths.add(folded)
            groups[kind].append((p, load(p)))
    selected = base/'players/v1/selected.json'
    groups['selection'] = [(p,o) for p,o in groups['profile'] if p==selected]
    groups['profile'] = [(p,o) for p,o in groups['profile'] if p!=selected]
    for kind, rows in groups.items():
        for p, obj in rows:
            VALIDATOR.validate(obj)
            require(obj['kind'] == kind, f'kind/path mismatch: {p}')
            if 'sha256' in obj:
                require(obj['sha256'] == digest(obj['payload']), f'integrity mismatch: {p}')
    profiles, identities, defs, metadata, caps = {}, {}, {}, {}, []
    for p,o in groups['profile']:
        x=o['payload']
        require(p==base/'players/v1'/f"{x['profile_id']}.json", 'profile path')
        profiles[x['profile_id']]=x
        if 'avatar_sha256' in x:
            asset(sd,x['avatar_sha256'],'avatar')
    for _,o in groups['selection']:
        require(o['payload']['profile_id'] in profiles,'unknown selected profile')
    for p,o in groups['identity']:
        x=o['payload']
        key=(x['game_id'],content_key(x['content']))
        require(key not in identities, 'duplicate identity')
        require(p==base/'identities/v1'/x['game_id']/f"{digest(x['content'])}.json", 'identity path')
        safe_path(x['rom_path'])
        if 'archive_member' in x:
            safe_path(x['archive_member'])
        require(x['content']['method']=='raw-sha256-v1' and 'archive_member' not in x and
                'components' not in x, 'reference verifier only handles unarchived raw ROMs')
        require(rom_hash(inside(sd,Path(x['rom_path'])))==x['content']['sha256'],'wrong ROM identity')
        identities[key]=x
    sd_game_ids={g for g,_ in identities}
    for p,o in groups['metadata']:
        x=o['payload']
        key=(x['game_id'],*set_key(x['set']))
        require(p==base/'metadata/v1'/x['game_id']/key[1]/key[2]/f'{key[3]}.json','metadata path')
        require(x['game_id'] in sd_game_ids and key not in metadata,'invalid metadata association')
        metadata[key]=names([a['id'] for a in x['achievements']])
        for a in x['achievements']:
            if 'badge_sha256' in a:
                asset(sd,a['badge_sha256'],'badge')
    for p,o in groups['capability']:
        x=o['payload']
        require(p==base/'capabilities/v1'/f"{x['core_sha256']}.json",'capability path')
        require(x['rcheevos_revision']==PIN,'unverified parser pin')
        require(x['provider']=='nes-system-ram-v1' and x['content_console_id']==7 and
                x['core_id']=='fceumm' and x['regions']==[{'start':0,'length':2048}],
                'reference verifier rejects unregistered memory provider')
        names(x['features'])
        names(x['definition_versions'])
        last=0
        for entry in x['regions']:
            require(entry['start']>=last,'overlapping or unordered regions')
            last=entry['start']+entry['length']
            require(last<=2**32,'memory region overflow')
        if x['evidence']=='physical-legacy-only':
            require(not x['generalized_enabled'],'false generalized capability claim')
        caps.append(x)
    for p,o in groups['definition']:
        x=o['payload']
        key=set_key(x['set'])
        require(p==base/'definitions/v1'/key[0]/key[1]/str(key[2])/f"{digest(x['content'])}.json",'definition path')
        require((x['game_id'],content_key(x['content'])) in identities,'definition identity mismatch')
        require(x['authorization']['mode']=='original-local' and key[0].startswith(('diagnostic.','local.')) and 'ra' not in x,
                'no authorized official source in v1 reference policy')
        required=names(x['requires']['features'])
        matches=[c for c in caps if c['provider']==x['requires']['provider'] and
                 c['content_console_id']==x['content']['console_id'] and 1 in c['definition_versions'] and required<=set(c['features'])]
        require(len(matches)==1,'missing or ambiguous reference capability')
        ids=names([a['id'] for a in x['achievements']])
        meta=metadata.get((x['game_id'],*key))
        if meta is not None:
            require(meta==ids,'metadata and definition IDs disagree')
        require(sum(examine(a['condition'],matches[0],required,checker) for a in x['achievements'])<=128*1024,
                'parse allocation budget')
        defs[o['sha256']]=x
    total=set()
    for p,o in groups['progress']:
        x=o['payload']
        scope=x['scope']
        require(p in {base/'progress/v1'/scope['profile_id']/digest(scope)/f'{slot}.json' for slot in ('a','b')},
                'progress path')
        require(scope['profile_id'] in profiles,'unknown progress profile')
        definition=defs.get(scope['definition_sha256'])
        require(definition is not None,'missing definition for fixture')
        require(scope['set']==definition['set'] and scope['content']==definition['content'],'progress revision/identity mismatch')
        ids={a['id'] for a in definition['achievements']}
        ids_in_order = [a['achievement_id'] for a in x['events']]
        names(ids_in_order)
        require(ids_in_order == sorted(ids_in_order), 'unsorted snapshot events')
        require(x['generation'] >= bool(x['events']), 'nonempty generation zero')
        for evt in x['events']:
            require(evt['achievement_id'] in ids and evt['event_id']==event_id(scope,evt['achievement_id']),
                    'progress event identity')
            require(('unlocked_at' in evt)==(evt['timestamp_status']=='trusted'),'timestamp status mismatch')
            total.add(evt['event_id'])
    require(bool(identities) and bool(defs),'empty fixture')
    return len(total)


def main():
    ap=argparse.ArgumentParser(description=__doc__)
    ap.add_argument('sd',type=Path)
    ap.add_argument('--checker',type=Path,required=True)
    args=ap.parse_args()
    try:
        print(f'PASS contract fixture: {validate(args.sd.resolve(),args.checker.resolve())} distinct durable unlocks')
    except (ValueError, OSError, RecursionError, subprocess.TimeoutExpired) as error:
        print(f'INVALID: {error}',file=sys.stderr)
        return 1
    except Exception as error:
        from jsonschema.exceptions import ValidationError
        if not isinstance(error, ValidationError):
            raise
        print(f'INVALID schema: {error.message}',file=sys.stderr)
        return 1
    return 0


if __name__=='__main__':
    sys.exit(main())
