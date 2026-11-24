#ifndef FROGUI_THEME_RESOURCE_H
#define FROGUI_THEME_RESOURCE_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#define THEME_RESOURCE_PATH_MAX 256
#define THEME_RESOURCE_ID_MAX 64
#define THEME_RESOURCE_CACHE_MAX 16
#define THEME_RESOURCE_FILE_MAX (1024u * 1024u)
#define THEME_BACKGROUND_MAX_WIDTH 640
#define THEME_BACKGROUND_MAX_HEIGHT 480
#define THEME_IMAGE_MAX_EDGE 256
#define THEME_SOUND_RATE 44100
#define THEME_SOUND_MAX_FRAMES (THEME_SOUND_RATE / 10)
#define THEME_SOUND_MAX_BYTES (THEME_SOUND_MAX_FRAMES * 2 + 512)

typedef enum {
    THEME_RESOURCE_BACKGROUND,
    THEME_RESOURCE_PLATFORM_ICON,
    THEME_RESOURCE_SETTINGS_ICON,
    THEME_RESOURCE_COVER_PLACEHOLDER,
    THEME_RESOURCE_ACHIEVEMENT_BADGE,
    THEME_RESOURCE_NAV_SOUND,
    THEME_RESOURCE_APP_ICON,
    THEME_RESOURCE_COUNT
} ThemeResourceKind;

typedef struct {
    char id[THEME_RESOURCE_ID_MAX];
    char path[THEME_RESOURCE_PATH_MAX]; /* relative to the open theme directory */
    ThemeResourceKind kind;
    unsigned width, height;
    size_t file_size;
    unsigned generation;
} ThemeResourceDescriptor;

typedef struct {
    char id[THEME_RESOURCE_ID_MAX];
    char name[128];
    unsigned screen_width, screen_height;
} ThemeResourceInfo;

typedef struct {
    unsigned transition_ms;
    unsigned icon_scale_percent;
    unsigned icon_spacing;
    unsigned coverflow_neighbors;
    unsigned coverflow_side_scale_percent;
    unsigned coverflow_spacing;
    bool background_crossfade;
    bool reduced_motion;
    bool navigation_sounds;
} ThemePresentation;

typedef struct ThemeResourceManager ThemeResourceManager;

ThemeResourceManager *theme_resource_create(const char *root, size_t cache_capacity);
void theme_resource_destroy(ThemeResourceManager *manager);
/* A failed reload leaves the previous manifest intact. */
bool theme_resource_load_manifest(ThemeResourceManager *manager, const char *relative_path);
void theme_resource_invalidate(ThemeResourceManager *manager);
bool theme_resource_resolve(ThemeResourceManager *manager, const char *id,
                            ThemeResourceDescriptor *out);
/* Returns a bounded, caller-owned blob to be freed with free(). The caller
 * probes and decodes those same bytes using its native decoder. */
unsigned char *theme_resource_read(const ThemeResourceManager *manager,
                                   const ThemeResourceDescriptor *resource);
/* Mono signed PCM16 at THEME_SOUND_RATE. Returns zero on validation failure. */
size_t theme_resource_sound_pcm(const ThemeResourceManager *manager,
                                const ThemeResourceDescriptor *resource,
                                int16_t pcm[THEME_SOUND_MAX_FRAMES]);
const ThemeResourceInfo *theme_resource_info(const ThemeResourceManager *manager);
const ThemePresentation *theme_resource_presentation(const ThemeResourceManager *manager);
bool theme_resource_supports_screen(const ThemeResourceManager *manager, unsigned w, unsigned h);
size_t theme_resource_cache_count(const ThemeResourceManager *manager);
size_t theme_resource_allocation_bytes(const ThemeResourceManager *manager);
unsigned theme_resource_generation(const ThemeResourceManager *manager);

#endif
