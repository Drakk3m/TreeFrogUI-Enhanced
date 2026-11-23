/* Host integration test. Include the core to exercise its private UI state. */
#include <assert.h>
static char test_theme_pack_root[512];
#define FROGUI_THEME_PACK_ROOT test_theme_pack_root
#include "../frogui_libretro.c"

static char test_settings[MAX_PATH_LEN];
static char test_cache[MAX_PATH_LEN];
static char test_launch[MAX_PATH_LEN];

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

int __real_access(const char *path, int mode);
int __wrap_access(const char *path, int mode) {
    if (test_legacy_icon[0] && strcmp(path, legacy_icon) == 0)
        return __real_access(test_legacy_icon, mode);
    if (strncmp(path, "/mnt/sdcard/", 12) == 0)
        return -1;
    return __real_access(path, mode);
}

static void press(FrogButton button) {
    input_set_ext_raw(1u << input_get_raw_bit(button));
    handle_input();
    /* Face buttons require a stable hold in the real input layer. */
    if (button == FROG_BTN_A || button == FROG_BTN_B) {
        usleep(80000);
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
                       "[resource.platform.noartwork.background]\npath=bad.png\n");

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
    assert(theme_resource_resolve(enhanced_theme, "platform.noartwork.icon", &bad));
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
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer); /* Offline export absent. */
    assert(selected_index == 0 && scroll_offset == 0);
    press(FROG_BTN_LEFT);
    assert(selected_index == 0);

    press(FROG_BTN_RIGHT);
    prepare_enhanced_grid();
    assert(selected_index == 1 && scroll_offset == 0);
    assert(enhanced_grid_first == 0 && enhanced_grid_count == ENHANCED_HOME_VISIBLE);
    assert(enhanced_home_center_offset(0, 9) == 0);
    assert(enhanced_home_center_offset(4, 9) == 2);
    assert(enhanced_home_center_offset(8, 9) == 4);
    assert(enhanced_home_center_offset(3, 4) == 0);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_grid();
    assert(selected_index == 2 && scroll_offset == 0);
    press(FROG_BTN_RIGHT);
    prepare_enhanced_grid();
    assert(selected_index == 3 && scroll_offset == 1);
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
    assert(enhanced_home_index(platforms) == platforms + 2);
    assert(scroll_offset == platforms + 3 - ENHANCED_HOME_VISIBLE);
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
    for (int i = 0; i < SETTINGS_ROW_N; i++) {
        if (!strcmp(settings_rows[i].label, "settings.system"))
            settings_section_open[settings_section_of(i)] = true;
        if (settings_rows[i].type == RT_CACHE_REBUILD) settings_menu_idx = i;
    }
    press(FROG_BTN_A);
    assert(!settings_menu_active && entry_count == stored_count + 1);
    prepare_enhanced_grid();
    assert(enhanced_home_kind == ENHANCED_HOME_RECENTS);
    assert(enhanced_home_index(enhanced_home_platform_count()) == platforms + 1);
    assert(scroll_offset == enhanced_home_platform_count() + 3 - ENHANCED_HOME_VISIBLE);
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
    assert(enhanced_home_index(enhanced_home_platform_count()) == 0);
    assert(strcmp(enhanced_grid_names[0], entries[1].name) == 0);

    /* Zero and one platform both retain exactly three utilities. */
    entries[0] = entries[1];
    entry_count = 1;
    selected_index = scroll_offset = 0;
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer);
    assert(enhanced_grid_count == 4 && enhanced_home_kind == ENHANCED_HOME_PLATFORM);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_RECENTS);
    entry_count = 0;
    enhanced_home_kind = ENHANCED_HOME_PLATFORM;
    prepare_enhanced_grid();
    render_enhanced_home(framebuffer);
    assert(enhanced_grid_count == 3 && enhanced_home_kind == ENHANCED_HOME_RECENTS);
    press(FROG_BTN_LEFT);
    assert(enhanced_home_kind == ENHANCED_HOME_RECENTS);
    press(FROG_BTN_RIGHT);
    press(FROG_BTN_RIGHT);
    press(FROG_BTN_RIGHT);
    assert(enhanced_home_kind == ENHANCED_HOME_SETTINGS);
    free(framebuffer);
    framebuffer = NULL;
    puts("PASS unified ribbon, Settings context, utility routes, cache rebuild, and reduced motion");
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

    char loose[MAX_PATH_LEN], first[MAX_PATH_LEN], collection[MAX_PATH_LEN], rom[MAX_PATH_LEN];
    snprintf(first, sizeof first, "%s/AFirst", ROMS_PATH);
    snprintf(loose, sizeof loose, "%s/Loose.nes", ROMS_PATH);
    snprintf(collection, sizeof collection, "%s/NoArtwork", ROMS_PATH);
    snprintf(rom, sizeof rom, "%s/NoArtwork/Example.nes", ROMS_PATH);
    write_fixture(loose, "");
    assert(mkdir(first, 0700) == 0);
    char first_rom[MAX_PATH_LEN];
    snprintf(first_rom, sizeof first_rom, "%s/AFirst/First.nes", ROMS_PATH);
    write_fixture(first_rom, "");
    assert(mkdir(collection, 0700) == 0);
    write_fixture(rom, "");
    settings_file_cache = 0;
    scan_directory(ROMS_PATH);
    assert(entry_count == 2 && entries[0].is_dir && entries[1].is_dir);
    assert(strcmp(entries[selected_index].name, "AFirst") == 0);
    input_reset_defaults();
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
    assert(entry_count == 1 && strcmp(entries[0].name, "Example.nes") == 0);
    assert(!icon_system_view());
    press(FROG_BTN_START);
    assert(settings_menu_active);
    assert(enhanced_settings_navigation.state.screen == NAV_SETTINGS);
    assert(strcmp(enhanced_settings_navigation.state.game_id, "Example.nes") == 0);
    /* Exercise the restore adapter with the fixture's discovered ROM root. */
    settings_menu_active = false;
    restore_settings_listing();
    assert(strcmp(current_path, collection) == 0);
    assert(strcmp(entries[selected_index].name, "Example.nes") == 0);
    press(FROG_BTN_A);
    FILE *handoff = fopen(test_launch, "r");
    assert(handoff);
    char core_line[MAX_PATH_LEN], rom_line[MAX_PATH_LEN];
    assert(fgets(core_line, sizeof core_line, handoff));
    assert(fgets(rom_line, sizeof rom_line, handoff));
    assert(fclose(handoff) == 0);
    assert(strcmp(core_line, CORES_PATH "/quicknes_libretro.so\n") == 0);
    rom_line[strcspn(rom_line, "\n")] = 0;
    assert(strcmp(rom_line, rom) == 0);
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
    test_enhanced_art(argv[1]);
    test_ui_sounds(argv[1]);
    test_ribbon(argv[1]);
    return 0;
}
