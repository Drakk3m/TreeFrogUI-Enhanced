#ifndef ENHANCED_RECENTS_H
#define ENHANCED_RECENTS_H

static Thumbnail enhanced_recents_screenshot;
static char enhanced_recents_game[MAX_PATH_LEN];
static char enhanced_recents_platform[64];
static unsigned enhanced_recents_generation;

static void prepare_enhanced_recents(void) {
    const char *game = collection_game_path(selected_index);
    if (!enhanced_recents_view() || enhanced_details_view() || enhanced_achievements_view() ||
        !game || !p9_rom_available) {
        free(enhanced_recents_screenshot.data);
        enhanced_recents_screenshot = (Thumbnail){0};
        recents_platform_icon = NULL;
        recents_platform_icon_probed = false;
        enhanced_recents_game[0] = 0;
        enhanced_recents_platform[0] = 0;
        return;
    }
    bool new_game = strcmp(enhanced_recents_game, game) != 0 ||
                    enhanced_recents_generation != filelist_scan_generation;
    if (new_game) {
        free(enhanced_recents_screenshot.data);
        enhanced_recents_screenshot = (Thumbnail){0};
        recents_platform_icon = NULL;
        recents_platform_icon_probed = false;
        enhanced_recents_generation = filelist_scan_generation;
        snprintf(enhanced_recents_game, sizeof enhanced_recents_game, "%s", game);
        load_bounded_screenshot_thumbnail(game, &enhanced_recents_screenshot);
    }
    if (recents_platform_icon_probed) return;
    recents_platform_icon_probed = true;
    if (collection_platform(enhanced_recents_platform, sizeof enhanced_recents_platform, game)) {
        ThemeResourceDescriptor resource;
        if (enhanced_platform_resource(enhanced_recents_platform, "icon", &resource)) {
            recents_platform_icon = system_icon_load(resource.path, &resource);
        }
    }
}

static void render_recent_box(uint16_t *fb, const Thumbnail *cover) {
    const ThemePresentation *theme = theme_resource_presentation(enhanced_theme);
    int width = SCREEN_WIDTH * 31 / 100, height = SCREEN_HEIGHT * 53 / 100;
    if (cover && cover->width > 0 && cover->height > 0) {
        if (width * cover->height > height * cover->width)
            width = height * cover->width / cover->height;
        else
            height = width * cover->height / cover->width;
    } else {
        width = width * 3 / 4;
    }
    if (width < 1 || height < 1) return;
    int x = SCREEN_WIDTH * 21 / 100 - width / 2;
    int y = SCREEN_HEIGHT * 45 / 100 - height / 2;
    int spine = UI_S(15), skew = UI_S(7);
    render_rounded_rect_alpha(fb, x - spine + UI_S(5), y + UI_S(8),
                              width + spine + skew, height + UI_S(2), UI_S(8), 0,
                              theme ? theme->recents_shadow_opacity : 166);
    for (int dy = 0; dy < height; dy++) {
        int shift = skew * dy / height;
        render_fill_rect(fb, x + shift - spine, y + dy, spine, 1, COLOR_HEADER);
        render_fill_rect(fb, x + shift - UI_S(3), y + dy, width + UI_S(5), 1, COLOR_SELECT_BG);
    }
    if (cover)
        render_thumbnail_skewed(fb, cover, x, y, width, height, skew);
    else
        enhanced_home_utility_icon(fb, ENHANCED_HOME_APPS, x + width / 2, y + height / 2,
                                   width * 2 / 3, COLOR_SELECT_TEXT);
}

static void render_enhanced_recents(uint16_t *fb) {
    if (enhanced_recents_screenshot.data) {
        const ThemePresentation *theme = theme_resource_presentation(enhanced_theme);
        render_clear_screen(fb);
        render_thumbnail_contained(fb, &enhanced_recents_screenshot, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        render_rounded_rect_alpha(fb, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0,
                                  theme ? theme->recents_screenshot_dim : 96);
    }
    render_header(fb, tr("tab.recents"));
    const char *hint = tr_or("enhanced.recents.back", "B Back");
    if (entry_count <= 0) {
        const char *empty = tr_or("enhanced.recents.empty", "No recent games");
        int x = (SCREEN_WIDTH - font_measure_text(empty)) / 2;
        font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, x, SCREEN_HEIGHT / 2, empty, COLOR_TEXT);
    } else {
        if (selected_index > 0) render_details_chevron(fb, UI_S(14), -1);
        if (selected_index + 1 < entry_count)
            render_details_chevron(fb, SCREEN_WIDTH - UI_S(14), 1);
        const Thumbnail *cover = selected_index >= coverflow_first && selected_index < coverflow_last
                               ? coverflow_images[selected_index - coverflow_first] : NULL;
        const SystemIcon *icon = recents_platform_icon ? recents_platform_icon : cover_placeholder_icon;
        Thumbnail fallback;
        if (!cover && icon) {
            fallback = (Thumbnail){.data=icon->pixels, .alpha=icon->alpha,
                                   .width=icon->width, .height=icon->height};
            cover = &fallback;
        }
        render_recent_box(fb, cover);
        int x = SCREEN_WIDTH * 44 / 100, w = SCREEN_WIDTH - x - UI_S(29);
        int y = HEADER_HEIGHT + UI_S(26), step = font_get_size() + UI_S(6);
        render_rounded_rect_alpha(fb, x - UI_S(10), y - UI_S(10), w + UI_S(18),
                                  SCREEN_HEIGHT - y - UI_S(49), UI_S(8), COLOR_BG, 225);
        render_details_text(fb, coverflow_title_text, x, y, w, step, COLOR_TEXT);
        y += step;
        if (enhanced_recents_platform[0]) {
            render_details_text(fb, system_display_name(enhanced_recents_platform), x, y, w, step, COLOR_TEXT);
            y += step;
        }
        char label[128];
        const char *date = collection_detail_value(FILELIST_RELEASE_DATE);
        if (strspn(date, "0123456789") >= 4) {
            snprintf(label, sizeof label, "%s: %.4s", tr_or("enhanced.details.release_date", "Release"), date);
            render_details_text(fb, label, x, y, w, step, COLOR_TEXT);
            y += step;
        }
        struct tm local;
        if (details_play_stats.last > 0 && localtime_r(&details_play_stats.last, &local)) {
            strftime(label, sizeof label, "%Y-%m-%d %H:%M", &local);
            render_details_text(fb, label, x, y, w, step, COLOR_TEXT);
            y += step;
        }
        const long values[] = {details_play_stats.last_seconds, details_play_stats.seconds};
        const char *const titles[] = {
            tr_or("enhanced.details.last_session", "Last session"),
            tr_or("enhanced.details.playtime", "Total")
        };
        for (int i = 0; i < 2; i++) {
            if (values[i] <= 0) continue;
            char duration[32];
            format_details_duration(duration, sizeof duration, values[i]);
            snprintf(label, sizeof label, "%s: %s", titles[i], duration);
            render_details_text(fb, label, x, y, w, step, COLOR_TEXT);
            y += step;
        }
        if (details_play_stats.runs > 0) {
            snprintf(label, sizeof label, "%d %s", details_play_stats.runs,
                tr_or("enhanced.details.sessions", "sessions"));
            render_details_text(fb, label, x, y, w, step, COLOR_TEXT);
            y += step;
        }
        if (selected_favorite) {
            enhanced_home_utility_icon(fb, ENHANCED_HOME_FAVOURITES, x + UI_S(12),
                                       y + UI_S(12), UI_S(24), COLOR_TEXT);
            render_details_text(fb, tr("folder.favourites"), x + UI_S(28), y,
                                w - UI_S(28), step, COLOR_TEXT);
            y += step;
        }
        if (details_achievements_valid) {
            render_ra_trophy(fb, x, y, UI_S(24));
            render_details_text(fb, "RetroAchievements", x + UI_S(28), y, w - UI_S(28), step, COLOR_TEXT);
        }
        if (p9_rom_available)
            hint = tr_or("enhanced.recents.actions", "A Play   X Details   B Back");
        else
            render_details_text(fb, tr_or("enhanced.recents.missing", "ROM unavailable"), x, y, w, step, COLOR_DISABLED);

        snprintf(label, sizeof label, "%d / %d", selected_index + 1, entry_count);
        font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT,
                       SCREEN_WIDTH - UI_S(22) - font_measure_text(label),
                       SCREEN_HEIGHT - ITEM_HEIGHT - UI_S(5), label, COLOR_LEGEND);
    }
    font_draw_text(fb, SCREEN_WIDTH, SCREEN_HEIGHT, UI_S(22),
                   SCREEN_HEIGHT - ITEM_HEIGHT - UI_S(5), hint, COLOR_LEGEND);
}

#endif
