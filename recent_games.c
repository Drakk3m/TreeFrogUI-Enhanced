#include "recent_games.h"
#include <stdbool.h>
#include <stdio.h>
#include <string.h>

// Recent games state
static RecentGame recent_games[MAX_RECENT_GAMES];
static int recent_count = 0;

void recent_games_init(void) {
    recent_count = 0;
    recent_games_load();
}

void recent_games_load(void) {
    FILE *fp = fopen(HISTORY_FILE, "r");
    if (!fp) {
        recent_count = 0;
        return;
    }
    
    recent_count = 0;
    char line[sizeof ((RecentGame *)0)->core_name +
              sizeof ((RecentGame *)0)->game_name + sizeof ((RecentGame *)0)->full_path + 2];
    
    while (fgets(line, sizeof(line), fp) && recent_count < MAX_RECENT_GAMES) {
        // Remove newline
        line[strcspn(line, "\r\n")] = 0;
        
        // Parse line: "core_name|game_name|full_path"
        char *separator1 = strchr(line, '|');
        if (separator1) {
            *separator1 = '\0';
            char *separator2 = strchr(separator1 + 1, '|');
            if (separator2) {
                *separator2 = '\0';
                strncpy(recent_games[recent_count].core_name, line, sizeof(recent_games[recent_count].core_name) - 1);
                strncpy(recent_games[recent_count].game_name, separator1 + 1, sizeof(recent_games[recent_count].game_name) - 1);
                strncpy(recent_games[recent_count].full_path, separator2 + 1, sizeof(recent_games[recent_count].full_path) - 1);
                
                // Create display name
                snprintf(recent_games[recent_count].display_name, sizeof(recent_games[recent_count].display_name),
                        "%s (%s)", recent_games[recent_count].game_name, recent_games[recent_count].core_name);
                
                recent_count++;
            } else {
                // Old format fallback: "core_name|game_name" 
                strncpy(recent_games[recent_count].core_name, line, sizeof(recent_games[recent_count].core_name) - 1);
                strncpy(recent_games[recent_count].game_name, separator1 + 1, sizeof(recent_games[recent_count].game_name) - 1);
                recent_games[recent_count].full_path[0] = '\0'; // No path available
                
                snprintf(recent_games[recent_count].display_name, sizeof(recent_games[recent_count].display_name),
                        "%s (%s)", recent_games[recent_count].game_name, recent_games[recent_count].core_name);
                
                recent_count++;
            }
        }
    }
    
    fclose(fp);
}

void recent_games_save(void) {
    FILE *fp = fopen(HISTORY_FILE, "w");
    if (!fp) return;
    
    for (int i = 0; i < recent_count; i++) {
        fprintf(fp, "%s|%s|%s\n", recent_games[i].core_name, recent_games[i].game_name, recent_games[i].full_path);
    }
    
    fclose(fp);
}

void recent_games_add(const char *core_name, const char *game_name, const char *full_path) {
    int existing_index = -1;
    for (int i = 0; i < recent_count; i++) {
        const RecentGame *record = &recent_games[i];
        bool same = full_path[0] && record->full_path[0]
                  ? !strcmp(record->full_path, full_path)
                  : !strcmp(record->core_name, core_name) && !strcmp(record->game_name, game_name);
        if (same) { existing_index = i; break; }
    }
    if (existing_index < 0) {
        if (recent_count < MAX_RECENT_GAMES) recent_count++;
        existing_index = recent_count - 1;
    }
    for (int i = existing_index; i > 0; i--)
        recent_games[i] = recent_games[i - 1];
    RecentGame *top = &recent_games[0];
    snprintf(top->core_name, sizeof top->core_name, "%s", core_name);
    snprintf(top->game_name, sizeof top->game_name, "%s", game_name);
    snprintf(top->full_path, sizeof top->full_path, "%s", full_path);
    snprintf(top->display_name, sizeof top->display_name, "%s (%s)", game_name, core_name);

    recent_games_save();
}

const RecentGame* recent_games_get_list(void) {
    return recent_games;
}

int recent_games_get_count(void) {
    return recent_count;
}