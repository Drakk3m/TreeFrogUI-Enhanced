#define _POSIX_C_SOURCE 200809L
#include "../theme_resource.h"
#define STB_IMAGE_IMPLEMENTATION
#include "../stb_image.h"

#include <assert.h>
#include <fcntl.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

#define VERSION "[theme]\nmanifest_version=1\n"

static unsigned char bmp[] = {
    'B','M',58,0,0,0,0,0,0,0,54,0,0,0,
    40,0,0,0,1,0,0,0,1,0,0,0,1,0,24,0,0,0,0,0,4,0,0,0,
    0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,255,0
};

static void put(const char *path, const void *data, size_t size) {
    FILE *file = fopen(path, "wb");
    assert(file && fwrite(data, 1, size, file) == size);
    assert(fclose(file) == 0);
}

static void manifest(const char *text) {
    put("theme.ini", text, strlen(text));
}

static void test_manifest(void) {
    ThemeResourceManager *manager = theme_resource_create(".", 2);
    assert(manager);
    assert(!theme_resource_load_manifest(manager, "missing.ini"));
    manifest("[theme]\nmanifest_version=2\n");
    assert(!theme_resource_load_manifest(manager, "theme.ini"));

    put("art.bmp", bmp, sizeof bmp);
    manifest(VERSION "id=mine\nname=My Theme\nscreen_width=640\nscreen_height=480\n"
             "[presentation]\ntransition_ms=300\nicon_scale_percent=120\n"
             "coverflow_neighbors=2\nreduced_motion=on\nfuture_key=ignore\n"
             "[resource.home.background]\npath=art.bmp\n"
             "[resource.platform.nes.background]\npath=art.bmp\n"
             "[resource.future.unrecognized]\npath=ignored\n");
    assert(theme_resource_load_manifest(manager, "theme.ini"));
    assert(strcmp(theme_resource_info(manager)->id, "mine") == 0);
    assert(strcmp(theme_resource_info(manager)->name, "My Theme") == 0);
    assert(theme_resource_supports_screen(manager, 640, 480));
    assert(!theme_resource_supports_screen(manager, 854, 480));
    const ThemePresentation *presentation = theme_resource_presentation(manager);
    assert(presentation->transition_ms == 300 && presentation->icon_scale_percent == 120);
    assert(presentation->coverflow_neighbors == 2 && presentation->reduced_motion);
    ThemeResourceDescriptor art;
    assert(theme_resource_resolve(manager, "home.background", &art));
    assert(art.width == 1 && art.height == 1 && art.kind == THEME_RESOURCE_BACKGROUND);
    assert(theme_resource_resolve(manager, "platform.nes.background", &art));
    assert(!theme_resource_resolve(manager, "future.unrecognized", &art));
    unsigned char *bytes = theme_resource_read(manager, &art);
    assert(bytes && memcmp(bytes, bmp, sizeof bmp) == 0);
    free(bytes);

    unsigned generation = theme_resource_generation(manager);
    manifest(VERSION "[presentation]\ntransition_ms=10000\n");
    assert(!theme_resource_load_manifest(manager, "theme.ini"));
    assert(theme_resource_generation(manager) == generation);
    assert(theme_resource_presentation(manager)->transition_ms == 300);
    put("theme.ini", VERSION "\0[resource.home.background]", sizeof(VERSION "\0[resource.home.background]") - 1);
    assert(!theme_resource_load_manifest(manager, "theme.ini"));
    assert(symlink("theme.ini", "linked.ini") == 0);
    assert(!theme_resource_load_manifest(manager, "linked.ini"));
    theme_resource_destroy(manager);
}

static void test_invalid_resources(void) {
    ThemeResourceManager *manager = theme_resource_create(".", 2);
    assert(manager);
    manifest(VERSION "[resource.platform.nes.icon]\npath=../art.bmp\n");
    assert(!theme_resource_load_manifest(manager, "theme.ini"));
    manifest(VERSION "[resource.platform.nes.icon]\npath=C:\\art.bmp\n");
    assert(!theme_resource_load_manifest(manager, "theme.ini"));
    manifest(VERSION "[resource.platform.nes.icon]\npath=/art.bmp\n");
    assert(!theme_resource_load_manifest(manager, "theme.ini"));

    put("corrupt.bmp", "BMbad", 5);
    assert(symlink("art.bmp", "linked.bmp") == 0);
    assert(symlink(".", "linked-dir") == 0);
    int fd = open("large.bmp", O_CREAT | O_WRONLY, 0600);
    assert(fd >= 0 && ftruncate(fd, THEME_RESOURCE_FILE_MAX + 1) == 0 && close(fd) == 0);
    bmp[18] = 1;
    bmp[19] = 1;
    put("too-wide.bmp", bmp, sizeof bmp); /* 257 pixels wide */
    bmp[19] = 0;
    manifest(VERSION
             "[resource.platform.missing.icon]\npath=absent.bmp\n"
             "[resource.platform.corrupt.icon]\npath=corrupt.bmp\n"
             "[resource.platform.link.icon]\npath=linked.bmp\n"
             "[resource.platform.dirlink.icon]\npath=linked-dir/art.bmp\n"
             "[resource.platform.large.icon]\npath=large.bmp\n"
             "[resource.platform.wide.icon]\npath=too-wide.bmp\n");
    assert(theme_resource_load_manifest(manager, "theme.ini"));
    ThemeResourceDescriptor art;
    const char *ids[] = { "missing", "corrupt", "link", "dirlink", "large", "wide" };
    for (size_t i = 0; i < sizeof ids / sizeof ids[0]; i++) {
        char id[64];
        snprintf(id, sizeof id, "platform.%s.icon", ids[i]);
        assert(!theme_resource_resolve(manager, id, &art));
    }
    assert(theme_resource_cache_count(manager) == 0);
    theme_resource_destroy(manager);
}

static void test_cache_and_switch(void) {
    ThemeResourceManager *manager = theme_resource_create(".", 2);
    assert(manager && !theme_resource_create(".", THEME_RESOURCE_CACHE_MAX + 1));
    put("a.bmp", bmp, sizeof bmp);
    put("b.bmp", bmp, sizeof bmp);
    put("c.bmp", bmp, sizeof bmp);
    manifest(VERSION "[resource.platform.a.icon]\npath=a.bmp\n"
             "[resource.platform.b.icon]\npath=b.bmp\n"
             "[resource.platform.c.icon]\npath=c.bmp\n");
    assert(theme_resource_load_manifest(manager, "theme.ini"));
    ThemeResourceDescriptor a, b, c;
    assert(theme_resource_resolve(manager, "platform.a.icon", &a));
    assert(theme_resource_resolve(manager, "platform.b.icon", &b));
    assert(theme_resource_resolve(manager, "platform.a.icon", &a)); /* make a most-recent */
    assert(unlink("a.bmp") == 0 && unlink("b.bmp") == 0);
    assert(theme_resource_resolve(manager, "platform.c.icon", &c));
    assert(theme_resource_cache_count(manager) == 2);
    assert(!theme_resource_resolve(manager, "platform.b.icon", &b));
    assert(theme_resource_resolve(manager, "platform.a.icon", &a)); /* descriptor hit, no I/O */
    assert(!theme_resource_read(manager, &a)); /* file gone, retain safe fallback */

    manifest(VERSION "[resource.platform.a.icon]\npath=c.bmp\n");
    assert(theme_resource_load_manifest(manager, "theme.ini"));
    assert(theme_resource_cache_count(manager) == 0);
    assert(!theme_resource_read(manager, &a)); /* old generation */
    assert(theme_resource_resolve(manager, "platform.a.icon", &c));
    assert(strcmp(c.path, "c.bmp") == 0 && c.generation != a.generation);
    theme_resource_destroy(manager);
}

static void test_memory_limit(void) {
    ThemeResourceManager *manager = theme_resource_create(".", THEME_RESOURCE_CACHE_MAX);
    assert(manager);
    FILE *file = fopen("theme.ini", "wb");
    assert(file && fputs(VERSION, file) >= 0);
    for (int i = 0; i < 128; i++)
        assert(fprintf(file, "[resource.platform.p%d.icon]\npath=art.bmp\n", i) > 0);
    assert(fclose(file) == 0);
    assert(theme_resource_load_manifest(manager, "theme.ini"));
    size_t bytes = theme_resource_allocation_bytes(manager);
    assert(bytes < 48 * 1024);
    printf("Manager heap requested at 128 manifest entries and 16 cache slots: %zu bytes\n", bytes);
    theme_resource_destroy(manager);
}

int main(int argc, char **argv) {
    assert(argc == 2 && chdir(argv[1]) == 0);
    test_manifest();
    test_invalid_resources();
    test_cache_and_switch();
    test_memory_limit();
    puts("PASS theme_resource");
    return 0;
}
