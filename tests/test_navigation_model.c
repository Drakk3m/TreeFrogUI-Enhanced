#include "../navigation_model.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    bool platform;
    bool game;
} LibraryFixture;

static bool has_platform(void *context, const char *id) {
    const LibraryFixture *fixture = context;
    return fixture->platform && strcmp(id, "NES") == 0;
}

static bool has_game(void *context, const char *platform, const char *id) {
    const LibraryFixture *fixture = context;
    return fixture->game && strcmp(platform, "NES") == 0 &&
           strcmp(id, "Game = edition.nes") == 0;
}

static void route(NavModel *model, const NavCatalog *catalog, NavInput input,
                  NavEffect expected, NavScreen screen) {
    NavEffect actual = NAV_EFFECT_NONE;
    assert(nav_model_handle(model, catalog, input, &actual) == (expected != NAV_EFFECT_NONE));
    assert(actual == expected);
    assert(model->state.screen == screen);
}

static const NavSetting categories[] = {
    { .id = "disabled", .kind = NAV_SETTING_ACTION },
    { .id = "toggle", .kind = NAV_SETTING_TOGGLE, .enabled = true },
    { .id = "choice", .kind = NAV_SETTING_CHOICE, .enabled = true },
    { .id = "range", .kind = NAV_SETTING_RANGE, .enabled = true },
    { .id = "nested", .kind = NAV_SETTING_SUBCATEGORY, .enabled = true,
      .children = categories, .child_count = 5 },
};

static void test_settings(NavModel *model, const NavCatalog *catalog, NavScreen origin,
                          NavEffect return_effect) {
    route(model, catalog, NAV_INPUT_START, NAV_EFFECT_OPEN_SETTINGS, NAV_SETTINGS);
    assert(nav_settings_level(model)->count == 5);
    assert(nav_settings_level(model)->items[0].title_key);
    assert(nav_settings_level(model)->items[0].description_key);
    assert(nav_settings_set_hub(model, categories, sizeof categories / sizeof categories[0]));

    route(model, catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_NONE, NAV_SETTINGS);
    assert(nav_settings_level(model)->selected == 0);
    route(model, catalog, NAV_INPUT_DOWN, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
    route(model, catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_ACTIVATE_SETTING, NAV_SETTINGS);
    route(model, catalog, NAV_INPUT_DOWN, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
    route(model, catalog, NAV_INPUT_RIGHT, NAV_EFFECT_ADJUST_SETTING, NAV_SETTINGS);
    route(model, catalog, NAV_INPUT_DOWN, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
    route(model, catalog, NAV_INPUT_LEFT, NAV_EFFECT_ADJUST_SETTING, NAV_SETTINGS);
    route(model, catalog, NAV_INPUT_DOWN, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
    route(model, catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
    assert(model->settings_depth == 1 && nav_settings_level(model)->selected == 0);
    route(model, catalog, NAV_INPUT_BACK, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
    assert(model->settings_depth == 0 && nav_settings_level(model)->selected == 4);

    for (size_t depth = 0; depth <= NAV_SETTINGS_MAX_DEPTH; depth++) {
        while (nav_settings_level(model)->selected < 4)
            route(model, catalog, NAV_INPUT_DOWN, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
        route(model, catalog, NAV_INPUT_CONFIRM,
              depth < NAV_SETTINGS_MAX_DEPTH ? NAV_EFFECT_MOVE_SELECTION : NAV_EFFECT_NONE,
              NAV_SETTINGS);
    }
    assert(model->settings_depth == NAV_SETTINGS_MAX_DEPTH);
    for (size_t depth = NAV_SETTINGS_MAX_DEPTH; depth > 0; depth--) {
        route(model, catalog, NAV_INPUT_BACK, NAV_EFFECT_MOVE_SELECTION, NAV_SETTINGS);
        assert(model->settings_depth == depth - 1 && nav_settings_level(model)->selected == 4);
    }
    route(model, catalog, NAV_INPUT_BACK, return_effect, origin);
}

static void test_routes(void) {
    LibraryFixture fixture = { true, true };
    NavCatalog catalog = { has_platform, has_game, &fixture };
    NavModel model;
    nav_model_init(&model, false);
    route(&model, &catalog, NAV_INPUT_START, NAV_EFFECT_NONE, NAV_PLATFORM_HOME);
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_NONE, NAV_PLATFORM_HOME);
    nav_model_init(&model, true);
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_NONE, NAV_PLATFORM_HOME);
    assert(!nav_model_set_platform(&model, &catalog, "Missing"));
    assert(nav_model_set_platform(&model, &catalog, "NES"));
    test_settings(&model, &catalog, NAV_PLATFORM_HOME, NAV_EFFECT_RETURN_TO_HOME);
    assert(strcmp(model.state.platform_id, "NES") == 0);
    route(&model, &catalog, NAV_INPUT_RIGHT, NAV_EFFECT_MOVE_SELECTION, NAV_PLATFORM_HOME);
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_OPEN_LIBRARY, NAV_GAME_LIBRARY);
    assert(!nav_model_set_game(&model, &catalog, "Missing.nes"));
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_NONE, NAV_GAME_LIBRARY);
    assert(nav_model_set_game(&model, &catalog, "Game = edition.nes"));
    nav_model_init(&model, true);
    assert(nav_model_set_platform(&model, &catalog, "NES"));
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_OPEN_LIBRARY, NAV_GAME_LIBRARY);
    assert(nav_model_set_game(&model, &catalog, "Game = edition.nes"));
    test_settings(&model, &catalog, NAV_GAME_LIBRARY, NAV_EFFECT_RETURN_TO_LIBRARY);
    assert(strcmp(model.state.game_id, "Game = edition.nes") == 0);

    /* Reset the hub for this new fixture, preserving the browser context. */
    NavState library = model.state;
    nav_model_init(&model, true);
    model.state = library;
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_OPEN_DETAILS, NAV_GAME_DETAILS);
    route(&model, &catalog, NAV_INPUT_DOWN, NAV_EFFECT_SCROLL_DETAILS, NAV_GAME_DETAILS);
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_LAUNCH_GAME, NAV_GAME_DETAILS);
    test_settings(&model, &catalog, NAV_GAME_DETAILS, NAV_EFFECT_RETURN_TO_DETAILS);
    route(&model, &catalog, NAV_INPUT_BACK, NAV_EFFECT_RETURN_TO_LIBRARY, NAV_GAME_LIBRARY);
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_OPEN_DETAILS, NAV_GAME_DETAILS);
    route(&model, &catalog, NAV_INPUT_START, NAV_EFFECT_OPEN_SETTINGS, NAV_SETTINGS);
    fixture.game = false;
    route(&model, &catalog, NAV_INPUT_BACK, NAV_EFFECT_RETURN_TO_LIBRARY, NAV_GAME_LIBRARY);
    assert(!model.state.game_id[0]);
    route(&model, &catalog, NAV_INPUT_CONFIRM, NAV_EFFECT_NONE, NAV_GAME_LIBRARY);
    fixture.platform = false;
    route(&model, &catalog, NAV_INPUT_BACK, NAV_EFFECT_RETURN_TO_HOME, NAV_PLATFORM_HOME);
    assert(!model.state.platform_id[0]);
}

static void test_window(void) {
    NavHorizontalWindow window;
    nav_window_init(&window, 0, 4, 0);
    assert(!nav_window_move(&window, 0, 4, 1));
    assert(window.selected == -1 && window.offset == 0 && window.transition_target == -1);
    nav_window_init(&window, 1, 4, 0);
    assert(!nav_window_move(&window, 1, 4, -1));
    assert(!nav_window_move(&window, 1, 4, 1));
    assert(window.selected == 0 && window.offset == 0);
    nav_window_init(&window, 7, 4, 0);
    assert(!nav_window_move(&window, 7, 4, -1));
    for (int index = 1; index < 7; index++) {
        window.transition_progress = 0.25f;
        assert(nav_window_move(&window, 7, 4, 1));
        assert(window.selected == index && window.offset == (index < 3 ? index : 3));
        assert(window.transition_target == index && window.transition_progress == 0.0f);
    }
    assert(!nav_window_move(&window, 7, 4, 1));
    for (int index = 5; index >= 0; index--) {
        assert(nav_window_move(&window, 7, 4, -1));
        assert(window.selected == index && window.offset == (index < 3 ? index : 3));
    }
    for (int i = 0; i < 1000; i++) {
        int direction = i % 23 < 13 ? 1 : -1;
        nav_window_move(&window, 7, 4, direction);
        assert(window.selected >= 0 && window.selected < 7);
        assert(window.offset >= 0 && window.offset <= 3);
        assert(window.selected >= window.offset && window.selected - window.offset < 4);
        assert(window.transition_target == window.selected);
    }
    nav_window_move(&window, 2, 1, 0);
    assert(window.offset == window.selected && window.selected < 2);
}

static void test_snapshot(void) {
    LibraryFixture fixture = { true, true };
    NavCatalog catalog = { has_platform, has_game, &fixture };
    NavModel previous, restarted;
    NavHorizontalWindow window;
    nav_model_init(&previous, true);
    nav_model_init(&restarted, true);
    assert(nav_model_set_platform(&previous, &catalog, "NES"));
    char bytes[1024], again[1024];
    size_t length, next_length;
    assert(nav_model_save(&previous, bytes, sizeof bytes, &length));
    assert(nav_model_load(&restarted, &catalog, bytes, length));
    assert(strcmp(restarted.state.platform_id, "NES") == 0 && !restarted.state.game_id[0]);
    assert(!nav_model_set_game(&previous, NULL, "bad\nname"));
    assert(nav_model_set_game(&previous, &catalog, "Game = edition.nes"));
    previous.state.screen = NAV_GAME_DETAILS;
    assert(nav_model_save(&previous, bytes, sizeof bytes, &length));
    nav_window_init(&window, 8, 3, 1);
    nav_window_move(&window, 8, 3, 1);
    window.transition_progress = 0.8f;
    assert(nav_model_save(&previous, again, sizeof again, &next_length));
    assert(length == next_length && memcmp(bytes, again, length) == 0);
    /* Deserialization must accept a bounded, non-NUL-terminated input. */
    memset(bytes + length, 'x', sizeof bytes - length);
    assert(nav_model_load(&restarted, &catalog, bytes, length));
    assert(restarted.state.screen == NAV_PLATFORM_HOME);
    assert(strcmp(restarted.state.game_id, "Game = edition.nes") == 0);
    assert(!nav_model_load(&restarted, &catalog, bytes, length + 1));
    assert(!nav_model_load(&restarted, &catalog, bytes, length - 1));
    assert(!nav_model_load(&restarted, NULL, bytes, length));
    bytes[8] = '9';
    assert(!nav_model_load(&restarted, &catalog, bytes, length));
    bytes[8] = '1';
    fixture.game = false;
    assert(!nav_model_load(&restarted, &catalog, bytes, length));
    fixture.platform = false;
    assert(!nav_model_load(&restarted, &catalog, bytes, length));
    assert(strcmp(restarted.state.platform_id, "NES") == 0);
    assert(strcmp(restarted.state.game_id, "Game = edition.nes") == 0);
}

int main(void) {
    test_routes();
    test_window();
    test_snapshot();
    puts("PASS navigation model");
    return 0;
}
