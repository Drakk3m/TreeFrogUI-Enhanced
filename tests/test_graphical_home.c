/* Host integration test. Include the core to exercise its private UI state. */
#include <assert.h>
static char test_theme_pack_root[512];
#define FROGUI_THEME_PACK_ROOT test_theme_pack_root
static char test_screenshot_root[512];
#define FROGUI_SCREENSHOT_ROOT test_screenshot_root
#include "../frogui_libretro.c"

static char test_settings[MAX_PATH_LEN];
static char test_cache[MAX_PATH_LEN];
static char test_launch[MAX_PATH_LEN];
static char test_p9_favorites[MAX_PATH_LEN], test_p9_history[MAX_PATH_LEN];
static char test_sessions[MAX_PATH_LEN];
static char test_playtime[MAX_PATH_LEN];

static const char *cache_fixture_path(const char *path, char *mapped, size_t size) {
    size_t prefix = strlen(CACHE_DIR);
    if (strncmp(path, CACHE_DIR, prefix) != 0 ||
        (path[prefix] != 0 && path[prefix] != '/')) return path;
    assert(test_cache[0]);
    assert(snprintf(mapped, size, "%s%s", test_cache, path + prefix) < (int)size);
    return mapped;
}

DIR *__real_opendir(const char *path);
DIR *__wrap_opendir(const char *path) {
    char mapped[MAX_PATH_LEN];
    return __real_opendir(cache_fixture_path(path, mapped, sizeof mapped));
}

int __real_mkdir(const char *path, mode_t mode);
int __wrap_mkdir(const char *path, mode_t mode) {
    char mapped[MAX_PATH_LEN];
    path = cache_fixture_path(path, mapped, sizeof mapped);
    if (strncmp(path, "/mnt/sdcard/", 12) == 0) {
        errno = EACCES;
        return -1;
    }
    return __real_mkdir(path, mode);
}

int __real_unlink(const char *path);
int __wrap_unlink(const char *path) {
    char mapped[MAX_PATH_LEN];
    return __real_unlink(cache_fixture_path(path, mapped, sizeof mapped));
}

int __real_rename(const char *old_path, const char *new_path);
int __wrap_rename(const char *old_path, const char *new_path) {
    char old_mapped[MAX_PATH_LEN], new_mapped[MAX_PATH_LEN];
    return __real_rename(cache_fixture_path(old_path, old_mapped, sizeof old_mapped),
                         cache_fixture_path(new_path, new_mapped, sizeof new_mapped));
}

void __wrap_sync(void) { }

static char test_legacy_icon[512];
static const char legacy_icon[] = "/mnt/sdcard/frogui/system-icons/NoArtwork.png";
static bool measure_allocations;
static size_t allocations, allocation_bytes, largest_allocation;

void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    if (measure_allocations) {
        allocations++;
        allocation_bytes += size;
        if (size > largest_allocation) largest_allocation = size;
    }
    return __real_malloc(size);
}

FILE *__real_fopen(const char *path, const char *mode);
FILE *__wrap_fopen(const char *path, const char *mode) {
    char mapped[MAX_PATH_LEN];
    path = cache_fixture_path(path, mapped, sizeof mapped);
    if (test_sessions[0] && strcmp(path, PLAY_SESSIONS_PATH) == 0)
        return __real_fopen(test_sessions, mode);
    if (test_playtime[0] && strcmp(path, "/mnt/sdcard/frogui/playtime.txt") == 0)
        return __real_fopen(test_playtime, mode);
    if (test_p9_favorites[0] && !strcmp(path, "/mnt/sdcard/frogui/favorites.txt"))
        return __real_fopen(test_p9_favorites, mode);
    if (test_p9_history[0] && !strcmp(path, HISTORY_FILE))
        return __real_fopen(test_p9_history, mode);
    if (strcmp(path, LAUNCH_FILE) == 0) return __real_fopen(test_launch, mode);
    if (strcmp(path, SETTINGS_FILE) == 0)
        return __real_fopen(test_settings, mode);
    if (test_legacy_icon[0] && strcmp(path, legacy_icon) == 0)
        return __real_fopen(test_legacy_icon, mode);
    if (strncmp(path, "/mnt/sdcard/", 12) == 0)
        return NULL;
    return __real_fopen(path, mode);
}

/* Keep settings writes and asset lookups inside the host fixture. */
int __wrap_system(const char *command) {
    (void)command;
    return 0;
}

static bool test_app_executables;
int __real_access(const char *path, int mode);
int __wrap_access(const char *path, int mode) {
    if (test_app_executables && (mode & X_OK) &&
        (!strcmp(path, USB_MODE_BIN) || !strcmp(path, REBOOT_BIN) || !strcmp(path, SHUTDOWN_BIN)))
        return 0;
    if (test_legacy_icon[0] && strcmp(path, legacy_icon) == 0)
        return __real_access(test_legacy_icon, mode);
    if (strncmp(path, "/mnt/sdcard/", 12) == 0)
        return -1;
    return __real_access(path, mode);
}

static long input_fixture_ms = 1000;

int __wrap_gettimeofday(struct timeval *time, void *zone) {
    (void)zone;
    time->tv_sec = input_fixture_ms / 1000;
    time->tv_usec = (input_fixture_ms % 1000) * 1000;
    return 0;
}

static void press(FrogButton button) {
    input_fixture_ms += 100;
    input_set_ext_raw(1u << input_get_raw_bit(button));
    handle_input();
    /* Face buttons require a stable hold in the real input layer. */
    if (button == FROG_BTN_A || button == FROG_BTN_B || button == FROG_BTN_X || button == FROG_BTN_Y) {
        input_fixture_ms += 80;
        handle_input();
    }
    input_set_ext_raw(0);
    handle_input();
}

static void write_fixture(const char *path, const char *value) {
    FILE *file = fopen(path, "w");
    assert(file);
    assert(fputs(value, file) >= 0);
    assert(fclose(file) == 0);
}

static const unsigned char icon_png[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
    0x0d, 0x49, 0x44, 0x41, 0x54, 0x78, 0x9c, 0x63, 0xf8, 0xcf, 0xc0, 0xb0,
    0x00, 0x00, 0x04, 0xa1, 0x01, 0xa0, 0xa4, 0xc0, 0xe4, 0x7d, 0x00, 0x00,
    0x00, 0x00, 0x49, 0x45, 0x4e, 0x44, 0xae, 0x42, 0x60, 0x82
};

static const unsigned char corrupt_png[] = {
    0x89, 0x50, 0x4e, 0x47, 0x0d, 0x0a, 0x1a, 0x0a, 0x00, 0x00, 0x00, 0x0d,
    0x49, 0x48, 0x44, 0x52, 0x00, 0x00, 0x00, 0x01, 0x00, 0x00, 0x00, 0x01,
    0x08, 0x06, 0x00, 0x00, 0x00, 0x1f, 0x15, 0xc4, 0x89, 0x00, 0x00, 0x00,
    0x08, 0x49, 0x44, 0x41, 0x54, 0x6e, 0x6f, 0x74, 0x20, 0x7a, 0x6c, 0x69,
    0x62, 0x55, 0x69, 0x11, 0xf7, 0x00, 0x00, 0x00, 0x00, 0x49, 0x45, 0x4e,
    0x44, 0xae, 0x42, 0x60, 0x82
};

static void write_bytes(const char *path, const void *bytes, size_t size) {
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(bytes, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void write_full_screen_bmp(const char *path) {
    /* 640x480 BI_RGB, 24-bit, tightly aligned rows. */
    const unsigned char header[54] = {
        'B','M',0x36,0x10,0x0e,0,0,0,0,0,54,0,0,0,
        40,0,0,0,0x80,2,0,0,0xe0,1,0,0,1,0,24,0
    };
    unsigned char row[640 * 3];
    for (size_t i = 0; i < sizeof row; i += 3) {
        row[i] = 0;
        row[i + 1] = 0;
        row[i + 2] = 255;
    }
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(header, 1, sizeof header, file) == sizeof header);
    for (int y = 0; y < 480; y++)
        assert(fwrite(row, 1, sizeof row, file) == sizeof row);
    assert(fclose(file) == 0);
}

static void write_cover_bmp(const char *path, unsigned width, unsigned height) {
    size_t row_size = (width * 3 + 3) & ~3u;
    size_t data_size = row_size * height;
    unsigned char header[54] = {0};
    header[0] = 'B'; header[1] = 'M'; header[10] = 54; header[14] = 40;
    header[26] = 1; header[28] = 24;
    unsigned values[] = {(unsigned)(54 + data_size), width, height};
    int offsets[] = {2, 18, 22};
    for (size_t i = 0; i < sizeof values / sizeof values[0]; i++)
        for (int shift = 0; shift < 32; shift += 8)
            header[offsets[i] + shift / 8] = (unsigned char)(values[i] >> shift);
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(header, 1, sizeof header, file) == sizeof header);
    unsigned char row[2000];
    assert(row_size <= sizeof row);
    memset(row, 0x90, row_size);
    for (unsigned y = 0; y < height; y++) assert(fwrite(row, 1, row_size, file) == row_size);
    assert(fclose(file) == 0);
}

static void test_settings_mapping_contract(void) {
    /* The settings table deliberately has 48 rows: six section headers and
     * exactly 42 mapped controls. This catches accidental loss or reordering
     * when a control is added to the Enhanced settings projection. */
    static const char *const expected[] = {
        "settings.appearance", "UI theme", "settings.background_theme_pack", "Settings style",
        "Enhanced graphical platform home", "Decorative pills", "settings.icon_pack",
        "settings.center_text", "settings.friendly_system_names", "settings.font", "settings.font_size",
        "settings.battery_colour_mode", "settings.stock_battery_indicator", "settings.background_images",
        "settings.background_dim", "settings.wallpaper", "settings.background_image_fit", "settings.general",
        "settings.language", "settings.brightness", "settings.animations", "settings.menu_sounds",
        "settings.ui_sounds", "settings.hide_extensions", "settings.hide_empty_folders", "settings.library",
        "Apps in Settings", "Open Apps", "settings.rom_source", "settings.otg_storage", "settings.game_switcher",
        "Rounded recents screenshots", "settings.start_in_recents", "settings.gameplay", "settings.quick_resume",
        "settings.autosave_autoload", "settings.custom_aspect_ratios", "settings.rtc_emulation", "settings.rtc_time",
        "settings.rtc_sync_usb", "settings.rtc_bump", "settings.system", "settings.volume", "settings.file_cache",
        "settings.rebuild_file_cache", "settings.disable_sleep", "settings.button_mapping", "settings.version"
    };
    assert(SETTINGS_ROW_N == (int)(sizeof expected / sizeof expected[0]));
    for (int section = 0; section < SETTINGS_SECTION_N; section++)
        settings_section_open[section] = true;
    settings_layout = SETTINGS_LAYOUT_SECTIONS;
    settings_build_vis_rows();
    int mapped = 0;
    for (int i = 0; i < SETTINGS_ROW_N; i++) {
        assert(settings_rows[i].label && strcmp(settings_rows[i].label, expected[i]) == 0);
        if (settings_rows[i].type != RT_HEADER) {
            mapped++;
            assert(settings_rows[i].label[0]);
            assert(settings_row_visible(i));
        }
    }
    assert(mapped == 42);

    /* Dependent controls remain represented but become unavailable. */
    settings_render_scheme_idx = 0;
    settings_backgrounds = 0;
    assert(!settings_row_enabled(&settings_rows[2]));
    settings_backgrounds = 1;
    assert(settings_row_enabled(&settings_rows[2]));
    settings_render_scheme_idx = 1;
    assert(!settings_row_enabled(&settings_rows[2]));
    settings_render_scheme_idx = 0;
    settings_style = STYLE_SYSTEM;
    assert(settings_row_enabled(&settings_rows[6]));
    settings_style = STYLE_VERTICAL;
    assert(!settings_row_enabled(&settings_rows[6]));
    settings_style = STYLE_SYSTEM;
    puts("PASS 42 settings mappings, labels, visibility and dependent enablement");
}

static void test_classic_and_enhanced_navigation_contract(void) {
    NavModel model;
    NavEffect effect = NAV_EFFECT_NONE;
    nav_model_init(&model, false);
    assert(nav_model_set_platform(&model, NULL, "NES"));
    assert(!nav_model_handle(&model, NULL, NAV_INPUT_CONFIRM, &effect));
    assert(model.state.screen == NAV_PLATFORM_HOME);
    nav_model_init(&model, true);
    assert(nav_model_set_platform(&model, NULL, "NES"));
    assert(nav_model_handle(&model, NULL, NAV_INPUT_CONFIRM, &effect));
    assert(effect == NAV_EFFECT_OPEN_LIBRARY && model.state.screen == NAV_GAME_LIBRARY);
    assert(nav_model_handle(&model, NULL, NAV_INPUT_BACK, &effect));
    assert(effect == NAV_EFFECT_RETURN_TO_HOME && model.state.screen == NAV_PLATFORM_HOME);
    puts("PASS Classic disabled and Enhanced navigation routing contract");
}

static void select_enhanced_setting(int row) {
    int target = -1;
    for (int i = 0; i < ENHANCED_SETTINGS_CATEGORY_COUNT; i++)
        if (enhanced_settings_row_in_category(row, i)) target = i;
    assert(target >= 0);
    if (enhanced_settings_options_focused()) press(FROG_BTN_B);
    while (enhanced_settings_category < target) press(FROG_BTN_DOWN);
    while (enhanced_settings_category > target) press(FROG_BTN_UP);
    press(FROG_BTN_A);
    assert(enhanced_settings_options_focused());
    int position = 0;
    while (position < enhanced_settings_row_count() && enhanced_settings_row_at(position) != row)
        position++;
    assert(position < enhanced_settings_row_count());
    while (enhanced_settings_row_cursor() < position) press(FROG_BTN_DOWN);
    while (enhanced_settings_row_cursor() > position) press(FROG_BTN_UP);
}

static unsigned settings_shutdown_requests;

static bool capture_settings_environment(unsigned command, void *data) {
    (void)data;
    if (command == RETRO_ENVIRONMENT_SHUTDOWN) settings_shutdown_requests++;
    return false;
}

static void test_enhanced_settings_integration(void) {
    for (int i = 0; i < SETTINGS_ROW_N; i++) {
        int mapped = 0;
        for (int j = 0; j < ENHANCED_SETTINGS_CATEGORY_COUNT; j++)
            mapped += enhanced_settings_row_in_category(i, j);
        assert(mapped == (settings_rows[i].type != RT_HEADER));
    }
    /* Exercise the original adapter with the shared operations in place. */
    settings_graphical_home = 0;
    settings_layout = SETTINGS_LAYOUT_SECTIONS;
    settings_section_open[1] = true;
    settings_brightness = 75;
    settings_menu_active = true;
    settings_menu_idx = SR_BRIGHTNESS;
    handle_settings_menu();
    press(FROG_BTN_RIGHT);
    assert(settings_menu_active && settings_brightness == 80);
    press(FROG_BTN_A);
    assert(!settings_menu_active);
    settings_brightness = 75;
    settings_load_file();
    assert(settings_brightness == 80);

    strcpy(current_path, ROMS_PATH);
    scan_directory(current_path);
    settings_graphical_home = 1;
    settings_render_scheme_idx = 0;
    settings_style = STYLE_VERTICAL;
    settings_apps_in_settings = 0;
    int previous_apps = settings_apps_in_settings;
    open_enhanced_settings();
    assert(enhanced_settings_active() && enhanced_settings_category == 0);
    press(FROG_BTN_UP);
    assert(enhanced_settings_category == 0);
    press(FROG_BTN_R1);
    assert(enhanced_settings_active());

    select_enhanced_setting(SR_UI_THEME);
    int previous_theme = ui_theme_index();
    press(FROG_BTN_A);
    assert(enhanced_settings_selector_active);
    press(FROG_BTN_DOWN);
    assert(ui_theme_index() == previous_theme);
    press(FROG_BTN_B);
    assert(!enhanced_settings_selector_active && ui_theme_index() == previous_theme);
    int selection = enhanced_settings_row_cursor();
    press(FROG_BTN_B);
    press(FROG_BTN_A);
    assert(enhanced_settings_row_cursor() == selection);

    press(FROG_BTN_A);
    press(FROG_BTN_DOWN);
    press(FROG_BTN_A);
    assert(ui_theme_index() != previous_theme && enhanced_settings_active());
    press(FROG_BTN_A);
    press(FROG_BTN_UP);
    press(FROG_BTN_A);
    assert(ui_theme_index() == previous_theme);

    select_enhanced_setting(SR_ICON_PACK);
    assert(enhanced_settings_row_disabled(SR_ICON_PACK));
    press(FROG_BTN_A);
    assert(!enhanced_settings_selector_active);
    select_enhanced_setting(SR_UI_SOUNDS);
    assert(enhanced_settings_row_at(enhanced_settings_row_cursor()) == SR_UI_SOUNDS);
    for (int i = 0; i < enhanced_settings_row_count(); i++)
        assert(enhanced_settings_row_at(i) != SR_MENU_SOUNDS);
    select_enhanced_setting(SR_VERSION);
    press(FROG_BTN_A);
    assert(enhanced_settings_options_focused() && !enhanced_settings_selector_active);

    select_enhanced_setting(SR_RTC_TIME);
    press(FROG_BTN_A);
    assert(settings_rtc_edit_active);
    press(FROG_BTN_RIGHT);
    assert(settings_rtc_edit_field == RTC_MONTH);
    press(FROG_BTN_B);
    assert(!settings_rtc_edit_active && enhanced_settings_options_focused());

    select_enhanced_setting(SR_ROM_SOURCE);
    enhanced_settings_otg = false;
    enhanced_settings_refresh();
    int previous_source = settings_rom_source;
    press(FROG_BTN_RIGHT);
    press(FROG_BTN_A);
    assert(settings_rom_source == previous_source && !enhanced_settings_selector_active);

    select_enhanced_setting(SR_BUTTON_MAPPING);
    press(FROG_BTN_A);
    assert(remap_wizard_active && !settings_menu_active && remap_step == 0);
    int count_buttons = remap_wizard_count();
    for (int i = 0; i < count_buttons; i++) press(FROG_BTN_B);
    assert(!remap_wizard_active && enhanced_settings_active());
    assert(enhanced_settings_row_at(enhanced_settings_row_cursor()) == SR_BUTTON_MAPPING);

    select_enhanced_setting(SR_LANGUAGE);
    assert(i18n_scan_languages() > 1);
    settings_language = 0;
    environ_cb = capture_settings_environment;
    press(FROG_BTN_A);
    press(FROG_BTN_DOWN);
    assert(settings_language == 0 && !settings_shutdown_requests);
    press(FROG_BTN_A);
    assert(settings_language == 1 && settings_shutdown_requests == 1);
    environ_cb = NULL;
    settings_language = 0;
    assert(i18n_init("en_US"));

    render_set_geometry(640, 480, 100);
    assert(!framebuffer);
    framebuffer = calloc(640 * 480, sizeof *framebuffer);
    assert(framebuffer);
    memset(enhanced_settings_icons, 0, sizeof enhanced_settings_icons);
    for (int i = 0; i < ENHANCED_SETTINGS_CATEGORY_COUNT; i++) {
        select_enhanced_setting(enhanced_settings_categories[i].rows[0]);
        render_enhanced_settings_menu();
    }
    settings_panel_category_text("Long localized category title with more than two words",
                                 16, 64, 184, 42, COLOR_TEXT);
    settings_panel_text("An extremely long localized option value with many words",
                        560, 120, 64, 38, COLOR_TEXT);
    free(framebuffer);
    framebuffer = NULL;

    select_enhanced_setting(SR_APPS_IN_SETTINGS);
    int count = enhanced_settings_row_count();
    press(FROG_BTN_A);
    assert(settings_apps_in_settings != previous_apps);
    assert(enhanced_settings_row_count() == count + 1);
    select_enhanced_setting(SR_OPEN_APPS);
    press(FROG_BTN_A);
    assert(settings_apps_origin && viewing_apps && !settings_menu_active);
    press(FROG_BTN_B);
    assert(enhanced_settings_active() &&
           enhanced_settings_row_at(enhanced_settings_row_cursor()) == SR_OPEN_APPS);

    select_enhanced_setting(SR_GRAPHICAL_HOME);
    press(FROG_BTN_A);
    assert(!settings_graphical_home && enhanced_settings_active());
    press(FROG_BTN_B);
    assert(enhanced_settings_active() && !enhanced_settings_options_focused());
    press(FROG_BTN_B);
    assert(!settings_menu_active && !enhanced_settings_visit);
    puts("PASS Classic preview and persistence, Enhanced settings focus, selectors, RTC, Apps and mode exit");
}

static void test_bounded_artwork(const char *root) {
    char folder[MAX_PATH_LEN], path[MAX_PATH_LEN], rom[MAX_PATH_LEN];
    assert(snprintf(folder, sizeof folder, "%s/AFirst/.res", root) < (int)sizeof folder);
    assert(mkdir(folder, 0700) == 0);
    Thumbnail image = {0};
    assert(snprintf(rom, sizeof rom, "%s/AFirst/First.nes", root) < (int)sizeof rom);
    assert(!load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    assert(snprintf(path, sizeof path, "%s/First.png", folder) < (int)sizeof path);
    assert(mkfifo(path, 0600) == 0);
    assert(!load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    assert(unlink(path) == 0);
    write_bytes(path, "GIF89a", 6);
    assert(!load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    write_bytes(path, corrupt_png, sizeof corrupt_png);
    assert(!load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof path, "%s/First.bmp", folder) < (int)sizeof path);
    write_cover_bmp(path, 600, 20);
    assert(load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    assert(image.data && image.width == 250 && image.height == 8);
    write_cover_bmp(path, 20, 440);
    assert(load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    assert(image.data && image.width == 11 && image.height == 250);
    write_cover_bmp(path, 650, 10);
    assert(!load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    assert(load_game_artwork(rom, ARTWORK_BOXART, &image));
    assert(image.data && image.width == 250);
    assert(unlink(path) == 0);
    assert(snprintf(path, sizeof path, "%s/First.rgb565", folder) < (int)sizeof path);
    static const uint16_t raw[64 * 64] = {0xf81f};
    write_bytes(path, raw, sizeof raw);
    assert(load_game_artwork_bounded(rom, ARTWORK_BOXART, &image));
    assert(image.data && !image.alpha && image.width == 64 && image.data[0] == 0xf81f);
    free_thumbnail(&image);
    puts("PASS native artwork lookup with bounded corrupt, unsupported, FIFO, oversized, extreme and raw fixtures");
}

static void test_enhanced_art(const char *root) {
    char theme_root[512], path[512];
    char packs_root[512];
    assert(snprintf(packs_root, sizeof packs_root, "%s/theme-packs", root) < (int)sizeof packs_root);
    assert(mkdir(packs_root, 0700) == 0);
    assert(snprintf(theme_root, sizeof theme_root, "%s/Pack", packs_root) < (int)sizeof theme_root);
    assert(mkdir(theme_root, 0700) == 0);
    strcpy(test_theme_pack_root, packs_root);
    assert(snprintf(test_legacy_icon, sizeof test_legacy_icon, "%s/icon.png", theme_root)
           < (int)sizeof test_legacy_icon);
    write_bytes(test_legacy_icon, icon_png, sizeof icon_png);
    snprintf(path, sizeof path, "%s/bad.png", theme_root);
    write_bytes(path, corrupt_png, sizeof corrupt_png);
    snprintf(path, sizeof path, "%s/wall.bmp", theme_root);
    write_full_screen_bmp(path);
    snprintf(path, sizeof path, "%s/theme.ini", theme_root);
    write_fixture(path, "[theme]\nmanifest_version=1\n"
                       "[resource.home.background]\npath=wall.bmp\n"
                       "[resource.platform.afirst.icon]\npath=icon.png\n"
                       "[resource.platform.noartwork.icon]\npath=bad.png\n"
                       "[resource.platform.noartwork.background]\npath=bad.png\n"
                       "[presentation]\nrecents_screenshot_dim=72\n"
                       "recents_shadow_opacity=190\ncoverflow_selection_opacity=170\n"
                       "home_selection_opacity=160\n"
                       "[resource.home.apps.icon]\npath=icon.png\n"
                       "[resource.home.favourites.icon]\npath=bad.png\n"
                       "[resource.platform.favourites.icon]\npath=icon.png\n"
                       "[resource.cover.placeholder]\npath=icon.png\n"
                       "[resource.achievement.badge]\npath=icon.png\n"
);

    theme_pack_count = 2;
    strcpy(theme_pack_files[1], "Pack");
    strcpy(theme_pack_disp[1], "Pack");
    settings_theme_pack_idx = 1;
    settings_render_scheme_idx = 0;
    settings_graphical_home = 1;
    enhanced_theme_active = -1;
    assert(enhanced_theme_sync());
    assert(enhanced_theme && strcmp(enhanced_theme_active_id, "Pack") == 0);
    settings_render_scheme_idx = 1; /* imported schemes keep their native policy */
    assert(enhanced_theme_sync() && !enhanced_theme);
    settings_render_scheme_idx = 0;
    assert(enhanced_theme_sync() && enhanced_theme);
    ThemeResourceDescriptor bad;
    ThemeResourceDescriptor placeholder, badge;
    assert(theme_resource_resolve(enhanced_theme, "platform.noartwork.icon", &bad));
    assert(theme_resource_resolve(enhanced_theme, "cover.placeholder", &placeholder));
    assert(placeholder.kind == THEME_RESOURCE_COVER_PLACEHOLDER);
    assert(theme_resource_resolve(enhanced_theme, "achievement.badge", &badge));
    assert(badge.kind == THEME_RESOURCE_ACHIEVEMENT_BADGE);
    int width, height;
    assert(!enhanced_decode_image(&bad, &width, &height, 4)); /* valid header, bad pixel data */

    settings_graphical_home = 0;
    assert(!system_icon_get("AFirst")); /* legacy still ignores the Enhanced mapping */
    system_icons_clear(); /* simulate switching the manifest at the live boundary */
    settings_graphical_home = 1;
    settings_backgrounds = 1;
    settings_wallpaper_idx = 0;
    SystemIcon *enhanced_icon = system_icon_get("AFirst");
    assert(enhanced_icon && strcmp(enhanced_icon->path, "icon.png") == 0);
    SystemIcon *fallback_icon = system_icon_get("NoArtwork");
    assert(fallback_icon && strcmp(fallback_icon->path, legacy_icon) == 0);
    strcpy(current_path, ROMS_PATH);
    prepare_cover_placeholder();
    assert(cover_placeholder_icon && strcmp(cover_placeholder_icon->path, "icon.png") == 0);
    prepare_achievement_placeholder();
    assert(achievement_badge_icon == cover_placeholder_icon);
    assert(system_icon_get(FAVOURITES_ENTRY_NAME) == cover_placeholder_icon);
    const ThemePresentation *visuals = theme_resource_presentation(enhanced_theme);
    assert(visuals->recents_screenshot_dim == 72 && visuals->recents_shadow_opacity == 190);
    assert(visuals->coverflow_selection_opacity == 170 && visuals->home_selection_opacity == 160);
    for (int i = 0; i < 6; i++) {
        settings_theme_pack_idx = 0;
        assert(enhanced_theme_sync() && !enhanced_theme);
        assert(!cover_placeholder_icon && !achievement_badge_icon);
        settings_theme_pack_idx = 1;
        assert(enhanced_theme_sync() && enhanced_theme);
        prepare_cover_placeholder();
        assert(cover_placeholder_icon);
    }
    puts("PASS P10 theme fallback, presentation values and repeated theme switches");


    /* A monochrome 640x480 fixture exercises the production decoder and
     * allocation path without including fonts or SD activity in the sample. */
    render_set_geometry(640, 480, 100);
    banner_set_dim(0);
    framebuffer = calloc(640 * 480, sizeof *framebuffer);
    assert(framebuffer);
    measure_allocations = true;
    load_banner_for_view("AFirst", false, false);
    measure_allocations = false;
    assert(banner_is_animating() && allocations > 0);
    printf("640x480 BMP load: %zu malloc calls, %zu requested bytes, largest %zu bytes\n",
           allocations, allocation_bytes, largest_allocation);
    measure_allocations = false;
    prepare_enhanced_grid();
    allocations = 0;
    measure_allocations = true;
    for (int frame = 0; frame < 480 && banner_is_animating(); frame++) {
        prepare_enhanced_grid();
        banner_render(framebuffer);
    }
    assert(allocations == 0 && banner_is_loaded() && !banner_is_animating());
    load_banner_for_view("AFirst", false, false); /* same resolved backdrop */
    measure_allocations = false;
    assert(allocations == 0 && banner_is_loaded() && !banner_is_animating());
    load_banner_for_view("NoArtwork", false, false); /* corrupt per-platform art */
    assert(banner_is_loaded() && !banner_is_animating());
    assert(framebuffer[0] == 0xf800);
    settings_backgrounds = 0;
    load_banner_for_view("AFirst", false, false);
    assert(!banner_is_loaded());
    settings_backgrounds = 1;
    settings_wallpaper_idx = 1;
    assert(!load_enhanced_home_background("AFirst"));
    settings_wallpaper_idx = 0;
    puts("PASS enhanced resource decode, native fallback, and zero mallocs on settled rendering");

    assert(snprintf(path, sizeof path, "%s/Legacy", packs_root) < (int)sizeof path);
    assert(mkdir(path, 0700) == 0);
    theme_pack_count = 3;
    strcpy(theme_pack_files[2], "Legacy");
    strcpy(theme_pack_disp[2], "Legacy");
    settings_theme_pack_idx = 2;
    assert(enhanced_theme_sync() && !enhanced_theme);
    assert(!banner_is_loaded() && !banner_is_animating());
    assert(!system_icon_get("AFirst"));
    assert(!enhanced_theme_sync());
    settings_save_file();
    settings_theme_pack_idx = 0;
    settings_load_file();
    assert(settings_theme_pack_idx == 2 && settings_graphical_home);
    assert(settings_enhanced_theme_present && strcmp(settings_enhanced_theme, "legacy") == 0);
    settings_theme_pack_idx = 1;
    assert(enhanced_theme_sync() && enhanced_theme);
    assert(system_icon_get("AFirst"));
    system_icons_clear();
    theme_resource_destroy(enhanced_theme);
    enhanced_theme = NULL;
    free(framebuffer);
    framebuffer = NULL;
}

static size_t sound_batches, sound_frames;
static int16_t first_sample;

static size_t capture_batch(const int16_t *pcm, size_t frames) {
    sound_batches++;
    sound_frames += frames;
    first_sample = pcm[0];
    for (size_t i = 0; i < frames; i++) assert(pcm[2 * i] == pcm[2 * i + 1]);
    return frames;
}

static void capture_sample(int16_t left, int16_t right) {
    assert(left == right);
    sound_frames++;
}

static void test_ui_sounds(const char *root) {
    char path[MAX_PATH_LEN];
    assert(snprintf(path, sizeof path, "%s/sound.ini", root) < (int)sizeof path);
    write_fixture(path, "[theme]\nmanifest_version=1\n"
                        "[resource.navigation.move.sound]\npath=valid.wav\n"
                        "[resource.navigation.cancel.sound]\npath=invalid.wav\n");
    unsigned char wave[] = {
        'R','I','F','F', 40,0,0,0, 'W','A','V','E',
        'f','m','t',' ', 16,0,0,0, 1,0,1,0, 0x44,0xac,0,0, 0x88,0x58,1,0, 2,0,16,0,
        'd','a','t','a', 4,0,0,0, 0x00,0x40,0x00,0xc0
    };
    assert(snprintf(path, sizeof path, "%s/valid.wav", root) < (int)sizeof path);
    write_bytes(path, wave, sizeof wave);
    assert(snprintf(path, sizeof path, "%s/invalid.wav", root) < (int)sizeof path);
    wave[22] = 2; /* Unsupported stereo and inconsistent byte/block layout. */
    write_bytes(path, wave, sizeof wave);
    ThemeResourceManager *theme = theme_resource_create(root, THEME_RESOURCE_CACHE_MAX);
    assert(theme && theme_resource_load_manifest(theme, "sound.ini"));

    int ui_row = -1;
    for (int i = 0; i < SETTINGS_ROW_N; i++)
        if (settings_rows[i].val == &settings_ui_sounds) ui_row = i;
    assert(ui_row >= 0);
    settings_graphical_home = 0;
    assert(!settings_row_available(&settings_rows[ui_row]));
    settings_ui_sounds = 1;
    settings_save_file();
    settings_ui_sounds = 0;
    settings_load_file();
    assert(!settings_graphical_home && settings_ui_sounds);
    settings_graphical_home = 1;
    assert(settings_row_available(&settings_rows[ui_row]) && ui_audio_enabled());

    ui_prepare_sounds(theme, true);
    assert(ui_sounds[UI_SOUND_MOVE].frames == 2);
    assert(ui_sounds[UI_SOUND_MOVE].pcm[0] == 4096 && ui_sounds[UI_SOUND_MOVE].pcm[1] == -4096);
    assert(ui_sounds[UI_SOUND_CONFIRM].frames > 2400);
    assert(ui_sounds[UI_SOUND_CANCEL].frames > 2400);
    assert(ui_navigation_sound(false, false, true, false, false) == UI_SOUND_COUNT);
    assert(ui_navigation_sound(false, true, false, false, false) == UI_SOUND_COUNT);
    assert(ui_navigation_sound(false, true, true, false, false) == UI_SOUND_MOVE);
    assert(ui_navigation_sound(true, true, true, true, false) == UI_SOUND_CONFIRM);
    assert(ui_navigation_sound(true, true, false, false, true) == UI_SOUND_CANCEL);
    assert(ui_navigation_sound(true, false, false, false, false) == UI_SOUND_SELECT);
    retro_set_audio_sample_batch(capture_batch);
    ui_play_enhanced_sound(UI_SOUND_MOVE, 1000);
    assert(sound_batches == 1 && sound_frames == 2 && first_sample == 4096);
    ui_play_enhanced_sound(UI_SOUND_CONFIRM, 1060);
    ui_play_enhanced_sound(UI_SOUND_CANCEL, 1070);
    assert(sound_batches == 2);
    ui_prepare_sounds(NULL, true);
    assert(ui_sounds[UI_SOUND_MOVE].frames == SAMPLE_RATE * 60 / 1000);
    settings_ui_sounds = 0;
    ui_play_enhanced_sound(UI_SOUND_MOVE, 2000);
    assert(sound_batches == 2);
    settings_ui_sounds = 1;
    settings_graphical_home = 0;
    settings_menu_sounds = 1;
    ui_play_enhanced_sound(UI_SOUND_MOVE, 2000);
    assert(sound_batches == 2);
    ui_menu_tick();
    assert(sound_batches == 3);
    retro_set_audio_sample_batch(NULL);
    retro_set_audio_sample(capture_sample);
    size_t start_frames = sound_frames;
    settings_graphical_home = 1;
    ui_play_enhanced_sound(UI_SOUND_MOVE, 2100);
    assert(sound_frames - start_frames == ui_sounds[UI_SOUND_MOVE].frames);
    retro_set_audio_sample(NULL);
    settings_ui_sounds = 0;
    settings_menu_sounds = 0;
    ui_prepare_sounds(NULL, false);
    theme_resource_destroy(theme);
    puts("PASS audio WAV validation, event gating, fallback, persistence, and Libretro callbacks");
}

static void test_ribbon(const char *root) {
    char path[MAX_PATH_LEN];
    for (int i = 0; i < 7; i++) {
        assert(snprintf(path, sizeof path, "%s/Ribbon%d", root, i) < (int)sizeof path);
        assert(mkdir(path, 0700) == 0);
        assert(snprintf(path, sizeof path, "%s/Ribbon%d/Game.nes", root, i) < (int)sizeof path);
        write_fixture(path, "");
    }
    settings_graphical_home = 1;
    settings_anim = 1;
    settings_apps_in_settings = 0;
    scan_directory(ROMS_PATH);
    assert(entry_count > ENHANCED_HOME_VISIBLE);
    framebuffer = calloc(640 * 480, sizeof *framebuffer);
    assert(framebuffer);
    for (int count = 0; count <= 3; count++) {
        for (int row = 0; row < (count ? count : 1); row++) {
            render_enhanced_recents_chevrons(framebuffer, row, count);
            assert(framebuffer[240 * 640 + UI_S(6)] ==
                   (row > 0 ? COLOR_TEXT : COLOR_DISABLED));
            assert(framebuffer[240 * 640 + 640 - UI_S(6) - UI_S(2)] ==
                   (row + 1 < count ? COLOR_TEXT : COLOR_DISABLED));
        }
    }
    enhanced_home_kind = ENHANCED_HOME_FAVOURITES;
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer); /* Offline export absent. */
    assert(selected_index == 0 && scroll_offset == 0);
    press(FROG_BTN_LEFT);
    assert(selected_index == 0);

    press(FROG_BTN_RIGHT);
    prepare_enhanced_grid();
    assert(selected_index == 0 && scroll_offset == 0);
    assert(enhanced_grid_first == 0 && enhanced_grid_count == ENHANCED_HOME_VISIBLE);
    assert(enhanced_home_center_offset(0, 9) == 0);
    assert(enhanced_home_center_offset(4, 9) == 2);
    assert(enhanced_home_center_offset(8, 9) == 4);
    assert(enhanced_home_center_offset(3, 4) == 0);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_grid();
    assert(selected_index == 1 && scroll_offset == 0);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_grid();
    assert(selected_index == 2 && scroll_offset == 1);
    for (int i = 0; i < 5; i++) render_enhanced_home(framebuffer);
    float position = enhanced_home_visible_offset;
    assert(position > 0.0f && position < 1.0f);
    press(FROG_BTN_LEFT);
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer);
    assert(enhanced_home_visible_offset == position);
    for (int i = 0; i < SYSTEM_CAROUSEL_FRAMES; i++) render_enhanced_home(framebuffer);
    assert(enhanced_home_visible_offset == 0.0f && !enhanced_home_is_animating());

    for (int i = 0; i < entry_count + 3; i++) {
        press(FROG_BTN_RIGHT);
        prepare_enhanced_grid();
        render_enhanced_home(framebuffer);
        assert(enhanced_grid_count <= SYSTEM_GRID_PAGE);
    }
    int platforms = enhanced_home_platform_count();
    assert(selected_index == entry_count - 1);
    assert(enhanced_home_kind == ENHANCED_HOME_SETTINGS);
    assert(enhanced_home_index(platforms) == platforms + 3);
    assert(scroll_offset == platforms + 4 - ENHANCED_HOME_VISIBLE);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_SETTINGS);
    press(FROG_BTN_A);
    assert(settings_menu_active);
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_SETTINGS);

    press(FROG_BTN_LEFT);
    assert(enhanced_home_kind == ENHANCED_HOME_APPS);
    press(FROG_BTN_START);
    assert(settings_menu_active);
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_APPS);
    settings_apps_in_settings = 1;
    press(FROG_BTN_A);
    assert(viewing_apps);
    press(FROG_BTN_START);
    assert(settings_menu_active && enhanced_settings_navigation.state.settings_return == NAV_APPS);
    press(FROG_BTN_B);
    assert(viewing_apps);
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_APPS);
    settings_apps_in_settings = 0;
    press(FROG_BTN_R1);
    assert(viewing_apps);
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_APPS);

    press(FROG_BTN_LEFT);
    assert(enhanced_home_kind == ENHANCED_HOME_RECENTS);
    press(FROG_BTN_START);
    assert(settings_menu_active);
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_RECENTS);
    press(FROG_BTN_A);
    assert(viewing_recents && entry_count >= 2);
    settings_game_switcher = 1;
    game_switcher_fullscreen = true;
    selected_index = 1;
    press(FROG_BTN_START);
    assert(settings_menu_active && enhanced_settings_navigation.state.settings_return == NAV_RECENTS);
    press(FROG_BTN_B);
    assert(viewing_recents && selected_index == 1 && game_switcher_fullscreen);
    press(FROG_BTN_R1);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_RECENTS);
    press(FROG_BTN_A);
    assert(viewing_recents);
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_RECENTS);

    settings_file_cache = 1;
    scan_directory(ROMS_PATH);
    int stored_count = entry_count;
    press(FROG_BTN_START);
    assert(settings_menu_active);
    assert(snprintf(path, sizeof path, "%s/RibbonEmpty", root) < (int)sizeof path);
    assert(mkdir(path, 0700) == 0);
    settings_hide_empty = 0;
    select_enhanced_setting(SR_REBUILD_CACHE);
    press(FROG_BTN_A);
    assert(!settings_menu_active && entry_count == stored_count + 1);
    prepare_enhanced_grid();
    assert(enhanced_home_kind == ENHANCED_HOME_RECENTS);
    assert(enhanced_home_index(enhanced_home_platform_count()) == platforms + 2);
    assert(scroll_offset == enhanced_home_platform_count() + 4 - ENHANCED_HOME_VISIBLE);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_APPS);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_SETTINGS);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_SETTINGS);

    ThemeResourceManager *theme = theme_resource_create(root, THEME_RESOURCE_CACHE_MAX);
    assert(theme);
    assert(snprintf(path, sizeof path, "%s/reduced.ini", root) < (int)sizeof path);
    write_fixture(path, "[theme]\nmanifest_version=1\n[presentation]\nreduced_motion=on\n");
    assert(theme_resource_load_manifest(theme, "reduced.ini"));
    enhanced_theme = theme;
    press(FROG_BTN_LEFT);
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer);
    assert(!enhanced_home_is_animating() && enhanced_home_visible_offset == scroll_offset);
    theme_resource_destroy(enhanced_theme);
    enhanced_theme = NULL;
    /* The native Favorites sentinel is not a platform in the projection. */
    strcpy(entries[0].name, FAVOURITES_ENTRY_NAME);
    entries[0].is_dir = 0;
    assert(enhanced_home_platform_count() == entry_count - 1);
    enhanced_home_kind = ENHANCED_HOME_PLATFORM;
    selected_index = 1;
    prepare_enhanced_grid();
    assert(enhanced_home_index(enhanced_home_platform_count()) == 1);
    assert(strcmp(enhanced_grid_names[1], entries[1].name) == 0);

    /* Zero and one platform both retain Favorites and the three trailing utilities. */
    entries[0] = entries[1];
    entry_count = 1;
    selected_index = scroll_offset = 0;
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer);
    assert(enhanced_grid_count == 5 && enhanced_home_kind == ENHANCED_HOME_PLATFORM);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_RECENTS);
    entry_count = 0;
    enhanced_home_kind = ENHANCED_HOME_FAVOURITES;
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer);
    assert(enhanced_grid_count == 4 && enhanced_home_kind == ENHANCED_HOME_FAVOURITES);
    press(FROG_BTN_LEFT);
    assert(enhanced_home_kind == ENHANCED_HOME_FAVOURITES);
    press(FROG_BTN_RIGHT);
    press(FROG_BTN_RIGHT);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_SETTINGS);
    free(framebuffer);
    framebuffer = NULL;
    puts("PASS unified ribbon, Settings context, utility routes, cache rebuild, and reduced motion");
}

static void test_coverflow(const char *root) {
    enum { COUNT = 1500 };
    const size_t bytes = (size_t)640 * 480 * sizeof(uint16_t);
    framebuffer = calloc(1, bytes);
    assert(framebuffer);
    settings_graphical_home = 1;
    settings_anim = 1;
    settings_hide_extensions = 1;
    assert(snprintf(current_path, sizeof current_path, "%s/AFirst", root) < (int)sizeof current_path);
    DirEntry *games = realloc(entries, COUNT * sizeof(*games));
    assert(games);
    entries = games;
    entry_capacity = COUNT;
    entry_count = COUNT;
    memset(entries, 0, COUNT * sizeof(*entries));
    for (int i = 0; i < COUNT; i++) snprintf(entries[i].name, sizeof entries[i].name, "ROM%04d.nes", i);
    for (int i = 0; i < 12; i++) {
        char path[MAX_PATH_LEN];
        assert(snprintf(path, sizeof path, "%s/.res/ROM%04d.bmp", current_path, i) < (int)sizeof path);
        write_cover_bmp(path, 250, 250);
    }
    selected_index = 0;
    prepare_enhanced_library(true);
    render_enhanced_library(framebuffer);
    press(FROG_BTN_LEFT);
    assert(selected_index == 0);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_library(false);
    assert(selected_index == 1 && coverflow_animating());
    for (int i = 0; i < 4; i++) render_enhanced_library(framebuffer);
    float interrupted = coverflow_visible;
    assert(interrupted > 0 && interrupted < 1);
    press(FROG_BTN_LEFT);
    prepare_enhanced_library(false);
    assert(selected_index == 0 && coverflow_from == interrupted);
    for (int i = 0; i < SYSTEM_CAROUSEL_FRAMES; i++) render_enhanced_library(framebuffer);
    assert(!coverflow_animating() && coverflow_visible == 0);

    size_t largest_retained = 0, native_draw_allocations = 0;
    long long start = monotonic_ms();
    for (int step = 0; step < 180; step++) {
        press(step % 20 < 13 ? FROG_BTN_RIGHT : FROG_BTN_LEFT);
        prepare_enhanced_library(false);
        assert(coverflow_last - coverflow_first <= BOXART_FLOW_SLOTS);
        size_t retained = 0;
        for (int slot = 0; slot < BOXART_FLOW_SLOTS; slot++) {
            const BoxartCacheEntry *image = &boxart_cache[slot];
            retained += (size_t)image->thumb.width * image->thumb.height * (2 + (image->alpha != NULL));
        }
        assert(retained <= BOXART_FLOW_SLOTS * BOXART_FLOW_EDGE * BOXART_FLOW_EDGE * 3u);
        if (retained > largest_retained) largest_retained = retained;
        unsigned cached_before_paint = boxart_age;
        allocations = 0;
        measure_allocations = true;
        render_enhanced_library(framebuffer);
        measure_allocations = false;
        native_draw_allocations += allocations;
        assert(boxart_age == cached_before_paint);
    }
    printf("CoverFlow fixture: %d ROMs, 180 input/paint steps in %lld ms, max retained pixels %zu bytes\n",
           COUNT, monotonic_ms() - start, largest_retained);
    printf("CoverFlow draw: %zu native malloc calls over 180 frames, zero artwork cache accesses\n",
           native_draw_allocations);
    assert(largest_retained > 0);

    char changed_cover[MAX_PATH_LEN];
    assert(snprintf(changed_cover, sizeof changed_cover, "%s/.res/ROM0000.bmp", current_path)
           < (int)sizeof changed_cover);
    selected_index = 0;
    prepare_enhanced_library(false);
    assert(coverflow_images[0]);
    assert(unlink(changed_cover) == 0);
    prepare_enhanced_library(false);
    assert(coverflow_images[0]); /* Warm paint and unchanged selection never probe SD. */
    ThemeResourceManager *theme = theme_resource_create(root, THEME_RESOURCE_CACHE_MAX);
    assert(theme && theme_resource_load_manifest(theme, "reduced.ini"));
    enhanced_theme = theme;
    prepare_enhanced_library(true);
    assert(!coverflow_images[0]); /* Same-platform pack switch dropped stale pixels. */
    press(FROG_BTN_RIGHT);
    prepare_enhanced_library(false);
    assert(selected_index == 1 && !coverflow_animating());
    enhanced_theme = NULL;
    theme_resource_destroy(theme);
    prepare_enhanced_library(true);

    settings_anim = 0;
    selected_index = COUNT - 1;
    prepare_enhanced_library(false);
    press(FROG_BTN_RIGHT);
    assert(selected_index == COUNT - 1 && !coverflow_animating());
    render_enhanced_library(framebuffer);
    assert(coverflow_visible == COUNT - 1);
    for (int pass = 0; pass < 3; pass++) {
        strcpy(current_path, ROMS_PATH);
        prepare_enhanced_library(false);
        for (int slot = 0; slot < BOXART_FLOW_SLOTS; slot++) assert(!boxart_cache[slot].path[0]);
        assert(snprintf(current_path, sizeof current_path, "%s/NoArtwork", root) < (int)sizeof current_path);
        entry_count = pass;
        selected_index = 0;
        prepare_enhanced_library(true);
        render_enhanced_library(framebuffer);
        assert(coverflow_last == pass);
    }
    boxart_clear();
    free(framebuffer);
    framebuffer = NULL;
    puts("PASS bounded CoverFlow cache, interrupted navigation, edge cases and library lifecycle");
}

static unsigned ribbon_frames;

static void capture_ribbon(const void *pixels, unsigned width, unsigned height, size_t pitch) {
    assert(pixels && width == 640 && height == 480 && pitch == 1280);
    ribbon_frames++;
}

static void ribbon_step(FrogButton button, int expected, int platforms) {
    unsigned before = ribbon_frames;
    int previous = enhanced_home_index(platforms);
    input_set_ext_raw(1u << input_get_raw_bit(button));
    retro_run();
    input_set_ext_raw(0);
    retro_run();
    assert(enhanced_home_index(platforms) == expected);
    int total = platforms + 4;
    int offset = expected - 2;
    if (offset < 0) offset = 0;
    if (offset > total - ENHANCED_HOME_VISIBLE) offset = total - ENHANCED_HOME_VISIBLE;
    if (offset < 0) offset = 0;
    assert(scroll_offset == offset && enhanced_home_visible_offset == (float)offset);
    if (expected != previous) {
        if (ribbon_frames == before)
            fprintf(stderr, "Home selection %d/%d changed without presenting a frame\n",
                    expected, platforms + 3);
        assert(ribbon_frames > before);
    }
}

static void test_home_ribbon_presentation(void) {
    settings_graphical_home = 1;
    settings_anim = 0;
    settings_backgrounds = 0;
    settings_file_cache = 0;
    ui_toast_frames = 0;
    view_transition_frame = VIEW_TRANSITION_FRAMES + 1;
    strcpy(current_path, ROMS_PATH);
    enhanced_home_kind = ENHANCED_HOME_PLATFORM;
    scan_directory(ROMS_PATH);
    int full_count = entry_count;
    assert(enhanced_home_platform_count() > 5 && enhanced_home_platform_first() == 0);
    framebuffer = calloc(640 * 480, sizeof(*framebuffer));
    assert(framebuffer);
    retro_set_video_refresh(capture_ribbon);
    const int counts[] = {full_count, 0, 1};
    for (size_t i = 0; i < sizeof counts / sizeof counts[0]; i++) {
        entry_count = counts[i];
        selected_index = 0;
        enhanced_home_kind = ENHANCED_HOME_FAVOURITES;
        enhanced_grid_prepared = false;
        retro_run();
        int platforms = enhanced_home_platform_count();
        for (int index = 1; index < platforms + 4; index++)
            ribbon_step(FROG_BTN_RIGHT, index, platforms);
        ribbon_step(FROG_BTN_RIGHT, platforms + 3, platforms);
        for (int index = platforms + 2; index >= 0; index--)
            ribbon_step(FROG_BTN_LEFT, index, platforms);
        ribbon_step(FROG_BTN_LEFT, 0, platforms);
    }
    settings_graphical_home = 0;
    settings_explore_layout = STYLE_HORIZONTAL;
    entry_count = full_count;
    selected_index = 0;
    assert(horizontal_system_view());
    press(FROG_BTN_LEFT);
    assert(selected_index == full_count - 1); /* Classic circular navigation. */
    press(FROG_BTN_RIGHT);
    assert(selected_index == 0);
    retro_set_video_refresh(NULL);
    free(framebuffer);
    framebuffer = NULL;
    puts("PASS Home utility selection presents a frame even at a settled scroll offset");
}

static void check_csv_title(const char *filename, const char *title) {
    int index = visible_entry_named(filename);
    assert(index >= 0);
    selected_index = index;
    prepare_enhanced_library(false);
    assert(!strcmp(coverflow_title_text, title));
}

static void test_platform_filelists(const char *root) {
    char platform[MAX_PATH_LEN], csv[MAX_PATH_LEN], nested[MAX_PATH_LEN], path[MAX_PATH_LEN];
    assert(snprintf(platform, sizeof platform, "%s/Filelist", root) < (int)sizeof platform);
    assert(mkdir(platform, 0700) == 0);
    assert(snprintf(nested, sizeof nested, "%s/sub", platform) < (int)sizeof nested);
    assert(mkdir(nested, 0700) == 0);
    assert(snprintf(csv, sizeof csv, "%s/filelist.csv", platform) < (int)sizeof csv);
    const char *games[] = {"A, Game.nes", "Empty.nes", "Plain.nes", "Bad.nes", "Large.nes", "Quotes.nes"};
    for (size_t i = 0; i < sizeof games / sizeof games[0]; i++) {
        assert(snprintf(path, sizeof path, "%s/%s", platform, games[i]) < (int)sizeof path);
        write_fixture(path, "");
    }
    assert(snprintf(path, sizeof path, "%s/Plain.nes", nested) < (int)sizeof path);
    write_fixture(path, "");
    settings_graphical_home = 1;
    settings_hide_extensions = 0;
    settings_file_cache = 0;
    strcpy(current_path, platform);
    scan_directory(current_path);
    int discovered = entry_count;
    check_csv_title("A, Game.nes", "A, Game");
    write_fixture(csv, "");
    scan_directory(current_path);
    check_csv_title("A, Game.nes", "A, Game");
    write_fixture(csv, "Rom.ext,Display Title,ChineseTitle\n");
    scan_directory(current_path);
    check_csv_title("A, Game.nes", "A, Game");
    FILE *file = fopen(csv, "wb");
    assert(file);
    fputs("\xef\xbb\xbfRom.ext,Display Title,ChineseTitle\r\n"
          "\"A, Game.nes\",\"Pokémon, edición\",\r\n"
          "\"A, Game.nes\",Duplicate,\r\n"
          "Empty.nes,,\r\n"
          "Missing.nes,Phantom,\r\n"
          "sub/Plain.nes,Nested,\r\n"
          "Bad.nes,\xff,\r\n"
          "Large.nes,", file);
    for (int i = 0; i < 300; i++) fputc('Z', file);
    fputs(",\r\n"
          "Plain.nes,\"unclosed\r\n"
          "Quotes.nes,\"The \"\"Game\"\"\",\r\n", file);
    assert(fclose(file) == 0);
    scan_directory(current_path);
    assert(entry_count == discovered && visible_entry_named("Missing.nes") < 0);
    check_csv_title("A, Game.nes", "Pokémon, edición");
    check_csv_title("Empty.nes", "Empty");
    check_csv_title("Plain.nes", "Plain");
    check_csv_title("Bad.nes", "Bad");
    check_csv_title("Large.nes", "Large");
    check_csv_title("Quotes.nes", "The \"Game\"");
    file = fopen(csv, "wb");
    assert(file);
    fputs("Rom.ext,Display Title,ChineseTitle,Developer,Description\n"
          "Large.nes,Retained title,,Studio,", file);
    for (int i = 0; i < 300; i++) fputc('Z', file);
    fputs("\nQuotes.nes,Valid title,,Studio,", file);
    for (int i = 0; i < 250; i++) fputc('A', file);
    assert(fclose(file) == 0);
    scan_directory(current_path);
    check_csv_title("Large.nes", "Retained title");
    const char *value = filelist_details + filelist_detail_offsets[selected_index] - 1;
    for (int i = 0; i < FILELIST_DESCRIPTION; i++) value += strlen(value) + 1;
    assert(!*value);
    check_csv_title("Quotes.nes", "Valid title");
    value = filelist_details + filelist_detail_offsets[selected_index] - 1;
    for (int i = 0; i < FILELIST_DESCRIPTION; i++) value += strlen(value) + 1;
    assert(strlen(value) == 250);
    strcpy(current_path, nested);
    scan_directory(current_path);
    check_csv_title("Plain.nes", "Plain");

    assert(snprintf(path, sizeof path, "%s/AFirst/filelist.csv", root) < (int)sizeof path);
    write_fixture(path, "First.nes,Independent,\n");
    assert(snprintf(current_path, sizeof current_path, "%s/AFirst", root) < (int)sizeof current_path);
    scan_directory(current_path);
    check_csv_title("First.nes", "Independent");
    const Thumbnail *art = coverflow_images[selected_index - coverflow_first];
    assert(art && art->data[0] == 0xf81f); /* The native First.rgb565, not the title. */
    settings_file_cache = 1;
    scan_directory(current_path);
    write_fixture(path, "First.nes,Retitled,\n");
    scan_directory(current_path);
    check_csv_title("First.nes", "Retitled");
    settings_file_cache = 0;
    settings_graphical_home = 0;
    scan_directory(current_path);
    assert(!filelist_offsets && !strcmp(entries[0].name, "First.nes"));

    settings_graphical_home = 1;
    strcpy(current_path, platform);
    for (int i = 0; i < 1500; i++) {
        assert(snprintf(path, sizeof path, "%s/Item%04d.nes", platform, i) < (int)sizeof path);
        write_fixture(path, "");
    }
    assert(unlink(csv) == 0);
    long long start = monotonic_ms();
    scan_directory(current_path);
    long long without = monotonic_ms() - start;
    assert(entry_count == discovered + 1500);
    file = fopen(csv, "wb");
    assert(file);
    fputs("Rom.ext,Display Title,ChineseTitle,Description\n", file);
    for (int i = 0; i < 1500; i++)
        fprintf(file, "Item%04d.nes,Title %04d,,Summary %04d\n", i, i, i);
    assert(fclose(file) == 0);
    start = monotonic_ms();
    scan_directory(current_path);
    long long with = monotonic_ms() - start;
    check_csv_title("Item1499.nes", "Title 1499");
    printf("Filelist fixture: 1500 mapped ROMs, %zu bytes retained, scans %lld ms without / %lld ms with CSV\n",
           filelist_capacity + filelist_detail_capacity +
           (size_t)entry_count * (sizeof(*filelist_offsets) + sizeof(*filelist_detail_offsets)),
           without, with);
    file = fopen(csv, "wb");
    assert(file && ftruncate(fileno(file), 1024 * 1024 + 1) == 0);
    assert(fclose(file) == 0);
    scan_directory(current_path);
    check_csv_title("Item1499.nes", "Item1499");
    strcpy(current_path, ROMS_PATH);
    scan_directory(current_path);
    prepare_enhanced_library(false);
    assert(!filelist_offsets && !filelist_titles);
    puts("PASS per-platform CSV scanner, title resolution, fallback and resource limits");
}

static void test_ra_home(const char *root) {
    char directory[MAX_PATH_LEN], profile[MAX_PATH_LEN], avatar[MAX_PATH_LEN];
    assert(snprintf(directory, sizeof directory, "%s/ra", root) < (int)sizeof directory);
    load_ra_home(directory);
    assert(!ra_home_profile_valid);
    assert(mkdir(directory, 0700) == 0);
    assert(snprintf(profile, sizeof profile, "%s/profile.json", directory) < (int)sizeof profile);
    assert(snprintf(avatar, sizeof avatar, "%s/avatar.png", directory) < (int)sizeof avatar);
    write_fixture(profile, "{\"avatar\":\"avatar.png\",\"username\":\"Pilot\\u00e9\"}");
    load_ra_home(directory);
    assert(ra_home_profile_valid && !ra_avatar_icon.pixels);
    assert(!strcmp(ra_home_profile.username, "Piloté"));
    write_bytes(avatar, corrupt_png, sizeof corrupt_png);
    load_ra_home(directory);
    assert(ra_home_profile_valid && !ra_avatar_icon.pixels);
    write_bytes(avatar, icon_png, sizeof icon_png);
    load_ra_home(directory);
    assert(ra_home_profile_valid && ra_avatar_icon.pixels && ra_avatar_icon.width == RA_AVATAR_EDGE);
    assert(unlink(profile) == 0 && unlink(avatar) == 0);
    uint16_t *frame = calloc(640 * 480, sizeof(*frame));
    assert(frame);
    load_ra_trophy();
    assert(ra_trophy_icon.pixels && ra_trophy_icon.alpha[0] == 0);
    for (int i = 0; i < 4; i++) render_ra_home(frame);
    size_t painted = 0;
    bool left_pill = false, right_pill = false, central_gap_clear = true;
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++)
            if (frame[y * 640 + x]) {
                assert(y >= 39 && y < 97);
                if (x < 120) left_pill = true;
                if (x >= 310) right_pill = true;
                if (x >= 120 && x < 310) central_gap_clear = false;
                painted++;
            }
    assert(painted > 0 && left_pill && right_pill && central_gap_clear);
    free(frame);
    load_ra_home(directory);
    assert(!ra_home_profile_valid && !ra_avatar_icon.pixels);
    write_fixture(profile, "{\"avatar\":null,\"username\":\"Valid\",\"schema_version\":9}");
    load_ra_home(directory);
    assert(!ra_home_profile_valid);
    write_fixture(profile, "{\"avatar\":null,\"username\":\"broken\"");
    load_ra_home(directory);
    assert(!ra_home_profile_valid);
    FILE *file = fopen(profile, "wb");
    assert(file);
    fputs("{\"avatar\":null,\"username\":\"", file);
    for (int i = 0; i < 81; i++) fputc('P', file);
    fputs("\"}", file);
    assert(fclose(file) == 0);
    load_ra_home(directory);
    assert(!ra_home_profile_valid);
    file = fopen(profile, "wb");
    assert(file && ftruncate(fileno(file), 4097) == 0);
    assert(fclose(file) == 0);
    load_ra_home(directory);
    assert(!ra_home_profile_valid);
    puts("PASS offline RA identity, avatar validation, snapshot drawing, and unsupported data");
}

static void ra_game_fixture(const char *root, bool create) {
    static const char *directories[] = {
        "cubegm", "cubegm/enhanced", "cubegm/enhanced/ra", "cubegm/enhanced/ra/cache",
        "cubegm/enhanced/ra/cache/7",
        "cubegm/enhanced/ra/cache/7/0123456789abcdef0123456789abcdef"
    };
    char path[1024];
    if (!create) {
        snprintf(path, sizeof path, "%s/%s/gamepack.json", root, directories[5]);
        assert(unlink(path) == 0);
        snprintf(path, sizeof path, "%s/cubegm/enhanced/ra/rom_identity.json", root);
        assert(unlink(path) == 0);
        for (size_t i = sizeof directories / sizeof directories[0]; i > 0; i--) {
            snprintf(path, sizeof path, "%s/%s", root, directories[i - 1]);
            assert(rmdir(path) == 0);
        }
        return;
    }
    for (size_t i = 0; i < sizeof directories / sizeof directories[0]; i++) {
        assert(snprintf(path, sizeof path, "%s/%s", root, directories[i]) < (int)sizeof path);
        assert(mkdir(path, 0700) == 0);
    }
    assert(snprintf(path, sizeof path, "%s/cubegm/enhanced/ra/rom_identity.json", root) <
           (int)sizeof path);
    char rom[1024], identity[1024];
    struct stat st;
    assert(snprintf(rom, sizeof rom, "%s/NoArtwork/Example.nes", root) < (int)sizeof rom);
    assert(stat(rom, &st) == 0);
    assert(snprintf(identity, sizeof identity,
        "{\"Other/Rom.NES\":{\"size\":1,\"metadata_only\":true},"
        "\"NoArtwork/Example.nes\":{\"platform\":\"NoArtwork\",\"console_id\":7,"
        "\"game_id\":40,\"ra_hash\":\"0123456789abcdef0123456789abcdef\","
        "\"method\":\"rcheevos_compatible\",\"size\":0,\"mtime_ns\":%lld,"
        "\"metadata_only\":true}}", (long long)st.st_mtime * 1000000000LL) < (int)sizeof identity);
    write_fixture(path, identity);
    assert(snprintf(path, sizeof path, "%s/%s/gamepack.json", root, directories[5]) <
           (int)sizeof path);
    write_fixture(path,
        "{\"schema_version\":1,\"console_id\":7,\"game_id\":40,"
        "\"ra_hash\":\"0123456789abcdef0123456789abcdef\",\"metadata_only\":true,"
        "\"achievements\":[{\"id\":1,\"title\":\"First step\",\"points\":5},"
        "{\"id\":2,\"title\":\"Second step\",\"description\":\"Explore the cave\",\"points\":10}]}");
}

static void test_apps_coverflow(const char *root) {
    char path[1024], assets[1024];
    assert(snprintf(path, sizeof path, "%s/Ebook", root) < (int)sizeof path);
    assert(mkdir(path, 0700) == 0);
    assert(snprintf(assets, sizeof assets, "%s/.app-icons", root) < (int)sizeof assets);
    assert(mkdir(assets, 0700) == 0);
    assert(snprintf(path, sizeof path, "%s/theme.ini", assets) < (int)sizeof path);
    write_fixture(path, "[theme]\nmanifest_version=1\n"
                        "[resource.app.usbmode.icon]\npath=valid.png\n"
                        "[resource.app.ebook.icon]\npath=corrupt.png\n");
    assert(snprintf(path, sizeof path, "%s/valid.png", assets) < (int)sizeof path);
    write_bytes(path, icon_png, sizeof icon_png);
    assert(snprintf(path, sizeof path, "%s/corrupt.png", assets) < (int)sizeof path);
    write_bytes(path, corrupt_png, sizeof corrupt_png);
    assert(!enhanced_theme);
    enhanced_theme = theme_resource_create(assets, THEME_RESOURCE_CACHE_MAX);
    assert(enhanced_theme && theme_resource_load_manifest(enhanced_theme, "theme.ini"));
    assert(!framebuffer);
    framebuffer = calloc(640 * 480, sizeof *framebuffer);
    assert(framebuffer);

    test_app_executables = true;
    assert(setenv("TF_DEVICE", "R36SX", 1) == 0);
    settings_graphical_home = 1;
    settings_apps_in_settings = 0;
    viewing_recents = viewing_apps = viewing_favourites = viewing_search = false;
    strcpy(current_path, ROMS_PATH);
    scan_directory(current_path);
    enhanced_home_kind = ENHANCED_HOME_APPS;
    press(FROG_BTN_A);
    assert(enhanced_apps_view() && entry_count >= 4);

    int usb = visible_entry_named("usbmode"), ebook = visible_entry_named("ebook");
    assert(usb >= 0 && ebook >= 0);
    selected_index = usb;
    prepare_enhanced_library(true);
    assert(coverflow_apps && coverflow_images[usb - coverflow_first]);
    assert(!coverflow_images[ebook - coverflow_first]);
    render_enhanced_apps(framebuffer);
    press(FROG_BTN_A);
    assert(usb_mode_confirm_active && !usb_mode_initiated_active);
    press(FROG_BTN_B);
    assert(!usb_mode_confirm_active && enhanced_apps_view() && selected_index == usb);

    selected_index = 0;
    press(FROG_BTN_LEFT);
    assert(selected_index == 0);
    int motion_before = settings_anim;
    settings_anim = true;
    prepare_enhanced_library(false);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_library(false);
    assert(selected_index == 1 && coverflow_animating());
    settings_anim = false;
    prepare_enhanced_library(false);
    assert(!coverflow_animating());
    settings_anim = motion_before;
    selected_index = entry_count - 1;
    press(FROG_BTN_RIGHT);
    assert(selected_index == entry_count - 1);

    selected_index = ebook;
    press(FROG_BTN_A);
    assert(apps_browsing && !enhanced_apps_view());
    press(FROG_BTN_B);
    assert(enhanced_apps_view() && selected_index == ebook);

    const char *keys[] = {"reboot", "shutdown"};
    const char *bin[] = {REBOOT_BIN, SHUTDOWN_BIN};
    for (int i = 0; i < 2; i++) {
        selected_index = visible_entry_named(keys[i]);
        assert(selected_index >= 0);
        press(FROG_BTN_A);
        /* The wrapped launcher writes only to the fixture. There is no runtime callback. */
        FILE *file = fopen(LAUNCH_FILE, "r");
        char line[512];
        assert(file && fgets(line, sizeof line, file) && !strcmp(line, "standalone\n"));
        assert(fgets(line, sizeof line, file));
        line[strcspn(line, "\n")] = 0;
        assert(!strcmp(line, bin[i]));
        fclose(file);
    }

    assert(setenv("TF_DEVICE", "SF3000", 1) == 0);
    scan_apps_tab();
    assert(visible_entry_named("shutdown") < 0);
    assert(unsetenv("TF_DEVICE") == 0);
    int count = entry_count;
    for (int small = 0; small <= 1; small++) {
        entry_count = small;
        selected_index = 0;
        prepare_enhanced_library(true);
        render_enhanced_apps(framebuffer);
        press(FROG_BTN_LEFT);
        press(FROG_BTN_RIGHT);
        assert(selected_index == 0);
    }
    entry_count = count;
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_APPS);

    settings_apps_in_settings = 1;
    open_enhanced_settings();
    select_enhanced_setting(SR_OPEN_APPS);
    press(FROG_BTN_A);
    assert(enhanced_apps_view() && settings_apps_origin);
    press(FROG_BTN_B);
    assert(enhanced_settings_active() &&
           enhanced_settings_row_at(enhanced_settings_row_cursor()) == SR_OPEN_APPS);
    press(FROG_BTN_B);
    press(FROG_BTN_B);
    assert(!settings_menu_active);
    settings_apps_in_settings = 0;

    settings_graphical_home = 0;
    scan_apps_tab();
    assert(viewing_apps && !enhanced_apps_view());
    switch_main_tab(MAIN_TAB_GAMES);
    settings_graphical_home = 1;
    test_app_executables = false;
    theme_resource_destroy(enhanced_theme);
    enhanced_theme = NULL;
    system_icons_clear();
    free(framebuffer);
    framebuffer = NULL;
    assert(snprintf(path, sizeof path, "%s/Ebook", root) < (int)sizeof path);
    assert(rmdir(path) == 0);
    puts("PASS Apps shared CoverFlow, artwork, device restrictions, dispatch, submenus and return");
}

static void test_enhanced_collections(const char *root) {
    char first[MAX_PATH_LEN], second[MAX_PATH_LEN], capture[MAX_PATH_LEN], folder[MAX_PATH_LEN];
    snprintf(first, sizeof first, "%s/AFirst/First.nes", root);
    snprintf(second, sizeof second, "%s/NoArtwork/Example.nes", root);
    snprintf(test_p9_favorites, sizeof test_p9_favorites, "%s/.favorites", root);
    snprintf(test_p9_history, sizeof test_p9_history, "%s/.history", root);
    write_fixture(test_p9_favorites, "");
    write_fixture(test_p9_history, "");
    favorites_load();
    recent_games_load();
    settings_graphical_home = 1;
    strcpy(current_path, ROMS_PATH);
    scan_directory(current_path);
    enhanced_home_kind = ENHANCED_HOME_FAVOURITES;
    assert(enhanced_home_index(enhanced_home_platform_count()) == 0);
    press(FROG_BTN_A);
    assert(enhanced_favorites_view() && entry_count == 0);
    prepare_enhanced_library(false);
    press(FROG_BTN_B);
    assert(enhanced_home_view() && enhanced_home_kind == ENHANCED_HOME_FAVOURITES);

    assert(favorites_toggle_path("quicknes", "Same name", first));
    assert(favorites_toggle_path("quicknes", "Same name", second));
    favorites_load();
    assert(favorites_get_count() == 2);
    press(FROG_BTN_A);
    assert(enhanced_favorites_view() && entry_count == 2);
    int first_index = !strcmp(collection_game_path(0), first) ? 0 : 1;
    selected_index = first_index;
    prepare_enhanced_library(false);
    assert(!strcmp(coverflow_title_text, "Retitled"));
    press(FROG_BTN_A);
    assert(enhanced_details_view());
    press(FROG_BTN_Y);
    assert(enhanced_details_view() && favorites_get_count() == 1);
    assert(!strcmp(collection_game_path(selected_index), first));
    press(FROG_BTN_B);
    assert(enhanced_favorites_view() && entry_count == 1 &&
           !strcmp(collection_game_path(selected_index), second));
    ra_game_fixture(root, true);
    press(FROG_BTN_A);
    assert(enhanced_details_view());
    prepare_enhanced_library(false);
    prepare_details_achievements(root);
    press(FROG_BTN_L2);
    assert(enhanced_achievements_view());
    press(FROG_BTN_B);
    assert(enhanced_details_view() &&
           !strcmp(collection_game_path(selected_index), second));
    press(FROG_BTN_A);
    assert(enhanced_favorites_view());
    FILE *launch = fopen(LAUNCH_FILE, "r");
    char target[1024];
    assert(launch && fgets(target, sizeof target, launch) && fgets(target, sizeof target, launch));
    target[strcspn(target, "\r\n")] = 0;
    assert(!strcmp(target, second));
    fclose(launch);
    ra_game_fixture(root, false);
    press(FROG_BTN_A);
    press(FROG_BTN_Y);
    assert(enhanced_details_view() && favorites_get_count() == 0);
    press(FROG_BTN_B);
    assert(enhanced_favorites_view() && entry_count == 0);
    press(FROG_BTN_B);
    assert(enhanced_home_view());

    write_fixture(test_p9_history, "");
    recent_games_load();
    recent_games_add("quicknes", "Same name", first);
    recent_games_add("quicknes", "Same name", second);
    recent_games_add("quicknes", "Same name", first);
    recent_games_load();
    assert(recent_games_get_count() == 2 && !strcmp(recent_games_get_list()[0].full_path, first));
    assert(snprintf(test_screenshot_root, sizeof test_screenshot_root, "%s/.captures", root) <
           (int)sizeof test_screenshot_root);
    assert(mkdir(test_screenshot_root, 0700) == 0);
    assert(snprintf(folder, sizeof folder, "%s/AFirst", test_screenshot_root) < (int)sizeof folder);
    assert(mkdir(folder, 0700) == 0);
    assert(snprintf(capture, sizeof capture, "%s/First.scr.bmp", folder) < (int)sizeof capture);
    write_cover_bmp(capture, 640, 480);
    enhanced_home_kind = ENHANCED_HOME_RECENTS;
    press(FROG_BTN_A);
    assert(enhanced_recents_view());
    selected_index = 0;
    long long preparation_started = monotonic_ms();
    allocations = allocation_bytes = largest_allocation = 0;
    measure_allocations = true;
    prepare_enhanced_library(false);
    prepare_details_metrics();
    prepare_enhanced_recents();
    measure_allocations = false;
    printf("P9 cold preparation: %lld ms; %zu bytes requested by wrapped malloc; "
           "largest %zu bytes; selected screenshot %zu bytes\n",
           monotonic_ms() - preparation_started, allocation_bytes, largest_allocation,
           (size_t)enhanced_recents_screenshot.width * enhanced_recents_screenshot.height * 2);
    assert(enhanced_recents_screenshot.width == 320 && enhanced_recents_screenshot.height == 240);
    press(FROG_BTN_X);
    assert(enhanced_details_view());
    press(FROG_BTN_B);
    assert(enhanced_recents_view() && selected_index == 0);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_library(false);
    prepare_enhanced_recents();
    assert(selected_index == 1 && !enhanced_recents_screenshot.data);
    press(FROG_BTN_RIGHT);
    assert(selected_index == 1);
    assert(unlink(capture) == 0);
    write_fixture(capture, "bad file");
    Thumbnail screen = {0};
    assert(!load_bounded_screenshot_thumbnail(first, &screen));
    write_cover_bmp(capture, 641, 480);
    assert(!load_bounded_screenshot_thumbnail(first, &screen));
    press(FROG_BTN_B);
    prepare_enhanced_recents();
    assert(enhanced_home_view() && !enhanced_recents_screenshot.data);
    puts("PASS P9 cross-platform collections, metadata, favorite removal, history, screenshots and Details");
}

int main(int argc, char **argv) {
    assert(argc == 2);
    assert(setenv("FROGUI_ROMS_PATH", argv[1], 1) == 0);
    assert(snprintf(test_settings, sizeof test_settings, "%s/settings.txt", argv[1])
           < (int)sizeof test_settings);
    assert(strlen(argv[1]) < sizeof g_roms_path - 32);
    assert(snprintf(test_cache, sizeof test_cache, "%s/.cache", argv[1]) < (int)sizeof test_cache);
    assert(snprintf(test_launch, sizeof test_launch, "%s/.launch", argv[1]) < (int)sizeof test_launch);
    strcpy(g_roms_path, argv[1]);
    strcpy(current_path, ROMS_PATH);
    write_fixture(test_settings, "load_recents=on\nexplore_layout=horizontal\nenhanced_theme=legacy\n");
    settings_load_file();
    assert(settings_enhanced_theme_present && strcmp(settings_enhanced_theme, "legacy") == 0);
    assert(!settings_graphical_home);
    assert(settings_load_recents);
    recent_games_add("nes", "Recent", "/test/Recent.nes");
    assert(start_in_recents());
    assert(horizontal_system_view());
    assert(!icon_system_view());

    render_theme_load("vertical", "MinUIfied");
    switch_home_open = true;
    assert(switch_home_view());
    settings_graphical_home = 1;
    settings_save_file();
    settings_load_file();
    assert(settings_graphical_home && settings_load_recents);
    assert(!start_in_recents());
    assert(settings_explore_layout == STYLE_HORIZONTAL);
    render_theme_load("vertical", "MinUIfied");
    assert(!switch_home_view());
    assert(!horizontal_system_view());
    assert(icon_system_view());
    render_theme_load(NULL, NULL);

    char loose[MAX_PATH_LEN], first[MAX_PATH_LEN], collection[MAX_PATH_LEN], rom[MAX_PATH_LEN], second_rom[MAX_PATH_LEN];
    snprintf(first, sizeof first, "%s/AFirst", ROMS_PATH);
    snprintf(loose, sizeof loose, "%s/Loose.nes", ROMS_PATH);
    snprintf(collection, sizeof collection, "%s/NoArtwork", ROMS_PATH);
    snprintf(rom, sizeof rom, "%s/NoArtwork/Example.nes", ROMS_PATH);
    snprintf(second_rom, sizeof second_rom, "%s/NoArtwork/Second.nes", ROMS_PATH);
    write_fixture(loose, "");
    assert(mkdir(first, 0700) == 0);
    char first_rom[MAX_PATH_LEN];
    snprintf(first_rom, sizeof first_rom, "%s/AFirst/First.nes", ROMS_PATH);
    write_fixture(first_rom, "");
    assert(mkdir(collection, 0700) == 0);
    write_fixture(rom, "");
    write_fixture(second_rom, "");
    char launch_csv[MAX_PATH_LEN];
    assert(snprintf(launch_csv, sizeof launch_csv, "%s/filelist.csv", collection) < (int)sizeof launch_csv);
    write_fixture(launch_csv,
                  "\xef\xbb\xbfRom.ext,Display Title,ChineseTitle,Players,Misc,Description,"
                  "Developer,ReleaseYear,Publisher,Original Title,Genre,Developer\r\n"
                  "Example.nes,Custom Launch Title,,2,skip,"
                  "\"Explore a Pokémon cave, and find a hidden map.\","
                  "Studio,1994,Publisher,Example Original,Platformer,Duplicate\r\n"
                  "Second.nes,,,1,,\xff,Valid Studio,,,,\r\n"
                  "Second.nes,,,9,,Ignored duplicate,,,,,\r\n");
    assert(snprintf(test_sessions, sizeof test_sessions, "%s/.sessions", argv[1]) <
           (int)sizeof test_sessions);
    assert(snprintf(test_playtime, sizeof test_playtime, "%s/.playtime", argv[1]) <
           (int)sizeof test_playtime);
    char ledger[2048];
    assert(snprintf(ledger, sizeof ledger, "1730246400\t120\t%s\n0\t45\t%s\n"
                    "1730160000\t80\t%s\nnot-a-date\t999\t%s\n50\t999\t/elsewhere/Example.nes\n",
                    rom, rom, rom, rom) < (int)sizeof ledger);
    write_fixture(test_sessions, ledger);
    assert(snprintf(ledger, sizeof ledger, "630\t%s\n380\t%s\n", second_rom, rom) < (int)sizeof ledger);
    write_fixture(test_playtime, ledger);
    settings_file_cache = 0;
    scan_directory(ROMS_PATH);
    assert(entry_count == 2 && entries[0].is_dir && entries[1].is_dir);
    assert(strcmp(entries[selected_index].name, "AFirst") == 0);
    input_reset_defaults();
    enhanced_home_kind = ENHANCED_HOME_PLATFORM;
    press(FROG_BTN_RIGHT);
    assert(strcmp(entries[selected_index].name, "NoArtwork") == 0);
    assert(system_icon_get(entries[selected_index].name) == NULL);
    settings_backgrounds = 1;
    load_banner_for_view(collection, false, false);
    assert(!banner_is_loaded());

    press(FROG_BTN_L2);
    assert(settings_explore_layout == STYLE_HORIZONTAL);
    press(FROG_BTN_A);
    assert(strcmp(current_path, collection) == 0);
    assert(entry_count == 2 && strcmp(entries[0].name, "Example.nes") == 0);
    assert(!icon_system_view());
    check_csv_title("Example.nes", "Custom Launch Title");
    press(FROG_BTN_START);
    assert(settings_menu_active);
    assert(enhanced_settings_navigation.state.screen == NAV_SETTINGS);
    assert(strcmp(enhanced_settings_navigation.state.game_id, "Example.nes") == 0);
    /* Exercise the restore adapter with the fixture's discovered ROM root. */
    settings_menu_active = false;
    restore_settings_listing();
    assert(strcmp(current_path, collection) == 0);
    assert(strcmp(entries[selected_index].name, "Example.nes") == 0);
    int previous_motion = settings_anim;
    settings_anim = 1;
    press(FROG_BTN_A);
    assert(enhanced_details_view() && access(test_launch, F_OK) != 0);
    check_csv_title("Example.nes", "Custom Launch Title");
    assert(strcmp(details_title_text, "Custom Launch Title") == 0);
    uint32_t details_offset = filelist_detail_offsets[selected_index];
    assert(details_offset);
    const char *original_title = filelist_details + details_offset - 1;
    assert(strcmp(original_title, "Example Original") == 0);
    assert(strcmp(original_title + strlen(original_title) + 1, "1994") == 0);
    const char *field = original_title;
    for (int i = 0; i < FILELIST_DESCRIPTION; i++) field += strlen(field) + 1;
    assert(strcmp(field, "Explore a Pokémon cave, and find a hidden map.") == 0);
    assert(details_recent_rank(selected_index) == -1);
    prepare_details_metrics();
    assert(details_play_stats.runs == 3 && details_play_stats.seconds == 380);
    assert(details_play_stats.last == 1730246400 && details_play_stats.last_seconds == 120);
    assert(details_cover_transition_frame == 0);
    uint16_t *details_frame = calloc((size_t)SCREEN_WIDTH * SCREEN_HEIGHT, sizeof *details_frame);
    assert(details_frame);
    uint16_t red = 0xf800;
    Thumbnail cover = {.width = 1, .height = 1, .data = &red};
    coverflow_images[selected_index - coverflow_first] = &cover;
    render_enhanced_game_details(details_frame);
    int cover_y = SCREEN_HEIGHT * 44 / 100;
    assert(details_frame[cover_y * SCREEN_WIDTH + SCREEN_WIDTH / 2] == red);
    for (int i = 1; i < SYSTEM_CAROUSEL_FRAMES; i++) {
        memset(details_frame, 0, (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof *details_frame);
        render_enhanced_game_details(details_frame);
        if (i + 1 == SYSTEM_CAROUSEL_FRAMES / 2) {
            int midpoint_y = (cover_y + HEADER_HEIGHT + UI_S(20) + SCREEN_HEIGHT * 52 / 200) / 2;
            int red_pixels = 0;
            for (int x = 0; x < SCREEN_WIDTH; x++)
                red_pixels += details_frame[midpoint_y * SCREEN_WIDTH + x] == red;
            assert(red_pixels == 1);
        }
    }
    int left_x = SCREEN_WIDTH * 18 / 100;
    int left_y = HEADER_HEIGHT + UI_S(20) + SCREEN_HEIGHT * 52 / 200;
    assert(details_cover_transition_frame == SYSTEM_CAROUSEL_FRAMES);
    assert(details_frame[left_y * SCREEN_WIDTH + left_x] == red);
    assert(details_frame[left_y * SCREEN_WIDTH + SCREEN_WIDTH * 3 / 4] != red);
    coverflow_images[selected_index - coverflow_first] = NULL;
    int arrow_row = SCREEN_HEIGHT / 2 * SCREEN_WIDTH;
    assert(details_frame[arrow_row + UI_S(8)] == 0);
    assert(details_frame[arrow_row + SCREEN_WIDTH - UI_S(14) + UI_S(6)] == COLOR_TEXT);
    press(FROG_BTN_LEFT);
    assert(selected_index == 0);
    press(FROG_BTN_RIGHT);
    assert(enhanced_details_view() && details_cover_transition_frame == SYSTEM_CAROUSEL_FRAMES);
    assert(selected_index == 1 && strcmp(enhanced_settings_navigation.state.game_id, "Second.nes") == 0);
    prepare_enhanced_library(false);
    assert(strcmp(details_title_text, "Second") == 0);
    prepare_details_metrics();
    assert(details_play_stats.seconds == 630 && !details_play_stats.runs &&
           !details_play_stats.last && !details_play_stats.last_seconds);
    assert(filelist_detail_offsets[1]);
    const char *second_detail = filelist_details + filelist_detail_offsets[1] - 1;
    for (int i = 0; i < FILELIST_DEVELOPER; i++) second_detail += strlen(second_detail) + 1;
    assert(!strcmp(second_detail, "Valid Studio"));
    for (int i = FILELIST_DEVELOPER; i < FILELIST_DESCRIPTION; i++)
        second_detail += strlen(second_detail) + 1;
    assert(!*second_detail);
    memset(details_frame, 0, (size_t)SCREEN_WIDTH * SCREEN_HEIGHT * sizeof *details_frame);
    render_enhanced_game_details(details_frame);
    assert(details_frame[arrow_row + UI_S(14) - UI_S(6)] == COLOR_TEXT);
    assert(details_frame[arrow_row + SCREEN_WIDTH - UI_S(14) + UI_S(6)] == 0);
    press(FROG_BTN_RIGHT);
    assert(selected_index == 1);
    press(FROG_BTN_B);
    assert(enhanced_library_view() && !enhanced_details_view() && selected_index == 1);
    press(FROG_BTN_A);
    press(FROG_BTN_LEFT);
    assert(enhanced_details_view() && selected_index == 0);
    assert(strcmp(enhanced_settings_navigation.state.game_id, "Example.nes") == 0);
    prepare_details_achievements(ROMS_PATH);
    assert(!details_achievements_valid);
    press(FROG_BTN_L2);
    assert(enhanced_details_view());
    ra_game_fixture(ROMS_PATH, true);
    details_achievement_game = -1;
    prepare_details_achievements(ROMS_PATH);
    assert(details_achievements_valid && details_achievements.achievement_count == 2);
    press(FROG_BTN_L2);
    assert(enhanced_achievements_view());
    render_enhanced_achievements(details_frame);
    press(FROG_BTN_DOWN);
    assert(details_achievement_index == 1);
    press(FROG_BTN_START);
    assert(settings_menu_active &&
           enhanced_settings_navigation.state.settings_return == NAV_ACHIEVEMENTS_GALLERY);
    press(FROG_BTN_B);
    prepare_details_achievements(ROMS_PATH);
    assert(enhanced_achievements_view() && details_achievement_index == 1);
    press(FROG_BTN_B);
    assert(enhanced_details_view() && selected_index == 0);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_library(false);
    prepare_details_achievements(ROMS_PATH);
    assert(selected_index == 1 && !details_achievements_valid);
    assert(strcmp(details_title_text, "Second") == 0);
    press(FROG_BTN_L2);
    assert(enhanced_details_view());
    press(FROG_BTN_LEFT);
    prepare_enhanced_library(false);
    prepare_details_achievements(ROMS_PATH);
    assert(selected_index == 0 && details_achievements_valid);
    ra_game_fixture(ROMS_PATH, false);
    free(details_frame);
    press(FROG_BTN_START);
    assert(settings_menu_active && enhanced_settings_navigation.state.settings_return == NAV_GAME_DETAILS);
    press(FROG_BTN_B);
    assert(enhanced_details_view() && strcmp(entries[selected_index].name, "Example.nes") == 0);
    press(FROG_BTN_B);
    assert(enhanced_library_view() && !enhanced_details_view() && selected_index == 0);
    assert(access(test_launch, F_OK) != 0);
    settings_anim = 0;
    press(FROG_BTN_A);
    assert(enhanced_details_view());
    assert(details_cover_transition_frame == SYSTEM_CAROUSEL_FRAMES);
    settings_anim = previous_motion;
    press(FROG_BTN_A);
    assert(!enhanced_details_view());
    FILE *handoff = fopen(test_launch, "r");
    assert(handoff);
    char core_line[MAX_PATH_LEN], rom_line[MAX_PATH_LEN];
    assert(fgets(core_line, sizeof core_line, handoff));
    assert(fgets(rom_line, sizeof rom_line, handoff));
    assert(fclose(handoff) == 0);
    assert(strcmp(core_line, CORES_PATH "/quicknes_libretro.so\n") == 0);
    rom_line[strcspn(rom_line, "\n")] = 0;
    assert(strcmp(rom_line, rom) == 0);
    assert(details_recent_rank(selected_index) == 0);
    assert(details_recent_rank(1) == -1);
    press(FROG_BTN_B);
    assert(strcmp(current_path, ROMS_PATH) == 0 && icon_system_view());
    assert(strcmp(entries[selected_index].name, "NoArtwork") == 0);
    press(FROG_BTN_START);
    assert(settings_menu_active);
    press(FROG_BTN_B);
    assert(!settings_menu_active);
    assert(enhanced_home_kind == ENHANCED_HOME_PLATFORM);
    assert(strcmp(entries[selected_index].name, "NoArtwork") == 0);
    strcpy(g_roms_path, argv[1]);
    strcpy(current_path, ROMS_PATH);

    settings_graphical_home = 0;
    settings_save_file();
    settings_graphical_home = 1;
    settings_load_file();
    assert(!settings_graphical_home && settings_load_recents);
    assert(start_in_recents());
    assert(horizontal_system_view());
    scan_directory(ROMS_PATH);
    assert(entry_count == 3);
    test_bounded_artwork(argv[1]);
    test_settings_mapping_contract();
    test_classic_and_enhanced_navigation_contract();
    test_enhanced_art(argv[1]);
    test_ui_sounds(argv[1]);
    test_ribbon(argv[1]);
    test_coverflow(argv[1]);
    test_apps_coverflow(argv[1]);
    test_home_ribbon_presentation();
    test_platform_filelists(argv[1]);
    test_ra_home(argv[1]);
    test_enhanced_settings_integration();
    test_enhanced_collections(argv[1]);
    return 0;
}
