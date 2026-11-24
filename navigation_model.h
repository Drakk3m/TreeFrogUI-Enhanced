#ifndef FROGUI_NAVIGATION_MODEL_H
#define FROGUI_NAVIGATION_MODEL_H

#include <stdbool.h>
#include <stddef.h>

#define NAV_ID_MAX 256
#define NAV_SETTINGS_MAX_DEPTH 4

typedef enum {
    NAV_PLATFORM_HOME, NAV_GAME_LIBRARY, NAV_GAME_DETAILS, NAV_SETTINGS,
    NAV_ACHIEVEMENTS_GALLERY, NAV_RECENTS, NAV_APPS, NAV_FAVORITES
} NavScreen;

typedef enum {
    NAV_INPUT_UP, NAV_INPUT_DOWN, NAV_INPUT_LEFT, NAV_INPUT_RIGHT,
    NAV_INPUT_CONFIRM, NAV_INPUT_BACK, NAV_INPUT_START, NAV_INPUT_GALLERY
} NavInput;

typedef enum {
    NAV_EFFECT_NONE, NAV_EFFECT_MOVE_SELECTION, NAV_EFFECT_SCROLL_DETAILS,
    NAV_EFFECT_OPEN_LIBRARY, NAV_EFFECT_OPEN_DETAILS, NAV_EFFECT_LAUNCH_GAME,
    NAV_EFFECT_OPEN_SETTINGS, NAV_EFFECT_RETURN_TO_LIBRARY, NAV_EFFECT_RETURN_TO_HOME,
    NAV_EFFECT_RETURN_TO_DETAILS, NAV_EFFECT_ACTIVATE_SETTING, NAV_EFFECT_ADJUST_SETTING,
    NAV_EFFECT_OPEN_GALLERY, NAV_EFFECT_RETURN_TO_GALLERY,
    NAV_EFFECT_RETURN_TO_RECENTS, NAV_EFFECT_RETURN_TO_APPS, NAV_EFFECT_RETURN_TO_FAVORITES
} NavEffect;

typedef enum {
    NAV_SETTING_TOGGLE, NAV_SETTING_CHOICE, NAV_SETTING_ACTION,
    NAV_SETTING_SUBCATEGORY, NAV_SETTING_RANGE, NAV_SETTING_STATUS
} NavSettingKind;

typedef struct NavSetting {
    const char *id;
    const char *title_key;
    const char *description_key;
    const char *icon_key;
    NavSettingKind kind;
    bool enabled;
    const struct NavSetting *children;
    size_t child_count;
} NavSetting;

typedef struct {
    const NavSetting *items;
    size_t count;
    size_t selected;
} NavSettingsLevel;

typedef struct {
    NavScreen screen;
    NavScreen settings_return;
    NavScreen details_return;
    char platform_id[NAV_ID_MAX];
    char game_id[NAV_ID_MAX];
} NavState;

typedef struct {
    bool (*platform_exists)(void *context, const char *id);
    bool (*game_exists)(void *context, const char *platform_id, const char *id);
    void *context;
    /* True only when the discovered game has a valid offline RA set. */
    bool (*ra_set_valid)(void *context, const char *platform_id, const char *id);
} NavCatalog;

typedef struct {
    NavState state;
    bool enhanced_home;
    NavSettingsLevel settings[NAV_SETTINGS_MAX_DEPTH + 1];
    size_t settings_depth;
} NavModel;

typedef struct {
    int selected;
    int offset;
    int transition_target;
    float transition_progress; /* Presentation advances this from 0 to 1. */
} NavHorizontalWindow;

void nav_model_init(NavModel *model, bool enabled);
bool nav_model_set_platform(NavModel *model, const NavCatalog *catalog, const char *id);
bool nav_model_set_game(NavModel *model, const NavCatalog *catalog, const char *id);
bool nav_model_handle(NavModel *model, const NavCatalog *catalog, NavInput input,
                      NavEffect *effect);

/* Use a borrowed collection that outlives this Settings session. A NULL
 * collection is valid with count zero. The default hub contains five categories. */
bool nav_settings_set_hub(NavModel *model, const NavSetting *items, size_t count);
const NavSettingsLevel *nav_settings_level(const NavModel *model);

void nav_window_init(NavHorizontalWindow *window, int count, int visible, int selected);
bool nav_window_move(NavHorizontalWindow *window, int count, int visible, int direction);

/* Explicit opt-in snapshot operations. Load requires discovered identities
 * and never changes the startup screen or stores transient presentation state. */
bool nav_model_save(const NavModel *model, char *buffer, size_t capacity, size_t *length);
bool nav_model_load(NavModel *model, const NavCatalog *catalog,
                    const char *buffer, size_t length);

#endif
