#!/usr/bin/env python3
"""Compile the accepted FrogUI launch functions with a marker-only launch sink."""
import argparse
import re
import subprocess
from pathlib import Path
from tempfile import TemporaryDirectory

ap=argparse.ArgumentParser(description=__doc__)
ap.add_argument('frogui',type=Path,help='directory containing accepted FrogUI C source')
args=ap.parse_args()
base=args.frogui.resolve()
source=(base/'frogui_libretro.c').read_text()
def section(pattern,end):
    start=re.search(pattern,source,re.MULTILINE).start()
    return source[start:source.index(end,start)+len(end)]

constants=source[source.index('#define CORES_PATH'):source.index('/* Console → core mapping')]
functions=[
    section(r'^typedef struct \{ const char \*console_name;', '\n};'),
    section(r'^static const char\* get_core_for_folder\(', '\n}'),
    section(r'^typedef struct \{ const char \*ext; const char \*core_path;', '\n};'),
    section(r'^static const char\* get_core_for_extension\(', '\n}'),
    section(r'^static const char\* get_console_folder\(', '\n}'),
    section(r'^static void launch_by_path\([^;]+\) \{', '\n}'),
]
head=r'''
#define _POSIX_C_SOURCE 200809L
#include <assert.h>
#include <stdbool.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include "core_override.c"
#define SDCARD_BASE "/mnt/sdcard"
#define LAB_ROM_ROOT SDCARD_BASE "/roms"
#define MAX_PATH_LEN 1024
static const char *actual_core, *actual_rom;
static void request_game_launch(const char *core,const char *path) {
    actual_core=core; actual_rom=path;
}
static void request_standalone_launch(const char *core,const char *path) {
    (void)core; (void)path; assert(0);
}
static void dbg(const char *s) {(void)s;}
static bool is_ps1_folder(const char *f) {(void)f;return false;}
static bool is_psp_folder(const char *f) {(void)f;return false;}
static bool is_standalone_bin(const char *f) {(void)f;return false;}
'''
tail=r'''
static void check(const char *rom,const char *core) {
    actual_core=actual_rom=NULL;
    launch_by_path(rom);
    assert(actual_core&&actual_rom&&!strcmp(actual_core,core)&&!strcmp(actual_rom,rom));
}
int main(void) {
    const char *rom=LAB_ROM_ROOT "/nes/audit.nes";
    const char *fceu=CORES_PATH "/fceumm_libretro.so";
    check(rom,CORES_PATH "/quicknes_libretro.so");
    puts("PASS Studio roms/nes defaults to QuickNES");
    check(LAB_ROM_ROOT "/FC/r36sx_achievement_lab.nes",fceu);
    puts("PASS original lab path defaults to FCEUmm");
    strcpy(overrides[0].key,LAB_ROM_ROOT "/nes");
    strcpy(overrides[0].core,CORES_PATH "/quicknes_libretro.so");
    strcpy(overrides[1].key,rom);
    strcpy(overrides[1].core,fceu);
    override_count=2;
    check(rom,fceu);
    check(LAB_ROM_ROOT "/nes/unrelated.nes",CORES_PATH "/quicknes_libretro.so");
    puts("PASS exact per-ROM override takes precedence; unrelated title untouched");
}
'''
with TemporaryDirectory(prefix='ra-routing-') as work:
    path=Path(work)/'check.c'
    path.write_text(head+constants+'\n'.join(functions)+tail)
    subprocess.run(['cc','-g','-I',str(base),'-fsanitize=address,undefined','-no-pie',str(path),
                    '-o',str(path.with_suffix(''))],check=True)
    subprocess.run([str(path.with_suffix(''))],check=True)
