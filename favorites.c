#include "favorites.h"
#include <stdio.h>
#include <string.h>

#define FAVORITES_FILE "/mnt/sdcard/frogui/favorites.txt"

// Favorites state
static FavoriteGame favorites[MAX_FAVORITES];
static int favorite_count = 0;

static int favorite_index_by_path(const char *full_path) {
    if (!full_path || !*full_path) return -1;
    for (int i = 0; i < favorite_count; i++)
        if (strcmp(favorites[i].full_path, full_path) == 0) return i;
    return -1;
}

static void favorite_set_text(char *destination, size_t capacity, const char *source) {
    if (!capacity) return;
    snprintf(destination, capacity, "%s", source ? source : "");
}

static void favorite_set_display(FavoriteGame *favorite) {
    snprintf(favorite->display_name, sizeof favorite->display_name, "%.*s (%.*s)",
             (int)sizeof favorite->game_name - 4, favorite->game_name,
             (int)sizeof favorite->core_name - 1, favorite->core_name);
}

static bool favorite_add(const char *core_name, const char *game_name, const char *full_path) {
    if (favorite_count >= MAX_FAVORITES) return false;
    FavoriteGame *favorite = &favorites[favorite_count];
    memset(favorite, 0, sizeof *favorite);
    favorite_set_text(favorite->core_name, sizeof favorite->core_name, core_name);
    favorite_set_text(favorite->game_name, sizeof favorite->game_name, game_name);
    favorite_set_text(favorite->full_path, sizeof favorite->full_path, full_path);
    favorite_set_display(favorite);
    favorite_count++;
    favorites_save();
    return true;
}

void favorites_init(void) {
    favorite_count = 0;
    favorites_load();
}

void favorites_load(void) {
    FILE *fp = fopen(FAVORITES_FILE, "r");
    if (!fp) {
        favorite_count = 0;
        return;
    }

    favorite_count = 0;
    char line[sizeof ((FavoriteGame *)0)->core_name +
              sizeof ((FavoriteGame *)0)->game_name + sizeof ((FavoriteGame *)0)->full_path + 2];

    while (fgets(line, sizeof(line), fp) && favorite_count < MAX_FAVORITES) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;

        // Parse line: "core_name|game_name|full_path"
        char *separator1 = strchr(line, '|');
        if (separator1) {
            *separator1 = '\0';
            char *separator2 = strchr(separator1 + 1, '|');
            if (separator2) {
                *separator2 = '\0';
                memset(&favorites[favorite_count], 0, sizeof favorites[favorite_count]);
                favorite_set_text(favorites[favorite_count].core_name,
                                  sizeof favorites[favorite_count].core_name, line);
                favorite_set_text(favorites[favorite_count].game_name,
                                  sizeof favorites[favorite_count].game_name, separator1 + 1);
                favorite_set_text(favorites[favorite_count].full_path,
                                  sizeof favorites[favorite_count].full_path, separator2 + 1);

                // Create display name
                favorite_set_display(&favorites[favorite_count]);

                favorite_count++;
            }
        }
    }

    fclose(fp);
}

void favorites_save(void) {
    FILE *fp = fopen(FAVORITES_FILE, "w");
    if (!fp) return;

    for (int i = 0; i < favorite_count; i++) {
        fprintf(fp, "%s|%s|%s\n", favorites[i].core_name, favorites[i].game_name, favorites[i].full_path);
    }

    fclose(fp);
}

bool favorites_toggle(const char *core_name, const char *game_name, const char *full_path) {
    // Check if already favorited
    int existing_index = -1;
    for (int i = 0; i < favorite_count; i++) {
        if (strcmp(favorites[i].core_name, core_name) == 0 &&
            strcmp(favorites[i].game_name, game_name) == 0) {
            existing_index = i;
            break;
        }
    }

    if (existing_index >= 0) {
        // Remove from favorites
        for (int i = existing_index; i < favorite_count - 1; i++) {
            favorites[i] = favorites[i + 1];
        }
        favorite_count--;
        favorites_save();
        return false; // Removed
    } else {
        // Add to favorites
        if (favorite_count >= MAX_FAVORITES) {
            return false; // List full
        }

        return favorite_add(core_name, game_name, full_path); // Added
    }
}

bool favorites_toggle_path(const char *core_name, const char *game_name, const char *full_path) {
    if (!full_path || !*full_path || strlen(full_path) >= sizeof favorites[0].full_path ||
        strpbrk(full_path, "|\r\n")) return false;
    int existing_index = favorite_index_by_path(full_path);
    if (existing_index >= 0) {
        favorites_remove_by_index(existing_index);
        return false;
    }
    return favorite_add(core_name, game_name, full_path);
}

bool favorites_is_favorited_path(const char *full_path) {
    return favorite_index_by_path(full_path) >= 0;
}

bool favorites_remove_by_path(const char *full_path) {
    int index = favorite_index_by_path(full_path);
    return index >= 0 && favorites_remove_by_index(index);
}

bool favorites_remove_by_index(int index) {
    if (index < 0 || index >= favorite_count) {
        return false;
    }

    // Shift all entries after the removed one
    for (int i = index; i < favorite_count - 1; i++) {
        favorites[i] = favorites[i + 1];
    }
    favorite_count--;
    favorites_save();
    return true;
}

bool favorites_is_favorited(const char *core_name, const char *game_name) {
    for (int i = 0; i < favorite_count; i++) {
        if (strcmp(favorites[i].core_name, core_name) == 0 &&
            strcmp(favorites[i].game_name, game_name) == 0) {
            return true;
        }
    }
    return false;
}

const FavoriteGame* favorites_get_list(void) {
    return favorites;
}

int favorites_get_count(void) {
    return favorite_count;
}
