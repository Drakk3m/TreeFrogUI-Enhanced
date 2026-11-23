#define _POSIX_C_SOURCE 200809L
#include "ra_game.h"

#include <ctype.h>
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

enum { RA_IDENTITY_MAX = 2 * 1024 * 1024, RA_PACK_MAX = 2 * 1024 * 1024,
       RA_JSON_DEPTH_MAX = 32 };

typedef struct { const unsigned char *p, *end; } Json;

static void ws(Json *j) {
    while (j->p < j->end && (*j->p == ' ' || *j->p == '\t' ||
           *j->p == '\r' || *j->p == '\n')) j->p++;
}

static bool take(Json *j, unsigned char c) {
    ws(j);
    if (j->p >= j->end || *j->p != c) return false;
    j->p++;
    return true;
}

static int hex(unsigned char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

static bool put_utf8(char *out, size_t cap, size_t *n, unsigned c) {
    size_t need = c < 0x80 ? 1 : c < 0x800 ? 2 : c < 0x10000 ? 3 : 4;
    if (*n + need >= cap || c > 0x10ffff || (c >= 0xd800 && c <= 0xdfff)) return false;
    if (need == 1) out[(*n)++] = (char)c;
    else if (need == 2) { out[(*n)++] = (char)(0xc0 | (c >> 6)); out[(*n)++] = (char)(0x80 | (c & 63)); }
    else if (need == 3) { out[(*n)++] = (char)(0xe0 | (c >> 12)); out[(*n)++] = (char)(0x80 | ((c >> 6) & 63)); out[(*n)++] = (char)(0x80 | (c & 63)); }
    else { out[(*n)++] = (char)(0xf0 | (c >> 18)); out[(*n)++] = (char)(0x80 | ((c >> 12) & 63)); out[(*n)++] = (char)(0x80 | ((c >> 6) & 63)); out[(*n)++] = (char)(0x80 | (c & 63)); }
    out[*n] = 0;
    return true;
}

static bool string(Json *j, char *out, size_t cap) {
    size_t n = 0;
    ws(j);
    if (j->p >= j->end || *j->p++ != '"') return false;
    for (;;) {
        if (j->p >= j->end) return false;
        unsigned c = *j->p++;
        if (c == '"') { if (n >= cap) return false; out[n] = 0; return true; }
        if (c < 0x20) return false;
        if (c == '\\') {
            if (j->p >= j->end) return false;
            c = *j->p++;
            if (c == '"' || c == '\\' || c == '/') { }
            else if (c == 'b') c = '\b';
            else if (c == 'f') c = '\f';
            else if (c == 'n') c = '\n';
            else if (c == 'r') c = '\r';
            else if (c == 't') c = '\t';
            else if (c == 'u') {
                unsigned v = 0;
                for (int i = 0; i < 4; i++) { if (j->p >= j->end || hex(*j->p) < 0) return false; v = (v << 4) | (unsigned)hex(*j->p++); }
                if (v >= 0xd800 && v <= 0xdbff) {
                    if (j->end - j->p < 6 || j->p[0] != '\\' || j->p[1] != 'u') return false;
                    j->p += 2; unsigned lo = 0;
                    for (int i = 0; i < 4; i++) { if (j->p >= j->end || hex(*j->p) < 0) return false; lo = (lo << 4) | (unsigned)hex(*j->p++); }
                    if (lo < 0xdc00 || lo > 0xdfff) return false;
                    v = 0x10000 + ((v - 0xd800) << 10) + lo - 0xdc00;
                } else if (v >= 0xdc00) return false;
                if (!v || !put_utf8(out, cap, &n, v)) return false;
                continue;
            } else return false;
        }
        if (c >= 0x80) {
            /* Preserve UTF-8 bytes after checking the complete sequence. */
            size_t width = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc0 ? 2 : 0;
            if (!width || c > (width == 2 ? 0xdf : width == 3 ? 0xef : 0xf4) ||
                j->end - j->p < (ptrdiff_t)(width - 1) || n + width >= cap) return false;
            unsigned code = width == 2 ? c & 31 : width == 3 ? c & 15 : c & 7;
            for (size_t i = 1; i < width; i++) {
                if ((j->p[i - 1] & 0xc0) != 0x80) return false;
                code = (code << 6) | (j->p[i - 1] & 63);
            }
            if ((width == 2 && code < 0x80) || (width == 3 && code < 0x800) ||
                (width == 4 && code < 0x10000) || code > 0x10ffff ||
                (code >= 0xd800 && code <= 0xdfff)) return false;
            out[n++] = (char)c;
            for (size_t i = 1; i < width; i++) out[n++] = (char)j->p[i - 1];
            j->p += width - 1; out[n] = 0; continue;
        }
        if (!put_utf8(out, cap, &n, c)) return false;
    }
}

static bool integer(Json *j, int *out) {
    char buf[32]; size_t n = 0; ws(j);
    if (j->p < j->end && *j->p == '-') buf[n++] = (char)*j->p++;
    size_t digits = n;
    while (j->p < j->end && isdigit(*j->p)) { if (n + 1 >= sizeof buf) return false; buf[n++] = (char)*j->p++; }
    if (n == digits) return false;
    buf[n] = 0;
    char *end;
    errno = 0;
    long v = strtol(buf, &end, 10);
    if (errno == ERANGE || *end || v < 0 || v > 2147483647L) return false;
    *out = (int)v; return true;
}

static bool integer64(Json *j, long long *out) {
    char buf[32]; size_t n = 0; ws(j);
    if (j->p < j->end && *j->p == '-') buf[n++] = (char)*j->p++;
    size_t digits = n;
    while (j->p < j->end && isdigit(*j->p)) {
        if (n + 1 >= sizeof buf) return false;
        buf[n++] = (char)*j->p++;
    }
    if (n == digits) return false;
    buf[n] = 0;
    char *end;
    errno = 0;
    long long v = strtoll(buf, &end, 10);
    if (errno == ERANGE || *end || v < 0) return false;
    *out = v; return true;
}

static bool skip(Json *j, int depth);
static bool skip_string(Json *j) {
    ws(j); if (j->p >= j->end || *j->p++ != '"') return false;
    for (;;) {
        if (j->p >= j->end) return false;
        unsigned c = *j->p++;
        if (c == '"') return true;
        if (c < 0x20) return false;
        if (c == '\\') {
            if (j->p >= j->end) return false;
            c = *j->p++;
            if (c == 'u') { for (int i = 0; i < 4; i++) if (j->p >= j->end || hex(*j->p++) < 0) return false; }
            else if (c != '"' && c != '\\' && c != '/' && c != 'b' && c != 'f' && c != 'n' && c != 'r' && c != 't') return false;
        }
    }
}
static bool literal(Json *j, const char *value) {
    size_t size = strlen(value);
    if ((size_t)(j->end - j->p) < size || memcmp(j->p, value, size)) return false;
    j->p += size;
    return true;
}

static bool skip(Json *j, int depth) {
    if (depth > RA_JSON_DEPTH_MAX) return false;
    ws(j);
    if (j->p >= j->end) return false;
    if (*j->p == '"') return skip_string(j);
    if (*j->p == 't') return literal(j, "true");
    if (*j->p == 'f') return literal(j, "false");
    if (*j->p == 'n') return literal(j, "null");

    bool object = *j->p == '{';
    if (object || *j->p == '[') {
        unsigned char end = object ? '}' : ']';
        j->p++;
        if (take(j, end)) return true;
        do {
            if (object) {
                ws(j);
                if (!skip_string(j) || !take(j, ':')) return false;
            }
            if (!skip(j, depth + 1)) return false;
            if (take(j, end)) return true;
        } while (take(j, ','));
        return false;
    }
    long long number;
    return integer64(j, &number);
}

static bool read_file(const char *path, unsigned char **data, size_t *size, size_t limit) {
    int fd = open(path, O_RDONLY | O_NOFOLLOW | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) return false;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        (uintmax_t)st.st_size > limit) {
        close(fd);
        return false;
    }
    unsigned char *buffer = malloc((size_t)st.st_size);
    if (!buffer) { close(fd); return false; }
    size_t used = 0;
    while (used < (size_t)st.st_size) {
        ssize_t got = read(fd, buffer + used, (size_t)st.st_size - used);
        if (got < 0 && errno == EINTR) continue;
        if (got <= 0) { free(buffer); close(fd); return false; }
        used += (size_t)got;
    }
    unsigned char extra;
    ssize_t trailing;
    do { trailing = read(fd, &extra, 1); } while (trailing < 0 && errno == EINTR);
    close(fd);
    if (trailing != 0) { free(buffer); return false; }
    *data = buffer;
    *size = used;
    return true;
}

static bool hash_ok(const char *hash) {
    if (strlen(hash) != 32) return false;
    for (size_t i = 0; i < 32; i++)
        if (hex((unsigned char)hash[i]) < 0 || (hash[i] >= 'A' && hash[i] <= 'F'))
            return false;
    return true;
}

static bool parse_achievement(Json *j, RaGameAchievement *achievement) {
    if (!take(j, '{')) return false;
    do {
        char key[64];
        if (!string(j, key, sizeof key) || !take(j, ':')) return false;
        if (!strcmp(key, "id")) {
            if (achievement->id || !integer(j, &achievement->id) ||
                achievement->id <= 0) return false;
        } else if (!strcmp(key, "title")) {
            if (achievement->title[0] ||
                !string(j, achievement->title, sizeof achievement->title)) return false;
        } else if (!strcmp(key, "description")) {
            if (!string(j, achievement->description, sizeof achievement->description))
                return false;
        } else if (!strcmp(key, "points")) {
            if (!integer(j, &achievement->points)) return false;
        } else if (!strcmp(key, "badge")) {
            if (!string(j, achievement->badge, sizeof achievement->badge)) return false;
        } else if (!skip(j, 0)) return false;

        if (take(j, '}'))
            return achievement->id > 0 && achievement->title[0];
    } while (take(j, ','));
    return false;
}

static bool parse_achievements(Json *j, RaGame *out) {
    if (!take(j, '[')) return false;
    size_t count = 0;
    do {
        if (count >= RA_GAME_MAX_ACHIEVEMENTS) return false;
        RaGameAchievement achievement = {0};
        if (!parse_achievement(j, &achievement)) return false;
        for (size_t i = 0; i < count; i++)
            if (out->achievements[i].id == achievement.id) return false;
        out->achievements[count++] = achievement;
        if (take(j, ']')) {
            out->achievement_count = count;
            return true;
        }
    } while (take(j, ','));
    return false;
}

static bool parse_pack(const unsigned char *data, size_t size, int console, int game,
                       const char *hash, RaGame *out) {
    Json j = {data, data + size};
    int schema = -1, pack_console = -1, pack_game = -1;
    char pack_hash[33] = "";
    if (!take(&j, '{')) return false;
    do {
        char key[64];
        if (!string(&j, key, sizeof key) || !take(&j, ':')) return false;
        if (!strcmp(key, "schema_version")) {
            if (schema >= 0 || !integer(&j, &schema)) return false;
        } else if (!strcmp(key, "console_id")) {
            if (pack_console >= 0 || !integer(&j, &pack_console)) return false;
        } else if (!strcmp(key, "game_id")) {
            if (pack_game >= 0 || !integer(&j, &pack_game)) return false;
        } else if (!strcmp(key, "ra_hash")) {
            if (pack_hash[0] || !string(&j, pack_hash, sizeof pack_hash)) return false;
        } else if (!strcmp(key, "achievements")) {
            if (out->achievement_count || !parse_achievements(&j, out)) return false;
        } else if (!skip(&j, 0)) return false;

        if (take(&j, '}')) {
            ws(&j);
            return j.p == j.end && schema == 1 && pack_console == console &&
                   pack_game == game && !strcmp(pack_hash, hash) && out->achievement_count > 0;
        }
    } while (take(&j, ','));
    return false;
}

typedef struct {
    int console_id;
    int game_id;
    long long size;
    long long mtime_ns;
    char hash[33];
} RaIdentity;

static bool read_identity(Json *j, const char *platform, RaIdentity *identity) {
    char folder[64] = "", method[64] = "";
    if (!take(j, '{')) return false;
    do {
        char key[64];
        if (!string(j, key, sizeof key) || !take(j, ':')) return false;
        if (!strcmp(key, "platform")) {
            if (folder[0] || !string(j, folder, sizeof folder)) return false;
        } else if (!strcmp(key, "console_id")) {
            if (identity->console_id || !integer(j, &identity->console_id) ||
                identity->console_id <= 0) return false;
        } else if (!strcmp(key, "game_id")) {
            if (identity->game_id || !integer(j, &identity->game_id) ||
                identity->game_id <= 0) return false;
        } else if (!strcmp(key, "ra_hash")) {
            if (identity->hash[0] || !string(j, identity->hash, sizeof identity->hash))
                return false;
        } else if (!strcmp(key, "size")) {
            if (identity->size >= 0 || !integer64(j, &identity->size) ||
                identity->size < 0) return false;
        } else if (!strcmp(key, "mtime_ns")) {
            if (identity->mtime_ns >= 0 || !integer64(j, &identity->mtime_ns) ||
                identity->mtime_ns <= 0) return false;
        } else if (!strcmp(key, "method")) {
            if (method[0] || !string(j, method, sizeof method)) return false;
        } else if (!skip(j, 0)) return false;

        if (take(j, '}'))
            return !strcmp(folder, platform) && identity->console_id > 0 &&
                   identity->game_id > 0 && identity->size >= 0 && identity->mtime_ns > 0 &&
                   hash_ok(identity->hash) &&
                   (!strcmp(method, "rcheevos") || !strcmp(method, "rcheevos_compatible"));
    } while (take(j, ','));
    return false;
}

static bool parse_identity(const unsigned char *data, size_t size, const char *relative,
                           const char *platform, RaIdentity *identity) {
    Json j = { data, data + size };
    if (!take(&j, '{')) return false;
    bool found = false;
    do {
        char key[1024];
        if (!string(&j, key, sizeof key) || !take(&j, ':')) return false;
        if (!strcmp(key, relative)) {
            if (found || !read_identity(&j, platform, identity)) return false;
            found = true;
        } else if (!skip(&j, 0)) return false;
        if (take(&j, '}')) {
            ws(&j);
            return found && j.p == j.end;
        }
    } while (take(&j, ','));
    return false;
}

bool ra_game_read(const char *root, const char *rom_path, const char *platform, RaGame *out) {
    if (!root || !*root || !rom_path || !platform || !*platform || !out) return false;
    out->achievement_count = 0;
    size_t root_length = strlen(root);
    if (strncmp(rom_path, root, root_length) || rom_path[root_length] != '/')
        return false;
    const char *relative = rom_path + root_length + 1;
    if (!*relative) return false;

    char path[1024];
    int written = snprintf(path, sizeof path, "%s/cubegm/enhanced/ra/rom_identity.json", root);
    unsigned char *data;
    size_t size;
    if (written < 0 || (size_t)written >= sizeof path ||
        !read_file(path, &data, &size, RA_IDENTITY_MAX)) return false;
    RaIdentity identity = { .size = -1, .mtime_ns = -1 };
    bool matched = parse_identity(data, size, relative, platform, &identity);
    free(data);
    if (!matched) return false;

    struct stat st;
    if (stat(rom_path, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size != identity.size)
        return false;
    /* Studio copies ROMs with copy2. Allow FAT's two-second timestamp granularity. */
    long long exported_seconds = identity.mtime_ns / 1000000000LL;
    long long observed_seconds = st.st_mtime;
    if (observed_seconds < exported_seconds - 2 || observed_seconds > exported_seconds + 2)
        return false;
    written = snprintf(path, sizeof path, "%s/cubegm/enhanced/ra/cache/%d/%s/gamepack.json",
                       root, identity.console_id, identity.hash);
    if (written < 0 || (size_t)written >= sizeof path ||
        !read_file(path, &data, &size, RA_PACK_MAX)) return false;
    bool valid = parse_pack(data, size, identity.console_id, identity.game_id, identity.hash, out);
    free(data);
    if (!valid) { out->achievement_count = 0; return false; }
    out->console_id = identity.console_id;
    out->game_id = identity.game_id;
    memcpy(out->ra_hash, identity.hash, sizeof out->ra_hash);
    return true;
}
