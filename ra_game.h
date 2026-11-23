#ifndef FROGUI_RA_GAME_H
#define FROGUI_RA_GAME_H

#include <stdbool.h>
#include <stddef.h>

#define RA_GAME_MAX_ACHIEVEMENTS 256
#define RA_GAME_TITLE_MAX 300
#define RA_GAME_DESCRIPTION_MAX 768
#define RA_GAME_BADGE_MAX 128

typedef struct {
    int id;
    int points;
    char title[RA_GAME_TITLE_MAX + 1];
    char description[RA_GAME_DESCRIPTION_MAX + 1];
    char badge[RA_GAME_BADGE_MAX + 1];
} RaGameAchievement;

typedef struct {
    int console_id;
    int game_id;
    char ra_hash[33];
    size_t achievement_count;
    RaGameAchievement achievements[RA_GAME_MAX_ACHIEVEMENTS];
} RaGame;

/* Load one hash-bound, metadata-only game pack from an SD root. The ROM path
 * must be inside root and match rom_identity.json. No writes or networking.
 * The ROM content hash is not recomputed here. */
bool ra_game_read(const char *root, const char *rom_path, const char *platform,
                  RaGame *out);

#endif
