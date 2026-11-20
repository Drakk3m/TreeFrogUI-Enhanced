/* Host integration test. Include the core to exercise its private UI state. */
#include <assert.h>
#include "../frogui_libretro.c"

static char test_settings[512];

FILE *__real_fopen(const char *path, const char *mode);
FILE *__wrap_fopen(const char *path, const char *mode) {
    if (strcmp(path, SETTINGS_FILE) == 0)
        return __real_fopen(test_settings, mode);
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
    return 0;
}
