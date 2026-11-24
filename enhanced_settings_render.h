#ifndef ENHANCED_SETTINGS_RENDER_H
#define ENHANCED_SETTINGS_RENDER_H

static void settings_panel_text(const char *text, int x, int y, int w, int h, uint16_t color) {
    if (w <= 0 || h <= 0) return;
    char clipped[160];
    if (font_measure_text(text) > w) {
        fit_enhanced_title(clipped, sizeof clipped, text, w);
        text = clipped;
    }
    int baseline, cap_height;
    font_cap_metrics(&baseline, &cap_height);
    int text_y = y + h / 2 - (baseline - cap_height / 2);
    font_draw_text_clipped(framebuffer, SCREEN_WIDTH, SCREEN_HEIGHT, x, text_y,
                           text, color, x, y, w, h);
}

static void settings_panel_category_text(const char *text, int x, int y, int w, int h,
                                         uint16_t color) {
    if (font_measure_text(text) <= w) {
        settings_panel_text(text, x, y, w, h, color);
        return;
    }
    char line[128];
    size_t split = 0;
    for (const char *space = strchr(text, ' '); space; space = strchr(space + 1, ' ')) {
        size_t size = (size_t)(space - text);
        if (size >= sizeof line) break;
        snprintf(line, sizeof line, "%.*s", (int)size, text);
        if (font_measure_text(line) > w) break;
        split = size;
    }
    if (!split) {
        settings_panel_text(text, x, y, w, h, color);
        return;
    }
    snprintf(line, sizeof line, "%.*s", (int)split, text);
    settings_panel_text(line, x, y, w, h / 2, color);
    settings_panel_text(text + split + 1, x, y + h / 2, w, h - h / 2, color);
}

static const char *settings_category_title(int category) {
    const EnhancedSettingsCategory *group = &enhanced_settings_categories[category];
    return tr_or(group->title, group->fallback);
}

static int settings_panel_scroll(int offset, int selected, int count, int visible) {
    if (selected < offset) offset = selected;
    if (selected >= offset + visible) offset = selected - visible + 1;
    int max_offset = count > visible ? count - visible : 0;
    return offset > max_offset ? max_offset : offset < 0 ? 0 : offset;
}

static void settings_panel_scrollbar(int x, int y, int height, int count, int visible, int offset) {
    if (count <= visible || height <= 0) return;
    int handle = height * visible / count;
    if (handle < UI_S(8)) handle = UI_S(8);
    int at = (height - handle) * offset / (count - visible);
    render_fill_rect(framebuffer, x, y, UI_S(3), height, COLOR_LEGEND_BG);
    render_fill_rect(framebuffer, x, y + at, UI_S(3), handle, COLOR_HEADER);
}

static void settings_panel_row(int row, bool selected, int x, int y, int w, int h) {
    const SRow *setting = &settings_rows[row];
    bool unavailable = enhanced_settings_row_disabled(row);
    uint16_t text = unavailable ? COLOR_DISABLED : selected ? COLOR_SELECT_TEXT : COLOR_TEXT;
    if (selected)
        render_rounded_rect_alpha(framebuffer, x, y, w, h - UI_S(2), UI_S(5),
                                  COLOR_SELECT_BG, unavailable ? 108 : 255);
    int inset = UI_S(10);
    char value[128];
    settings_option_value(row, value, sizeof value);
    int control = w * (row == SR_RTC_TIME ? 56 : 42) / 100;
    NavSettingKind kind = enhanced_settings_kind(row);
    bool interface = row == SR_GRAPHICAL_HOME;
    bool render_value = value[0] && (kind != NAV_SETTING_TOGGLE || interface);
    int label_width = w - 2 * inset - ((render_value || kind == NAV_SETTING_TOGGLE ||
                                      kind == NAV_SETTING_ACTION) ? control : 0);
    settings_panel_text(settings_option_label(row), x + inset, y, label_width, h, text);

    int right = x + w - inset;
    if (kind == NAV_SETTING_TOGGLE) {
        int width = UI_S(39), height = UI_S(19);
        int bx = right - width;
        int by = y + (h - height) / 2;
        bool on = *setting->val != 0;
        render_rounded_rect(framebuffer, bx, by, width, height, height / 2,
                            on && !unavailable ? COLOR_HEADER : COLOR_LEGEND_BG);
        int knob = height - UI_S(4);
        uint16_t knob_color = unavailable ? COLOR_DISABLED : COLOR_LEGEND;
        int kx = on ? right - knob - UI_S(2) : bx + UI_S(2);
        render_rounded_rect(framebuffer, kx, by + UI_S(2), knob, knob, knob / 2, knob_color);
        if (interface) {
            int value_width = font_measure_text(value), available = control - UI_S(48);
            if (value_width > available) value_width = available;
            if (value_width > 0)
                settings_panel_text(value, bx - UI_S(8) - value_width, y, value_width, h, text);
        }
    } else if (kind == NAV_SETTING_RANGE) {
        int bar_width = UI_S(50), bar_x = right - bar_width;
        int filled = (*setting->val - setting->rmin) * bar_width /
                     (setting->rmax - setting->rmin);
        if (filled < 0) filled = 0;
        if (filled > bar_width) filled = bar_width;
        int bar_y = y + h / 2;
        render_fill_rect(framebuffer, bar_x, bar_y, bar_width, UI_S(3), COLOR_LEGEND_BG);
        if (filled) render_fill_rect(framebuffer, bar_x, bar_y, filled, UI_S(3), text);
        int value_width = font_measure_text(value);
        int available = control - bar_width - UI_S(8);
        if (value_width > available) value_width = available;
        settings_panel_text(value, bar_x - UI_S(8) - value_width, y, value_width, h, text);
    } else {
        int affordance = !unavailable && (kind == NAV_SETTING_CHOICE || kind == NAV_SETTING_ACTION)
                       ? UI_S(16) : 0;
        if (affordance)
            settings_panel_text(">", right - affordance + UI_S(4), y, affordance, h, text);
        if (render_value) {
            int width = font_measure_text(value);
            int maximum = control - affordance;
            if (width > maximum) width = maximum;
            settings_panel_text(value, right - affordance - width, y, width, h, text);
        }
    }
}

static void render_enhanced_settings_menu(void) {
    const int margin = UI_S(16), gap = UI_S(12), left_w = UI_S(184);
    const int top = UI_S(57), bottom = SCREEN_HEIGHT - UI_S(64);
    const int right_x = margin + left_w + gap, right_w = SCREEN_WIDTH - right_x - margin;
    const int heading_h = UI_S(40), row_h = UI_S(38);
    const int rows_top = top + heading_h;
    int visible = (bottom - rows_top - UI_S(4)) / row_h;
    if (visible < 1) visible = 1;
    int category = enhanced_settings_category;
    int cursor = enhanced_settings_row_cursor();
    bool options = enhanced_settings_options_focused();
    bool selector = enhanced_settings_selector_active;

    if (banner_is_loaded()) banner_render(framebuffer);
    else render_clear_screen(framebuffer);
    render_header(framebuffer, "");
    settings_panel_text(tr("tab.settings"), margin, 0,
                        SCREEN_WIDTH - margin - UI_S(76), HEADER_HEIGHT, COLOR_HEADER);

    render_rounded_rect_alpha(framebuffer, margin, top, left_w, bottom - top,
                              UI_S(8), COLOR_LEGEND_BG, 172);
    render_rounded_rect_alpha(framebuffer, right_x, top, right_w, bottom - top,
                              UI_S(8), COLOR_LEGEND_BG, 172);

    int category_h = (bottom - top - UI_S(12)) / ENHANCED_SETTINGS_CATEGORY_COUNT;
    for (int i = 0; i < ENHANCED_SETTINGS_CATEGORY_COUNT; i++) {
        int x = margin + UI_S(5), y = top + UI_S(6) + i * category_h;
        int tile_w = left_w - UI_S(10);
        if (i == category)
            render_rounded_rect_alpha(framebuffer, x, y, tile_w, category_h - UI_S(3),
                                      UI_S(5), COLOR_SELECT_BG, options ? 108 : 255);
        const SystemIcon *icon = enhanced_settings_icons[i];
        int start = x + UI_S(9);
        if (icon) {
            int icon_edge = UI_S(22);
            system_icon_draw(framebuffer, icon, start, y + (category_h - icon_edge) / 2,
                             icon_edge, icon_edge);
            start += icon_edge + UI_S(8);
        }
        settings_panel_category_text(settings_category_title(i), start, y,
                                     x + tile_w - UI_S(6) - start, category_h,
                                     i == category && !options ? COLOR_SELECT_TEXT : COLOR_TEXT);
    }

    const char *heading = settings_rtc_edit_active ? settings_option_label(SR_RTC_TIME)
                        : selector ? settings_option_label(enhanced_settings_selector_row)
                                   : settings_category_title(category);
    settings_panel_text(heading, right_x + UI_S(14), top, right_w - UI_S(28), heading_h, COLOR_HEADER);
    int count = selector ? enhanced_settings_selector_count() : enhanced_settings_row_count();
    int selection = selector ? enhanced_settings_selector_cursor : cursor;
    int offset = settings_panel_scroll(selector ? 0 : enhanced_settings_scroll[category],
                                       selection, count, visible);
    if (!selector) enhanced_settings_scroll[category] = offset;
    int x = right_x + UI_S(6), w = right_w - UI_S(18);
    if (settings_rtc_edit_active) {
        char value[64];
        settings_option_value(SR_RTC_TIME, value, sizeof value);
        render_rounded_rect(framebuffer, x, rows_top, w, row_h, UI_S(5), COLOR_SELECT_BG);
        settings_panel_text(value, x + UI_S(10), rows_top, w - UI_S(20), row_h,
                            COLOR_SELECT_TEXT);
        count = 0;
    }
    for (int pos = offset; pos < count && pos < offset + visible; pos++) {
        int y = rows_top + (pos - offset) * row_h;
        if (selector) {
            bool selected = pos == selection;
            if (selected)
                render_rounded_rect(framebuffer, x, y, w, row_h - UI_S(2), UI_S(5), COLOR_SELECT_BG);
            settings_panel_text(enhanced_settings_selector_name(pos), x + UI_S(10), y,
                                w - UI_S(20), row_h, selected ? COLOR_SELECT_TEXT : COLOR_TEXT);
        } else {
            settings_panel_row(enhanced_settings_row_at(pos), options && pos == cursor,
                               x, y, w, row_h);
        }
    }
    settings_panel_scrollbar(right_x + right_w - UI_S(7), rows_top,
                             visible * row_h, count, visible, offset);

    int row = enhanced_settings_row_at(cursor);
    const char *notice = settings_rtc_edit_active
        ? tr_or("enhanced.settings.rtc_fields", "Left/Right Field   Up/Down Adjust")
        : settings_option_notice(row);
    if (options && !selector && *notice)
        settings_panel_text(notice, margin, bottom, SCREEN_WIDTH - 2 * margin, UI_S(24), COLOR_HEADER);
    const char *hints;
    if (settings_rtc_edit_active)
        hints = tr_or("enhanced.settings.rtc_controls", "A Save   B Cancel");
    else if (selector)
        hints = tr_or("enhanced.settings.selector_controls", "D-pad Choose   A Confirm   B Cancel");
    else if (options && row >= 0 && (enhanced_settings_row_disabled(row) ||
                                      enhanced_settings_kind(row) == NAV_SETTING_STATUS))
        hints = tr_or("enhanced.settings.status_controls", "Up/Down Navigate   B Back");
    else if (options && row >= 0 && enhanced_settings_kind(row) == NAV_SETTING_RANGE)
        hints = tr_or("enhanced.settings.range_controls", "Left/Right Adjust   B Back");
    else if (options)
        hints = tr_or("enhanced.settings.option_controls", "D-pad Navigate   A Select   B Back");
    else
        hints = tr_or("enhanced.settings.category_controls", "D-pad Navigate   A Open   B Save & Exit");
    settings_panel_text(hints, margin, SCREEN_HEIGHT - UI_S(36),
                        SCREEN_WIDTH - 2 * margin, UI_S(30), COLOR_LEGEND);
}

#endif
