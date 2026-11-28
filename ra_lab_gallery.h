#ifndef FROGUI_RA_LAB_GALLERY_H
#define FROGUI_RA_LAB_GALLERY_H
#include "ra_lab_fixture.h"
#include "ra_lab_original_fixture.h"

#define RA_LAB_RELATIVE "/cubegm/enhanced/ra/local/r36sx-lab"

static unsigned ra_lab_progress;
static char ra_lab_root[512];
static bool details_achievements_local;

static bool ra_lab_file_equal(const char *path, const void *expected, size_t length) {
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;
    struct stat info;
    bool equal = fstat(fd, &info) == 0 && S_ISREG(info.st_mode) &&
                 info.st_size == (off_t)length;
    const unsigned char *data = expected;
    unsigned char block[512];
    while (equal && length) {
        size_t bytes = length < sizeof block ? length : sizeof block;
        equal = read(fd, block, bytes) == (ssize_t)bytes && !memcmp(block, data, bytes);
        length -= bytes;
        data += bytes;
    }
    close(fd);
    return equal;
}

static unsigned ra_lab_read_progress(const char *path, bool original) {
    char data[128], expected[128];
    int n = snprintf(expected, sizeof expected,
        "R36SX-LAB-PROGRESS 1\nlocal-player\nr36sx-lab\nNES\n%s\n1\n",
        original ? RA_LAB_ORIGINAL_SHA256 : RA_LAB_SHA256);
    if (n < 0 || (size_t)n >= sizeof expected) return 0;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return 0;
    struct stat info;
    ssize_t size = fstat(fd, &info) == 0 && S_ISREG(info.st_mode)
                 ? read(fd, data, sizeof data) : -1;
    close(fd);
    if (size != (ssize_t)n + 3 || memcmp(data, expected, n) || data[n + 2] != '\n')
        return 0;
    static const char hex[] = "0123456789abcdef";
    if (data[n] < '0' || data[n] > '3' || !data[n + 1]) return 0;
    const char *digit = strchr(hex, data[n + 1]);
    return digit ? (unsigned)(data[n] - '0') * 16 + (unsigned)(digit - hex) : 0;
}

static unsigned ra_lab_mask(const char *root, bool original) {
    char path[1024];
    int n = snprintf(path, sizeof path, "%s" RA_LAB_RELATIVE "/progress.v1", root);
    if (n < 0 || (size_t)n >= sizeof path) return 0;
    unsigned mask = ra_lab_read_progress(path, original);
    if (!original) {
        n = snprintf(path, sizeof path, "%s" RA_LAB_RELATIVE "/progress-readable.v1", root);
        if (n >= 0 && (size_t)n < sizeof path) mask |= ra_lab_read_progress(path, false);
    }
    return mask;
}

static int read_ra_lab_home_count(void) {
    char path[] = SDCARD_BASE RA_LAB_RELATIVE "/lab.def";
    bool original = ra_lab_file_equal(path, ra_lab_original_definitions, sizeof ra_lab_original_definitions - 1);
    if (!original && !ra_lab_file_equal(path, ra_lab_definitions, sizeof ra_lab_definitions - 1))
        return -1;
    unsigned mask = ra_lab_mask(SDCARD_BASE, original);
    int count = 0;
    for (; mask; mask >>= 1) count += mask & 1u;
    return count;
}

static bool load_ra_lab_gallery(const char *root, const char *path,
                                const char *platform, RaGame *game) {
    if (strcasecmp(platform, "nes") && strcasecmp(platform, "fc")) return false;
    if (strcmp(get_basename(path), "r36sx_achievement_lab.nes")) return false;
    char resource[1024];
    int n = snprintf(resource, sizeof resource, "%s" RA_LAB_RELATIVE "/lab.def", root);
    if (n < 0 || (size_t)n >= sizeof resource) return false;
    bool original = ra_lab_file_equal(resource, ra_lab_original_definitions, sizeof ra_lab_original_definitions - 1);
    if (!original && !ra_lab_file_equal(resource, ra_lab_definitions, sizeof ra_lab_definitions - 1)) return false;
    char *rom = malloc(RA_LAB_ROM_SIZE);
    if (!rom) return false;
    int decoded = stbi_zlib_decode_buffer(rom, RA_LAB_ROM_SIZE, (const char *)(original ? ra_lab_original_rom_zlib : ra_lab_rom_zlib),
                                          original ? sizeof ra_lab_original_rom_zlib : sizeof ra_lab_rom_zlib);
    bool match = decoded == RA_LAB_ROM_SIZE && ra_lab_file_equal(path, rom, RA_LAB_ROM_SIZE);
    free(rom);
    if (!match) return false;

    static const char *const titles[] = {"First Coin", "Coin Collector", "Next Chapter",
        "Monster Hunter", "Untouchable", "Laboratory Master"};
    static const char *const descriptions[] = {"Collect your first coin.",
        "Collect ten coins.", "Advance from level one to level two.",
        "Defeat five enemies.", "Defeat three enemies without damage.",
        "Reach level three with at least twenty coins and full health."};
    static const int points[] = {1, 5, 5, 5, 10, 10};
    game->console_id = 7;
    game->game_id = 0;
    game->ra_hash[0] = 0;
    game->achievement_count = sizeof points / sizeof *points;
    for (size_t i = 0; i < game->achievement_count; i++) {
        RaGameAchievement *achievement = &game->achievements[i];
        achievement->id = (int)i + 1;
        achievement->points = points[i];
        strcpy(achievement->title, titles[i]);
        strcpy(achievement->description, descriptions[i]);
        snprintf(achievement->badge, sizeof achievement->badge, "lab-%zu", i + 1);
    }
    ra_lab_progress = ra_lab_mask(root, original);
    if (snprintf(ra_lab_root, sizeof ra_lab_root, "%s", root) >= (int)sizeof ra_lab_root)
        return false;
    return true;
}
#endif
