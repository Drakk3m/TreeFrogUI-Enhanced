#include "navigation_model.h"

#include <stdio.h>
#include <string.h>

static const NavSetting ra_settings[] = {
    { "retroachievements", "enhanced.retroachievements",
      "enhanced.retroachievements.description", NULL, NAV_SETTING_SUBCATEGORY,
      true, NULL, 0 },
};

static const NavSetting settings_hub[] = {
    { "appearance", "settings.appearance", "enhanced.appearance.description",
      NULL, NAV_SETTING_SUBCATEGORY, true, NULL, 0 },
    { "games", "enhanced.games_emulation", "enhanced.games_emulation.description",
      NULL, NAV_SETTING_SUBCATEGORY, true, ra_settings,
      sizeof ra_settings / sizeof ra_settings[0] },
    { "controls", "enhanced.controls", "enhanced.controls.description",
      NULL, NAV_SETTING_SUBCATEGORY, true, NULL, 0 },
    { "system", "settings.system", "enhanced.system.description",
      NULL, NAV_SETTING_SUBCATEGORY, true, NULL, 0 },
    { "advanced", "enhanced.advanced", "enhanced.advanced.description",
      NULL, NAV_SETTING_SUBCATEGORY, true, NULL, 0 },
};

static bool valid_identity(const char *id) {
    return id && id[0] && strlen(id) < NAV_ID_MAX &&
           !strpbrk(id, "\r\n");
}

static bool platform_exists(const NavCatalog *catalog, const char *id) {
    return valid_identity(id) &&
           (!catalog || (catalog->platform_exists &&
                         catalog->platform_exists(catalog->context, id)));
}

static bool game_exists(const NavCatalog *catalog, const char *platform, const char *id) {
    return platform_exists(catalog, platform) && valid_identity(id) &&
           (!catalog || (catalog->game_exists &&
                         catalog->game_exists(catalog->context, platform, id)));
}

static bool ra_set_valid(const NavCatalog *catalog, const char *platform, const char *id) {
    return game_exists(catalog, platform, id) && catalog && catalog->ra_set_valid &&
           catalog->ra_set_valid(catalog->context, platform, id);
}

void nav_model_init(NavModel *model, bool enabled) {
    *model = (NavModel){ .enhanced_home = enabled };
    model->state.details_return = NAV_GAME_LIBRARY;
    nav_settings_set_hub(model, settings_hub, sizeof settings_hub / sizeof settings_hub[0]);
}

bool nav_model_set_platform(NavModel *model, const NavCatalog *catalog, const char *id) {
    if (!platform_exists(catalog, id)) return false;
    if (strcmp(model->state.platform_id, id) != 0) {
        strcpy(model->state.platform_id, id);
        model->state.game_id[0] = '\0';
    }
    return true;
}

bool nav_model_set_game(NavModel *model, const NavCatalog *catalog, const char *id) {
    if (!game_exists(catalog, model->state.platform_id, id)) return false;
    if (strcmp(model->state.game_id, id) != 0)
        strcpy(model->state.game_id, id);
    return true;
}

bool nav_settings_set_hub(NavModel *model, const NavSetting *items, size_t count) {
    if (!items && count) return false;
    model->settings_depth = 0;
    model->settings[0] = (NavSettingsLevel){ .items = items, .count = count };
    return true;
}

const NavSettingsLevel *nav_settings_level(const NavModel *model) {
    return model->state.screen == NAV_SETTINGS
           ? &model->settings[model->settings_depth] : NULL;
}

static NavEffect return_to_screen(NavModel *model, const NavCatalog *catalog, NavScreen screen) {
    NavState *state = &model->state;
    if (screen == NAV_RECENTS || screen == NAV_APPS || screen == NAV_FAVORITES) {
        state->screen = screen;
        return screen == NAV_RECENTS ? NAV_EFFECT_RETURN_TO_RECENTS
             : screen == NAV_FAVORITES ? NAV_EFFECT_RETURN_TO_FAVORITES : NAV_EFFECT_RETURN_TO_APPS;
    }
    if (!platform_exists(catalog, state->platform_id)) {
        state->platform_id[0] = '\0';
        state->game_id[0] = '\0';
        screen = NAV_PLATFORM_HOME;
    } else if (!game_exists(catalog, state->platform_id, state->game_id)) {
        state->game_id[0] = '\0';
        if (screen == NAV_GAME_DETAILS || screen == NAV_ACHIEVEMENTS_GALLERY)
            screen = NAV_GAME_LIBRARY;
    } else if (screen == NAV_ACHIEVEMENTS_GALLERY &&
               !ra_set_valid(catalog, state->platform_id, state->game_id)) {
        screen = NAV_GAME_DETAILS;
    }
    state->screen = screen;
    if (screen == NAV_GAME_DETAILS) return NAV_EFFECT_RETURN_TO_DETAILS;
    if (screen == NAV_ACHIEVEMENTS_GALLERY) return NAV_EFFECT_RETURN_TO_GALLERY;
    if (screen == NAV_GAME_LIBRARY) return NAV_EFFECT_RETURN_TO_LIBRARY;
    return NAV_EFFECT_RETURN_TO_HOME;
}

static NavEffect route_settings(NavModel *model, const NavCatalog *catalog, NavInput input) {
    NavSettingsLevel *level = &model->settings[model->settings_depth];
    if (input == NAV_INPUT_BACK) {
        if (model->settings_depth) {
            model->settings_depth--;
            return NAV_EFFECT_MOVE_SELECTION;
        }
        return return_to_screen(model, catalog, model->state.settings_return);
    }
    if (!level->count) return NAV_EFFECT_NONE;
    if (input == NAV_INPUT_UP && level->selected > 0) {
        level->selected--;
        return NAV_EFFECT_MOVE_SELECTION;
    }
    if (input == NAV_INPUT_DOWN && level->selected < level->count - 1) {
        level->selected++;
        return NAV_EFFECT_MOVE_SELECTION;
    }
    const NavSetting *item = &level->items[level->selected];
    if (!item->enabled || item->kind == NAV_SETTING_STATUS) return NAV_EFFECT_NONE;
    if ((input == NAV_INPUT_LEFT || input == NAV_INPUT_RIGHT) &&
        (item->kind == NAV_SETTING_TOGGLE || item->kind == NAV_SETTING_CHOICE ||
         item->kind == NAV_SETTING_RANGE))
        return NAV_EFFECT_ADJUST_SETTING;
    if (input != NAV_INPUT_CONFIRM) return NAV_EFFECT_NONE;
    if (item->kind != NAV_SETTING_SUBCATEGORY) return NAV_EFFECT_ACTIVATE_SETTING;
    if (model->settings_depth == NAV_SETTINGS_MAX_DEPTH ||
        (!item->children && item->child_count)) return NAV_EFFECT_NONE;
    model->settings[++model->settings_depth] = (NavSettingsLevel){
        .items = item->children, .count = item->child_count
    };
    return NAV_EFFECT_MOVE_SELECTION;
}

static NavEffect route_screen(NavModel *model, const NavCatalog *catalog, NavInput input) {
    NavState *state = &model->state;
    if (!model->enhanced_home) return NAV_EFFECT_NONE;
    if (state->screen == NAV_SETTINGS) return route_settings(model, catalog, input);
    if (input == NAV_INPUT_START) {
        state->settings_return = state->screen;
        state->screen = NAV_SETTINGS;
        model->settings_depth = 0;
        model->settings[0].selected = 0;
        return NAV_EFFECT_OPEN_SETTINGS;
    }
    if (input == NAV_INPUT_BACK) {
        if (state->screen == NAV_ACHIEVEMENTS_GALLERY)
            return return_to_screen(model, catalog, NAV_GAME_DETAILS);
        if (state->screen == NAV_GAME_DETAILS)
            return return_to_screen(model, catalog, state->details_return);
        if (state->screen == NAV_GAME_LIBRARY || state->screen == NAV_RECENTS ||
            state->screen == NAV_APPS || state->screen == NAV_FAVORITES)
            return return_to_screen(model, catalog, NAV_PLATFORM_HOME);
        return NAV_EFFECT_NONE;
    }
    if (state->screen == NAV_GAME_DETAILS && input == NAV_INPUT_GALLERY) {
        if (!ra_set_valid(catalog, state->platform_id, state->game_id))
            return NAV_EFFECT_NONE;
        state->screen = NAV_ACHIEVEMENTS_GALLERY;
        return NAV_EFFECT_OPEN_GALLERY;
    }
    if (state->screen == NAV_ACHIEVEMENTS_GALLERY)
        return NAV_EFFECT_NONE;
    if (input == NAV_INPUT_GALLERY)
        return NAV_EFFECT_NONE;
    if (input == NAV_INPUT_CONFIRM) {
        if (state->screen == NAV_APPS) return NAV_EFFECT_NONE;
        if (state->screen == NAV_PLATFORM_HOME) {
            if (!platform_exists(catalog, state->platform_id)) return NAV_EFFECT_NONE;
            state->screen = NAV_GAME_LIBRARY;
            return NAV_EFFECT_OPEN_LIBRARY;
        }
        if (!game_exists(catalog, state->platform_id, state->game_id)) return NAV_EFFECT_NONE;
        if (state->screen == NAV_GAME_LIBRARY || state->screen == NAV_RECENTS ||
            state->screen == NAV_FAVORITES) {
            state->details_return = state->screen;
            state->screen = NAV_GAME_DETAILS;
            return NAV_EFFECT_OPEN_DETAILS;
        }
        return NAV_EFFECT_LAUNCH_GAME;
    }
    if (state->screen == NAV_GAME_DETAILS)
        return input == NAV_INPUT_UP || input == NAV_INPUT_DOWN
               ? NAV_EFFECT_SCROLL_DETAILS : NAV_EFFECT_NONE;
    return NAV_EFFECT_MOVE_SELECTION;
}

bool nav_model_handle(NavModel *model, const NavCatalog *catalog, NavInput input,
                      NavEffect *effect) {
    NavEffect result = route_screen(model, catalog, input);
    if (effect) *effect = result;
    return result != NAV_EFFECT_NONE;
}

static int clamp_index(int n, int last) {
    if (n < 0) return 0;
    return n > last ? last : n;
}

void nav_window_init(NavHorizontalWindow *window, int count, int visible, int selected) {
    if (visible < 1) visible = 1;
    int last_offset = count > visible ? count - visible : 0;
    window->selected = count > 0 ? clamp_index(selected, count - 1) : -1;
    window->offset = clamp_index(window->selected, last_offset);
    window->transition_target = window->selected;
    window->transition_progress = 1.0f;
}

bool nav_window_move(NavHorizontalWindow *window, int count, int visible, int direction) {
    if (visible < 1) visible = 1;
    if (count <= 0 || window->selected < 0 || window->selected >= count) {
        nav_window_init(window, count, visible, window->selected);
        return false;
    }
    int old = window->selected;
    int last_offset = count > visible ? count - visible : 0;
    int offset = clamp_index(window->offset, last_offset);
    if (offset > old) offset = old;
    if (old - offset >= visible) offset = old - visible + 1;
    if (direction > 0 && old < count - 1) {
        window->selected++;
        if (old == offset && offset < last_offset) offset++;
    } else if (direction < 0 && old > 0) {
        window->selected--;
        if (window->selected < offset) offset--;
    }
    window->offset = offset;
    if (window->selected == old) return false;
    window->transition_target = window->selected;
    window->transition_progress = 0.0f;
    return true;
}

bool nav_model_save(const NavModel *model, char *buffer, size_t capacity, size_t *length) {
    const NavState *state = &model->state;
    if (!buffer || !valid_identity(state->platform_id) ||
        (state->game_id[0] && !valid_identity(state->game_id))) return false;
    int used = snprintf(buffer, capacity, "FROGNAV/1\nplatform=%s\ngame=%s\n",
                        state->platform_id, state->game_id);
    if (used < 0 || (size_t)used >= capacity) return false;
    if (length) *length = (size_t)used;
    return true;
}

static bool read_identity(const char **data, size_t *length, const char *key,
                          char identity[NAV_ID_MAX]) {
    size_t prefix = strlen(key);
    if (*length <= prefix || memcmp(*data, key, prefix) != 0) return false;
    const char *start = *data + prefix;
    const char *end = memchr(start, '\n', *length - prefix);
    if (!end) return false;
    size_t count = (size_t)(end - start);
    if (count >= NAV_ID_MAX || memchr(start, '\0', count)) return false;
    memcpy(identity, start, count);
    identity[count] = '\0';
    *length -= prefix + count + 1;
    *data = end + 1;
    return true;
}

bool nav_model_load(NavModel *model, const NavCatalog *catalog,
                    const char *buffer, size_t length) {
    static const char version[] = "FROGNAV/1\n";
    size_t prefix = sizeof version - 1;
    char platform[NAV_ID_MAX], game[NAV_ID_MAX];
    if (!buffer || !catalog || length < prefix || memcmp(buffer, version, prefix) != 0)
        return false;
    buffer += prefix;
    length -= prefix;
    if (!read_identity(&buffer, &length, "platform=", platform) ||
        !read_identity(&buffer, &length, "game=", game) || length != 0 ||
        !platform_exists(catalog, platform) ||
        (game[0] && !game_exists(catalog, platform, game))) return false;
    strcpy(model->state.platform_id, platform);
    strcpy(model->state.game_id, game);
    return true;
}
