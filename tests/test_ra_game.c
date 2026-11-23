#define _XOPEN_SOURCE 700
#include "../ra_game.h"

#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

static void mkdirs(const char *root) {
    char path[512];
    assert(snprintf(path, sizeof path, "%s/cubegm/enhanced/ra/cache/1/0123456789abcdef0123456789abcdef", root) < (int)sizeof path);
    char *p = path;
    for (; *p; p++) if (*p == '/') { *p = 0; if (*path) mkdir(path, 0700); *p = '/'; }
    assert(mkdir(path, 0700) == 0 || access(path, F_OK) == 0);
    assert(snprintf(path, sizeof path, "%s/roms/nes", root) < (int)sizeof path);
    for (p = path; *p; p++) if (*p == '/') { *p = 0; if (*path) mkdir(path, 0700); *p = '/'; }
    assert(mkdir(path, 0700) == 0 || access(path, F_OK) == 0);
}

static void write_text(const char *path, const char *text) {
    FILE *f = fopen(path, "wb"); assert(f); assert(fputs(text, f) >= 0); assert(fclose(f) == 0);
}

int main(void) {
    char root[] = "/tmp/frogui-ra-game-XXXXXX";
    assert(mkdtemp(root)); mkdirs(root);
    char rom[512], identity[512], pack[512];
    snprintf(rom, sizeof rom, "%s/roms/nes/Test.nes", root);
    write_text(rom, "rom-data");
    struct stat st; assert(stat(rom, &st) == 0);
    snprintf(identity, sizeof identity, "%s/cubegm/enhanced/ra/rom_identity.json", root);
    char identity_json[2048];
    snprintf(identity_json, sizeof identity_json,
             "{\"roms/nes/Test.nes\":{\"platform\":\"nes\",\"console_id\":1,\"game_id\":42,\"ra_hash\":\"0123456789abcdef0123456789abcdef\",\"method\":\"rcheevos\",\"size\":%lld,\"mtime_ns\":999999999999}}", (long long)st.st_size);
    write_text(identity, identity_json);
    snprintf(pack, sizeof pack, "%s/cubegm/enhanced/ra/cache/1/0123456789abcdef0123456789abcdef/gamepack.json", root);
    write_text(pack, "{\"schema_version\":1,\"console_id\":1,\"game_id\":42,\"ra_hash\":\"0123456789abcdef0123456789abcdef\",\"achievements\":[{\"id\":7,\"title\":\"First\",\"description\":\"Done\",\"points\":5,\"badge\":\"badge.png\",\"definition\":\"0=1\"}]}");
    RaGame game;
    assert(ra_game_read(root, rom, "nes", &game));
    assert(game.console_id == 1 && game.game_id == 42 && game.achievement_count == 1);
    assert(game.achievements[0].id == 7 && game.achievements[0].points == 5);
    assert(!strcmp(game.achievements[0].title, "First"));
    assert(!ra_game_read(root, rom, "snes", &game));
    write_text(rom, "replacement");
    assert(!ra_game_read(root, rom, "nes", &game));
    write_text(rom, "rom-data");
    write_text(pack, "{\"schema_version\":1,\"console_id\":1,\"game_id\":42,"
                     "\"ra_hash\":\"0123456789abcdef0123456789abcdef\","
                     "\"achievements\":[{\"id\":7,\"title\":\"First\"},"
                     "{\"id\":7,\"title\":\"Duplicate\"}]}");
    assert(!ra_game_read(root, rom, "nes", &game) && game.achievement_count == 0);
    write_text(pack, "{\"schema_version\":1,");
    assert(!ra_game_read(root, rom, "nes", &game));
    assert(unlink(pack) == 0);
    assert(!ra_game_read(root, rom, "nes", &game));
    puts("PASS ra game");
    return 0;
}
