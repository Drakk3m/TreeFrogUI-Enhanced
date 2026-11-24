#ifndef FROGUI_ENHANCED_SETTINGS_H
#define FROGUI_ENHANCED_SETTINGS_H

typedef struct {
    const char *key, *title, *fallback;
    const int *rows;
    size_t count;
} EnhancedSettingsCategory;

#define SETTINGS_CATEGORY(key, title, fallback, ...) \
    { key, title, fallback, (const int[]){ __VA_ARGS__ }, \
      sizeof((const int[]){ __VA_ARGS__ }) / sizeof(int) }

static const EnhancedSettingsCategory enhanced_settings_categories[] = {
    SETTINGS_CATEGORY("appearance", "settings.appearance", "Appearance",
        SR_GRAPHICAL_HOME, SR_UI_THEME, SR_THEME_PACK, SR_ICON_PACK, SR_ANIMATIONS,
        SR_PILLS, SR_FONT, SR_FONT_SIZE, SR_SETTINGS_STYLE, SR_CENTER_TEXT,
        SR_FRIENDLY_NAMES, SR_BACKGROUNDS, SR_BACKGROUND_DIM, SR_WALLPAPER, SR_WALLPAPER_FIT),
    SETTINGS_CATEGORY("display-audio", "enhanced.settings.display_audio", "Display & Audio",
        SR_BRIGHTNESS, SR_VOLUME, SR_BATTERY_COLOR, SR_STOCK_BATTERY, SR_MENU_SOUNDS, SR_UI_SOUNDS),
    SETTINGS_CATEGORY("games", "enhanced.settings.games_library", "Games & Library",
        SR_QUICK_RESUME, SR_AUTOSAVE, SR_CUSTOM_ASPECT, SR_HIDE_EXTENSIONS, SR_HIDE_EMPTY,
        SR_GAME_SWITCHER, SR_ROUNDED_SCREENSHOTS, SR_START_IN_RECENTS),
    SETTINGS_CATEGORY("controls", "enhanced.controls", "Controls", SR_BUTTON_MAPPING),
    SETTINGS_CATEGORY("storage", "enhanced.settings.storage", "Storage",
        SR_ROM_SOURCE, SR_OTG_STATUS, SR_FILE_CACHE, SR_REBUILD_CACHE),
    SETTINGS_CATEGORY("date-time", "enhanced.settings.date_time", "Date & Time",
        SR_RTC_TIME, SR_RTC_SYNC, SR_RTC_BUMP),
    SETTINGS_CATEGORY("system", "settings.system", "System",
        SR_LANGUAGE, SR_DISABLE_SLEEP, SR_APPS_IN_SETTINGS, SR_OPEN_APPS),
    SETTINGS_CATEGORY("about", "enhanced.settings.about", "About", SR_VERSION)
};
#undef SETTINGS_CATEGORY

enum { ENHANCED_SETTINGS_CATEGORY_COUNT =
    sizeof enhanced_settings_categories / sizeof enhanced_settings_categories[0] };

static NavSetting enhanced_settings_hub[ENHANCED_SETTINGS_CATEGORY_COUNT];
static NavSetting enhanced_settings_items[SETTINGS_ROW_N];
static int enhanced_settings_rows[SETTINGS_ROW_N];
static size_t enhanced_settings_offsets[ENHANCED_SETTINGS_CATEGORY_COUNT];
static size_t enhanced_settings_positions[ENHANCED_SETTINGS_CATEGORY_COUNT];
static int enhanced_settings_scroll[ENHANCED_SETTINGS_CATEGORY_COUNT];
static unsigned enhanced_settings_dependencies;
static const char *enhanced_settings_version;
static bool enhanced_settings_otg, enhanced_settings_selector_active;
static int enhanced_settings_selector_cursor, enhanced_settings_selector_row;

#define enhanced_settings_category ((int)enhanced_settings_navigation.settings[0].selected)

static bool enhanced_settings_active(void) {
    return settings_menu_active && enhanced_settings_visit;
}

static bool enhanced_settings_options_focused(void) {
    return enhanced_settings_navigation.settings_depth != 0;
}

static bool enhanced_settings_row_in_category(int row, int category) {
    const EnhancedSettingsCategory *group = &enhanced_settings_categories[category];
    for (size_t i = 0; i < group->count; i++)
        if (group->rows[i] == row) return true;
    return false;
}

static int enhanced_settings_row_count(void) {
    return (int)enhanced_settings_hub[enhanced_settings_category].child_count;
}

static int enhanced_settings_row_at(int position) {
    if (position < 0 || position >= enhanced_settings_row_count()) return -1;
    return enhanced_settings_rows[enhanced_settings_offsets[enhanced_settings_category] + position];
}

static int enhanced_settings_row_cursor(void) {
    return (int)(enhanced_settings_options_focused()
         ? enhanced_settings_navigation.settings[1].selected
         : enhanced_settings_positions[enhanced_settings_category]);
}

static bool enhanced_settings_row_disabled(int row) {
    return !settings_row_enabled(&settings_rows[row]) ||
           (row == SR_ROM_SOURCE && !enhanced_settings_otg);
}

static NavSettingKind enhanced_settings_kind(int row) {
    switch (settings_rows[row].type) {
    case RT_TOGGLE: return NAV_SETTING_TOGGLE;
    case RT_RANGE: return NAV_SETTING_RANGE;
    case RT_UI_THEME: case RT_SETTINGS_LAYOUT: case RT_FONT: case RT_WALLPAPER:
    case RT_WALLFIT: case RT_THEME_PACK: case RT_ICON_PACK: case RT_ROM_SOURCE:
    case RT_LANGUAGE: return NAV_SETTING_CHOICE;
    case RT_INFO: case RT_OTG_STATUS: return NAV_SETTING_STATUS;
    default: return NAV_SETTING_ACTION;
    }
}

static void enhanced_settings_refresh(void) {
    unsigned dependencies = (settings_graphical_home != 0) |
        ((settings_apps_in_settings != 0) << 1) | ((settings_render_scheme_idx != 0) << 2) |
        ((settings_backgrounds != 0) << 3) | ((settings_style == STYLE_SYSTEM) << 4) |
        (enhanced_settings_otg << 5);
    if (dependencies == enhanced_settings_dependencies) return;
    enhanced_settings_dependencies = dependencies;
    int old_row = enhanced_settings_row_at(enhanced_settings_row_cursor());
    size_t offset = 0;
    for (int category = 0; category < ENHANCED_SETTINGS_CATEGORY_COUNT; category++) {
        const EnhancedSettingsCategory *group = &enhanced_settings_categories[category];
        enhanced_settings_offsets[category] = offset;
        size_t count = 0;
        for (size_t i = 0; i < group->count; i++) {
            int row = group->rows[i];
            if (!settings_row_available(&settings_rows[row])) continue;
            enhanced_settings_rows[offset + count] = row;
            enhanced_settings_items[offset + count++] = (NavSetting){
                .id = settings_rows[row].label, .title_key = settings_rows[row].label,
                .kind = enhanced_settings_kind(row), .enabled = !enhanced_settings_row_disabled(row)
            };
        }
        enhanced_settings_hub[category] = (NavSetting){
            .id = group->key, .title_key = group->title, .kind = NAV_SETTING_SUBCATEGORY,
            .enabled = true, .children = enhanced_settings_items + offset, .child_count = count
        };
        if (enhanced_settings_positions[category] >= count)
            enhanced_settings_positions[category] = count ? count - 1 : 0;
        offset += count;
    }
    if (enhanced_settings_options_focused()) {
        int category = enhanced_settings_category;
        NavSettingsLevel *level = &enhanced_settings_navigation.settings[1];
        level->items = enhanced_settings_hub[category].children;
        level->count = enhanced_settings_hub[category].child_count;
        level->selected = enhanced_settings_positions[category];
        for (size_t i = 0; i < level->count; i++)
            if (enhanced_settings_row_at((int)i) == old_row) level->selected = i;
        enhanced_settings_positions[category] = level->selected;
    }
}

static void enhanced_settings_begin(int initial_row) {
    enhanced_settings_visit = true;
    enhanced_settings_otg = otg_roms_available();
    enhanced_settings_version = treefrogui_version();
    enhanced_settings_dependencies = ~0u;
    enhanced_settings_selector_active = false;
    memset(enhanced_settings_positions, 0, sizeof enhanced_settings_positions);
    memset(enhanced_settings_scroll, 0, sizeof enhanced_settings_scroll);
    nav_settings_set_hub(&enhanced_settings_navigation, enhanced_settings_hub,
                         ENHANCED_SETTINGS_CATEGORY_COUNT);
    enhanced_settings_refresh();
    if (initial_row < 0) return;
    for (int i = 0; i < ENHANCED_SETTINGS_CATEGORY_COUNT; i++) {
        if (!enhanced_settings_row_in_category(initial_row, i)) continue;
        enhanced_settings_navigation.settings[0].selected = (size_t)i;
        nav_model_handle(&enhanced_settings_navigation, NULL, NAV_INPUT_CONFIRM, NULL);
        for (int j = 0; j < enhanced_settings_row_count(); j++)
            if (enhanced_settings_row_at(j) == initial_row)
                enhanced_settings_navigation.settings[1].selected = (size_t)j;
        break;
    }
}

typedef struct { int index, count; } SettingsChoice;

static SettingsChoice enhanced_settings_choices(int row) {
    switch (row) {
    case SR_UI_THEME: return (SettingsChoice){ui_theme_index(), ui_theme_count()};
    case SR_SETTINGS_STYLE: return (SettingsChoice){settings_layout, 2};
    case SR_FONT: return (SettingsChoice){settings_font_idx, font_count};
    case SR_WALLPAPER: return (SettingsChoice){settings_wallpaper_idx, wallpaper_count};
    case SR_WALLPAPER_FIT: return (SettingsChoice){settings_wallpaper_fit, WALL_FIT_N};
    case SR_THEME_PACK: return (SettingsChoice){settings_theme_pack_idx, theme_pack_count};
    case SR_ICON_PACK: return (SettingsChoice){settings_icon_pack_idx, icon_pack_count};
    case SR_ROM_SOURCE: return (SettingsChoice){settings_rom_source, ROM_SOURCE_COUNT};
    case SR_LANGUAGE: return (SettingsChoice){settings_language, i18n_language_count()};
    default: return (SettingsChoice){0, 0};
    }
}

static const char *settings_choice_name(int row, int index) {
    switch (row) {
    case SR_UI_THEME: return ui_theme_name(index);
    case SR_SETTINGS_STYLE: return settings_layout_names[index];
    case SR_FONT: return font_disp[index];
    case SR_WALLPAPER: return wallpaper_disp[index];
    case SR_WALLPAPER_FIT: return wallpaper_fit_names[index];
    case SR_THEME_PACK: return theme_pack_disp[index];
    case SR_ICON_PACK: return icon_pack_disp[index];
    case SR_ROM_SOURCE: return rom_source_names[index];
    case SR_LANGUAGE: {
        char key[48];
        const char *code = i18n_language_code_at(index);
        snprintf(key, sizeof key, "language.%s", code);
        return tr_or(key, code);
    }
    default: return "";
    }
}

static int enhanced_settings_selector_count(void) {
    return enhanced_settings_choices(enhanced_settings_selector_row).count;
}

static const char *enhanced_settings_selector_name(int index) {
    return settings_choice_name(enhanced_settings_selector_row, index);
}

static const char *settings_option_label(int row) {
    switch (row) {
    case SR_GRAPHICAL_HOME: return tr_or("enhanced.settings.interface", "Interface");
    case SR_UI_THEME: return tr_or("enhanced.settings.ui_theme", "UI theme");
    case SR_SETTINGS_STYLE: return tr_or("enhanced.settings.classic_style", "Classic Settings style");
    case SR_PILLS: return tr_or("enhanced.settings.pills", "Decorative pills");
    case SR_UI_SOUNDS: return tr_or("settings.ui_sounds", "UI sounds");
    case SR_ROUNDED_SCREENSHOTS: return tr_or("enhanced.settings.rounded", "Rounded recents screenshots");
    case SR_APPS_IN_SETTINGS: return tr_or("enhanced.settings.apps", "Apps in Settings");
    case SR_OPEN_APPS: return tr_or("enhanced.settings.open_apps", "Open Apps");
    default: return tr(settings_rows[row].label);
    }
}

static void settings_option_value(int row, char *out, size_t size) {
    const SRow *setting = &settings_rows[row];
    SettingsChoice choice = enhanced_settings_choices(row);
    *out = '\0';
    if (choice.count) {
        int index = row == SR_ROM_SOURCE && !enhanced_settings_otg ? ROM_SOURCE_SD : choice.index;
        if (index >= 0 && index < choice.count)
            snprintf(out, size, "%s", settings_choice_name(row, index));
    } else if (setting->type == RT_RANGE) {
        const char *unit = row == SR_FONT_SIZE ? " px" : row == SR_RTC_BUMP ? " min" : "%";
        snprintf(out, size, "%d%s", *setting->val, unit);
    } else if (setting->type == RT_TOGGLE) {
        const char *value = row == SR_GRAPHICAL_HOME
            ? (*setting->val ? tr_or("enhanced.settings.enhanced_mode", "Enhanced")
                            : tr_or("enhanced.settings.classic_mode", "Classic"))
            : tr(*setting->val ? "value.on" : "value.off");
        snprintf(out, size, "%s", value);
    } else if (row == SR_VERSION) {
        snprintf(out, size, "%s", enhanced_settings_version);
    } else if (row == SR_OTG_STATUS) {
        snprintf(out, size, "%s", tr(enhanced_settings_otg ? "value.connected" : "value.not_connected"));
    } else if (row == SR_RTC_TIME) {
        time_t now = time(NULL);
        struct tm value;
        if (settings_rtc_edit_active) {
            value = settings_rtc_edit_time;
            const int fields[] = {value.tm_year + 1900, value.tm_mon + 1, value.tm_mday,
                                  value.tm_hour, value.tm_min};
            char parts[RTC_FIELD_COUNT][12];
            for (int i = 0; i < RTC_FIELD_COUNT; i++)
                snprintf(parts[i], sizeof parts[i],
                         i == settings_rtc_edit_field ? "[%02d]" : "%02d", fields[i]);
            snprintf(out, size, "%s-%s-%s %s:%s", parts[0], parts[1], parts[2], parts[3], parts[4]);
        } else if (localtime_r(&now, &value)) {
            strftime(out, size, "%Y-%m-%d %H:%M", &value);
        }
    }
}

static const char *settings_option_notice(int row) {
    if (row < 0) return "";
    if (enhanced_settings_row_disabled(row)) {
        if (row == SR_ROM_SOURCE) return tr_or("enhanced.settings.needs_otg", "Connect OTG storage with a ROM folder");
        if (row == SR_ICON_PACK) return tr("settings.requires_system_view");
        if (settings_render_scheme_idx > 0)
            return tr_or("enhanced.settings.scheme_controls", "Controlled by the active UI scheme");
        return tr("settings.requires_background_images");
    }
    switch (row) {
    case SR_LANGUAGE: return tr_or("enhanced.settings.restart_frontend", "Restarts FrogUI when confirmed");
    case SR_DISABLE_SLEEP: case SR_RTC_BUMP:
        return tr_or("enhanced.settings.next_boot", "Applies on the next device startup");
    case SR_QUICK_RESUME: case SR_START_IN_RECENTS:
        return tr_or("enhanced.settings.next_start", "Applies on the next FrogUI start");
    case SR_AUTOSAVE: case SR_CUSTOM_ASPECT:
        return tr_or("enhanced.settings.game_runtime", "Used by the game runtime");
    case SR_ROM_SOURCE: case SR_GRAPHICAL_HOME: case SR_HIDE_EMPTY: case SR_FILE_CACHE:
        return tr_or("enhanced.settings.on_exit", "Applies to browsing when you leave Settings");
    case SR_BRIGHTNESS: case SR_VOLUME: case SR_FONT: case SR_FONT_SIZE:
    case SR_ANIMATIONS: case SR_BACKGROUND_DIM:
        return tr_or("enhanced.settings.preview", "Previews immediately");
    case SR_SETTINGS_STYLE:
        return tr_or("enhanced.settings.classic_only", "Used by the Classic Settings screen");
    default: return "";
    }
}

static void handle_enhanced_settings_menu(void) {
    bool up = input_repeat(FROG_BTN_UP), down = input_repeat(FROG_BTN_DOWN);
    bool left = input_repeat(FROG_BTN_LEFT), right = input_repeat(FROG_BTN_RIGHT);
    bool confirm = input_was_pressed(FROG_BTN_A), back = input_was_pressed(FROG_BTN_B);
    if (settings_rtc_edit_active) {
        settings_handle_rtc_editor(up, down, left, right);
        return;
    }

    enhanced_settings_refresh();
    if (enhanced_settings_selector_active) {
        int count = enhanced_settings_selector_count();
        if (back || !count) {
            enhanced_settings_selector_active = false;
        } else if (confirm) {
            int row = enhanced_settings_selector_row;
            int delta = enhanced_settings_selector_cursor - enhanced_settings_choices(row).index;
            enhanced_settings_selector_active = false;
            if (delta && !enhanced_settings_row_disabled(row))
                settings_adjust_row(&settings_rows[row], delta, true);
        } else if ((up || left) && enhanced_settings_selector_cursor > 0) {
            enhanced_settings_selector_cursor--;
        } else if ((down || right) && enhanced_settings_selector_cursor + 1 < count) {
            enhanced_settings_selector_cursor++;
        }
        enhanced_settings_refresh();
        return;
    }

    NavModel *navigation = &enhanced_settings_navigation;
    if (back && !enhanced_settings_options_focused()) {
        enhanced_settings_visit = false;
        settings_exit_menu();
        return;
    }

    NavInput input;
    if (back) input = NAV_INPUT_BACK;
    else if (confirm) input = NAV_INPUT_CONFIRM;
    else if (up) input = NAV_INPUT_UP;
    else if (down) input = NAV_INPUT_DOWN;
    else if (left) input = NAV_INPUT_LEFT;
    else if (right) input = NAV_INPUT_RIGHT;
    else return;

    bool in_options = enhanced_settings_options_focused();
    int row = enhanced_settings_row_at(enhanced_settings_row_cursor());
    NavEffect effect = NAV_EFFECT_NONE;
    nav_model_handle(navigation, NULL, input, &effect);
    int category = enhanced_settings_category;
    if (!in_options && enhanced_settings_options_focused()) {
        if (enhanced_settings_row_in_category(SR_ROM_SOURCE, category)) {
            enhanced_settings_otg = otg_roms_available();
            enhanced_settings_refresh();
        }
        navigation->settings[1].selected = enhanced_settings_positions[category];
    } else if (enhanced_settings_options_focused()) {
        enhanced_settings_positions[category] = navigation->settings[1].selected;
    }
    int focused_row = enhanced_settings_row_at(enhanced_settings_row_cursor());
    if (focused_row >= 0) settings_menu_idx = focused_row;
    if (!in_options || row < 0) return;

    const SRow *setting = &settings_rows[row];
    if (effect == NAV_EFFECT_ADJUST_SETTING && row != SR_LANGUAGE) {
        settings_menu_idx = row;
        settings_adjust_row(setting, right ? 1 : -1,
                            input_was_pressed(right ? FROG_BTN_RIGHT : FROG_BTN_LEFT));
    } else if (effect == NAV_EFFECT_ACTIVATE_SETTING) {
        settings_menu_idx = row;
        if (enhanced_settings_kind(row) == NAV_SETTING_CHOICE) {
            if (row == SR_ROM_SOURCE) {
                enhanced_settings_otg = otg_roms_available();
                enhanced_settings_refresh();
            }
            SettingsChoice choice = enhanced_settings_choices(row);
            if (choice.count && !enhanced_settings_row_disabled(row)) {
                enhanced_settings_selector_active = true;
                enhanced_settings_selector_row = row;
                enhanced_settings_selector_cursor = choice.index;
            }
        } else if (setting->type == RT_TOGGLE) {
            settings_adjust_row(setting, 1, true);
        } else {
            if (row == SR_REBUILD_CACHE) {
                enhanced_settings_visit = false;
                navigation->settings_depth = 0;
            }
            settings_activate_row(setting);
        }
    }
    enhanced_settings_refresh();
}

#endif
