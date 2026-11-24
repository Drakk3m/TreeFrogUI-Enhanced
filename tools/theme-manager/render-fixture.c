#include "frogui_libretro.c"

static bool measure_draw;
static size_t allocations, largest;
void *__real_malloc(size_t size);
void *__wrap_malloc(size_t size) {
    if (measure_draw) { allocations++; if (size > largest) largest = size; }
    return __real_malloc(size);
}

static void demo_capture(uint16_t *pixels) {
    for (int y = 0; y < 240; y++) {
        for (int x = 0; x < 320; x++) {
            int sky = 115 + y / 4;
            uint16_t color = RGB565(33, sky, 202);
            int hill = 112 + (x / 17 % 6) * 7;
            if (y > hill + (x % 17) / 2) color = RGB565(79,129,90);
            if (y > 171) color = ((x / 12 + y / 10) % 2) ? RGB565(124,84,69) : RGB565(141,99,75);
            if (y >= 159 && y <= 172) color = RGB565(144,184,72);
            if (x >= 79 && x <= 95 && y >= 144 && y <= 158) color = RGB565(245,187,87);
            if (x >= 82 && x <= 94 && y >= 132 && y <= 144) color = RGB565(237,88,105);
            if (x >= 146 && x <= 178 && y >= 123 && y <= 134) color = RGB565(178,135,96);
            pixels[y * 320 + x] = color;
        }
    }
}

int main(int argc, char **argv) {
    if (argc != 3) return 2;
    static uint16_t frame[640 * 480], capture[320 * 240];
    static DirEntry platforms[] = {
        {.name="NES", .is_dir=1}, {.name="SNES", .is_dir=1}, {.name="GBA", .is_dir=1},
        {.name="MD", .is_dir=1}
    };
    static DirEntry games[] = {
        {.name="Aurora Circuit.gba"}, {.name="Tiny Atlas.gba"}, {.name="Lumen Harbor.gba"}
    };
    framebuffer = frame;
    settings_graphical_home = true;
    settings_anim = false;
    render_set_geometry(640, 480, 100);
    theme_apply(0);
    render_theme_load("vertical", NULL);
    i18n_init("en_US");
    font_init();
    font_set_size(23);
    long long load_start = monotonic_ms();
    enhanced_theme = theme_resource_create(argv[2], THEME_RESOURCE_CACHE_MAX);
    if (!enhanced_theme || !theme_resource_load_manifest(enhanced_theme, "theme.ini"))
        return 3;
    const char *mode = argv[1];
    bool home = !strcmp(mode, "home");
    bool apps = !strcmp(mode, "apps");
    bool settings = !strcmp(mode, "settings");
    bool gallery = !strcmp(mode, "gallery");
    bool dialog = !strcmp(mode, "dialog");
    bool details = !strcmp(mode, "details");
    bool favorites = !strncmp(mode, "favorites", 9);
    bool empty = strstr(mode, "empty") != NULL;
    static DirEntry applications[] = {
        {.name="activity", .is_dir=1}, {.name="usbmode", .is_dir=1},
        {.name="ebook", .is_dir=1}
    };
    if (home) {
        entries = platforms;
        entry_count = sizeof platforms / sizeof platforms[0];
        strcpy(current_path, ROMS_PATH);
        enhanced_home_kind = ENHANCED_HOME_FAVOURITES;
        prepare_enhanced_grid();
        load_ra_trophy();
    } else if (apps) {
        viewing_apps = true;
        entries = applications;
        entry_count = sizeof applications / sizeof applications[0];
        selected_index = 1;
        prepare_enhanced_library(true);
    } else if (settings) {
        settings_menu_active = enhanced_settings_visit = true;
        settings_build_vis_rows();
        prepare_enhanced_settings_icons(true);
    } else {
        entries = games;
        entry_count = empty ? 0 : 3;
        selected_index = 1;
        viewing_favourites = favorites;
        viewing_recents = !strcmp(mode, "recents");
        coverflow_first = 0;
        coverflow_last = entry_count;
        coverflow_target = 1;
        coverflow_frame = SYSTEM_CAROUSEL_FRAMES;
        coverflow_from = coverflow_visible = 1;
        prepare_cover_placeholder();
        static uint16_t covers[3][90 * 124];
        static Thumbnail thumbs[3];
        for (int card = 0; card < entry_count; card++) {
            for (int y = 0; y < 124; y++) for (int x = 0; x < 90; x++) {
                int ray = (x + y * (card + 1)) % 38;
                covers[card][y * 90 + x] = RGB565(35 + 13 * ray / 4,
                                                   70 + y / 2, 132 + x);
            }
            thumbs[card] = (Thumbnail){.data=covers[card], .width=90, .height=124};
            coverflow_images[card] = card == 0 ? NULL : &thumbs[card];
        }
        strcpy(coverflow_title_text, "Tiny Atlas");
        strcpy(details_title_text, "Tiny Atlas");
        coverflow_title_width = font_measure_text(coverflow_title_text);
        strcpy(collection_fields[FILELIST_RELEASE_DATE], "2003");
        strcpy(enhanced_recents_platform, "GBA");
        strcpy(enhanced_settings_navigation.state.platform_id, "GBA");
        selected_favorite = p9_rom_available = true;
        details_play_stats = (DetailsPlayStats){ .last = 1785000000, .seconds = 7200,
                                               .last_seconds = 750, .runs = 7 };
        if (viewing_recents) {
            demo_capture(capture);
            enhanced_recents_screenshot = (Thumbnail){.data=capture, .width=320, .height=240};
        }
        if (gallery) {
            details_achievements_valid = true;
            details_achievements.game_id = 1;
            details_achievements.achievement_count = 1;
            details_achievements.achievements[0].points = 5;
            strcpy(details_achievements.achievements[0].title, "Surveyor");
            strcpy(details_achievements.achievements[0].description, "Find the northern observatory.");
            load_ra_trophy();
            prepare_achievement_placeholder();
        }
    }
    ThemeResourceDescriptor backdrop;
    if (theme_resource_resolve(enhanced_theme, "home.background", &backdrop) &&
        use_enhanced_background(&backdrop)) {
        for (int i = 0; i < 64; i++) banner_render(frame);
    }
    size_t icon_bytes = 0;
    for (int i = 0; i < SYSTEM_ICON_CACHE_N; i++)
        if (system_icons[i].pixels)
            icon_bytes += (size_t)system_icons[i].width * system_icons[i].height * 3;
    fprintf(stderr, "%s: preparation %lld ms; icon cache %zu bytes\n",
            mode, monotonic_ms() - load_start, icon_bytes);
    int repeats = 80;
    long long start = monotonic_ms();
    measure_draw = true;
    for (int i = 0; i < repeats; i++) {
        if (banner_is_loaded()) banner_render(frame);
        else render_clear_screen(frame);
        if (home) render_enhanced_home(frame);
        else if (details) render_enhanced_game_details(frame);
        else if (apps) render_enhanced_apps(frame);
        else if (settings) render_enhanced_settings_menu();
        else if (gallery) render_enhanced_achievements(frame);
        else if (dialog) render_usb_confirm();
        else if (!viewing_recents) render_enhanced_library(frame);
        else render_enhanced_recents(frame);
    }
    measure_draw = false;
    fprintf(stderr,"%s: %lld ms/%d frames; %zu native allocations; max request %zu bytes\n",
            argv[1], monotonic_ms()-start, repeats, allocations, largest);
    printf("P6\n640 480\n255\n");
    for (size_t i = 0; i < sizeof frame / sizeof *frame; i++) {
        putchar(((frame[i] >> 11) & 31) * 255 / 31);
        putchar(((frame[i] >> 5) & 63) * 255 / 63);
        putchar((frame[i] & 31) * 255 / 31);
    }
    return 0;
}
