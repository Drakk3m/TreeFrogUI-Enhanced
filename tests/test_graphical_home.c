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
    int offset = expected - 2;
    if (offset < 0) offset = 0;
    if (offset > platforms - 2) offset = platforms - 2;
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
        enhanced_home_kind = ENHANCED_HOME_PLATFORM;
        enhanced_grid_prepared = false;
        retro_run();
        int platforms = enhanced_home_platform_count();
        for (int index = 1; index < platforms + 3; index++)
            ribbon_step(FROG_BTN_RIGHT, index, platforms);
        ribbon_step(FROG_BTN_RIGHT, platforms + 2, platforms);
        for (int index = platforms + 1; index >= 0; index--)
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
    for (int i = 0; i < 1500; i++) fprintf(file, "Item%04d.nes,Title %04d,\n", i, i);
    assert(fclose(file) == 0);
    start = monotonic_ms();
    scan_directory(current_path);
    long long with = monotonic_ms() - start;
    check_csv_title("Item1499.nes", "Title 1499");
    printf("Filelist fixture: 1500 mapped ROMs, %zu bytes retained, scans %lld ms without / %lld ms with CSV\n",
           filelist_capacity + (size_t)entry_count * sizeof(*filelist_offsets), without, with);
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
    for (int i = 0; i < 4; i++) render_ra_home(frame);
    size_t painted = 0;
    bool left_pill = false, right_pill = false, central_gap_clear = true;
    for (int y = 0; y < 480; y++)
        for (int x = 0; x < 640; x++)
            if (frame[y * 640 + x]) {
                assert(y >= 39 && y < 82);
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
    write_fixture(launch_csv, "Example.nes,Custom Launch Title,\n");
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
    assert(details_frame[left_y * SCREEN_WIDTH + SCREEN_WIDTH * 3 / 4] == 0);
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
    test_enhanced_art(argv[1]);
    test_ui_sounds(argv[1]);
    test_ribbon(argv[1]);
    test_coverflow(argv[1]);
    test_home_ribbon_presentation();
    test_platform_filelists(argv[1]);
    test_ra_home(argv[1]);
    return 0;
}
