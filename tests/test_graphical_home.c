/* Host integration test. Include the core to exercise its private UI state. */
#include <assert.h>
#include "../frogui_libretro.c"

static char test_settings[512];
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
    assert(snprintf(theme_root, sizeof theme_root, "%s/.theme", root) < (int)sizeof theme_root);
    assert(mkdir(theme_root, 0700) == 0);
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
                       "[resource.platform.noartwork.icon]\npath=bad.png\n");

    enhanced_theme = theme_resource_create(theme_root, THEME_RESOURCE_CACHE_MAX);
    assert(enhanced_theme && theme_resource_load_manifest(enhanced_theme, "theme.ini"));
    ThemeResourceDescriptor bad;
    assert(theme_resource_resolve(enhanced_theme, "platform.noartwork.icon", &bad));
    int width, height;
    assert(!enhanced_decode_image(&bad, &width, &height, 4)); /* valid header, bad pixel data */

    assert(!settings_graphical_home);
    assert(!system_icon_get("AFirst")); /* legacy still ignores the Enhanced mapping */
    system_icons_clear(); /* simulate switching the manifest at the live boundary */
    settings_graphical_home = 1;
    settings_backgrounds = 1;
    settings_wallpaper_idx = 0;
    scan_directory(ROMS_PATH);
    prepare_enhanced_grid();
    assert(enhanced_grid_prepared && enhanced_grid_count == 2);
    assert(enhanced_grid_icons[0] && enhanced_grid_icons[0]->alpha[0] == 160);
    assert(strcmp(enhanced_grid_icons[0]->path, "icon.png") == 0);
    assert(enhanced_grid_icons[1] && strcmp(enhanced_grid_icons[1]->path, legacy_icon) == 0);

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
    allocations = 0;
    measure_allocations = true;
    for (int frame = 0; frame < 480 && banner_is_animating(); frame++) {
        prepare_enhanced_grid();
        banner_render(framebuffer);
    }
    load_banner_for_view("NoArtwork", false, false); /* same resolved backdrop */
    measure_allocations = false;
    assert(allocations == 0 && banner_is_loaded() && !banner_is_animating());
    assert(framebuffer[0] == 0xf800);
    puts("PASS enhanced resource decode, native fallback, and zero mallocs on settled rendering");

    banner_clear_enhanced();
    assert(!banner_is_loaded());
    system_icons_clear();
    theme_resource_destroy(enhanced_theme);
    enhanced_theme = NULL;
    free(framebuffer);
    framebuffer = NULL;
}

int main(int argc, char **argv) {
    assert(argc == 2);
    assert(snprintf(test_settings, sizeof test_settings, "%s/settings.txt", argv[1])
           < (int)sizeof test_settings);
    assert(strlen(argv[1]) < sizeof g_roms_path - 32);
    strcpy(g_roms_path, argv[1]);
    strcpy(current_path, ROMS_PATH);
    write_fixture(test_settings, "load_recents=on\nexplore_layout=horizontal\n");
    settings_load_file();
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
    press(FROG_BTN_B);
    assert(strcmp(current_path, ROMS_PATH) == 0 && icon_system_view());
    assert(strcmp(entries[selected_index].name, "NoArtwork") == 0);
    press(FROG_BTN_START);
    assert(settings_menu_active);
    press(FROG_BTN_B);
    assert(!settings_menu_active);
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
    return 0;
}
