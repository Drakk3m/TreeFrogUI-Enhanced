#define _POSIX_C_SOURCE 200809L
#include "theme_resource.h"
#include "stb_image.h"

#include <ctype.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define MANIFEST_MAX_BYTES 16384u
#define MANIFEST_MAX_RESOURCES 128u

typedef struct {
    char id[THEME_RESOURCE_ID_MAX];
    char path[THEME_RESOURCE_PATH_MAX];
    ThemeResourceKind kind;
} ManifestResource;

typedef struct {
    ThemeResourceInfo info;
    ThemePresentation presentation;
    ManifestResource *resources;
    size_t count, capacity;
} Manifest;

typedef struct {
    ThemeResourceDescriptor descriptor;
    unsigned long stamp;
} CacheEntry;

struct ThemeResourceManager {
    int root_fd;
    Manifest manifest;
    CacheEntry *cache;
    size_t cache_capacity, cache_count;
    unsigned long clock;
    unsigned generation;
};

static const ThemePresentation defaults = {
    .transition_ms = 180,
    .icon_scale_percent = 115,
    .icon_spacing = 8,
    .coverflow_neighbors = 1,
    .coverflow_side_scale_percent = 75,
    .coverflow_spacing = 8,
};

static bool copy_field(char *destination, size_t capacity, const char *value) {
    size_t length = strlen(value);
    if (!length || length >= capacity) return false;
    memcpy(destination, value, length + 1);
    return true;
}

static bool ascii_token(const char *token, size_t length) {
    if (length == 0 || length >= THEME_RESOURCE_ID_MAX) return false;
    for (size_t i = 0; i < length; i++) {
        char c = token[i];
        if (!(c >= 'a' && c <= 'z') && !(c >= '0' && c <= '9') && c != '-' && c != '_')
            return false;
    }
    return true;
}

static bool relative_path_valid(const char *path) {
    if (!path || !*path || strlen(path) >= THEME_RESOURCE_PATH_MAX) return false;
    const char *start = path;
    for (const char *p = path;; p++) {
        unsigned char c = *p;
        if (c == '/' || c == '\0') {
            size_t length = (size_t)(p - start);
            if (!length || (length == 1 && start[0] == '.') ||
                (length == 2 && start[0] == '.' && start[1] == '.')) return false;
            if (!c) return true;
            start = p + 1;
        } else if (c < 32 || c == 127 || c == '\\' || c == ':') {
            return false;
        }
    }
}

/* Each path component is opened relative to a held directory descriptor.
 * Symlinks, FIFOs and nonregular leaves cannot escape the theme directory. */
static FILE *open_regular(const ThemeResourceManager *manager, const char *path,
                          size_t maximum, size_t *size) {
    if (!relative_path_valid(path)) return NULL;
    char parts[THEME_RESOURCE_PATH_MAX];
    strcpy(parts, path);
    char *part = parts;
    char *slash;
    int directory = dup(manager->root_fd);
    if (directory < 0) return NULL;
    while ((slash = strchr(part, '/')) != NULL) {
        *slash = '\0';
        int next = openat(directory, part, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        close(directory);
        if (next < 0) return NULL;
        directory = next;
        part = slash + 1;
    }
    int fd = openat(directory, part, O_RDONLY | O_NONBLOCK | O_NOFOLLOW | O_CLOEXEC);
    close(directory);
    if (fd < 0) return NULL;
    struct stat st;
    if (fstat(fd, &st) != 0 || !S_ISREG(st.st_mode) || st.st_size <= 0 ||
        (uintmax_t)st.st_size > maximum) {
        close(fd);
        return NULL;
    }
    FILE *file = fdopen(fd, "rb");
    if (!file) {
        close(fd);
        return NULL;
    }
    *size = (size_t)st.st_size;
    return file;
}

static char *trim(char *text) {
    while (isspace((unsigned char)*text)) text++;
    char *end = text + strlen(text);
    while (end > text && isspace((unsigned char)end[-1])) *--end = '\0';
    return text;
}

static bool number(const char *text, unsigned min, unsigned max, unsigned *out) {
    if (*text < '0' || *text > '9') return false;
    char *end;
    unsigned long value = strtoul(text, &end, 10);
    if (*end || value < min || value > max) return false;
    *out = (unsigned)value;
    return true;
}

static bool flag(const char *value, bool *out) {
    if (strcmp(value, "on") == 0) { *out = true; return true; }
    if (strcmp(value, "off") == 0) { *out = false; return true; }
    return false;
}

static bool scoped_id(const char *id, const char *prefix, const char *suffix) {
    size_t n = strlen(id), p = strlen(prefix), s = strlen(suffix);
    return n > p + s && strncmp(id, prefix, p) == 0 &&
           strcmp(id + n - s, suffix) == 0 && ascii_token(id + p, n - p - s);
}

static ThemeResourceKind resource_kind(const char *id) {
    if (strcmp(id, "home.background") == 0 || scoped_id(id, "platform.", ".background"))
        return THEME_RESOURCE_BACKGROUND;
    if (scoped_id(id, "platform.", ".icon")) return THEME_RESOURCE_PLATFORM_ICON;
    if (scoped_id(id, "app.", ".icon")) return THEME_RESOURCE_APP_ICON;
    if (scoped_id(id, "settings.", ".icon")) return THEME_RESOURCE_SETTINGS_ICON;
    if (strcmp(id, "cover.placeholder") == 0) return THEME_RESOURCE_COVER_PLACEHOLDER;
    if (strcmp(id, "achievement.badge") == 0 || strcmp(id, "ra.trophy") == 0)
        return THEME_RESOURCE_ACHIEVEMENT_BADGE;
    if (strcmp(id, "navigation.sound") == 0 ||
        strcmp(id, "navigation.move.sound") == 0 ||
        strcmp(id, "navigation.select.sound") == 0 ||
        strcmp(id, "navigation.confirm.sound") == 0 ||
        strcmp(id, "navigation.cancel.sound") == 0) return THEME_RESOURCE_NAV_SOUND;
    return THEME_RESOURCE_COUNT;
}

static const ManifestResource *find_resource(const Manifest *manifest, const char *id) {
    for (size_t i = 0; i < manifest->count; i++)
        if (strcmp(manifest->resources[i].id, id) == 0)
            return &manifest->resources[i];
    return NULL;
}

static bool add_resource(Manifest *manifest, const char *id, const char *path,
                         ThemeResourceKind kind) {
    if (!relative_path_valid(path) || strlen(id) >= THEME_RESOURCE_ID_MAX ||
        find_resource(manifest, id) || manifest->count >= MANIFEST_MAX_RESOURCES)
        return false;
    if (manifest->count == manifest->capacity) {
        size_t capacity = manifest->capacity ? manifest->capacity * 2 : 8;
        ManifestResource *resources = realloc(manifest->resources, capacity * sizeof(*resources));
        if (!resources) return false;
        manifest->resources = resources;
        manifest->capacity = capacity;
    }
    ManifestResource *resource = &manifest->resources[manifest->count++];
    strcpy(resource->id, id);
    strcpy(resource->path, path);
    resource->kind = kind;
    return true;
}

static bool parse_presentation(ThemePresentation *p, const char *key, const char *value) {
    if (strcmp(key, "transition_ms") == 0) return number(value, 0, 500, &p->transition_ms);
    if (strcmp(key, "icon_scale_percent") == 0) return number(value, 100, 150, &p->icon_scale_percent);
    if (strcmp(key, "icon_spacing") == 0) return number(value, 0, 64, &p->icon_spacing);
    if (strcmp(key, "coverflow_neighbors") == 0) return number(value, 0, 2, &p->coverflow_neighbors);
    if (strcmp(key, "coverflow_side_scale_percent") == 0)
        return number(value, 50, 100, &p->coverflow_side_scale_percent);
    if (strcmp(key, "coverflow_spacing") == 0) return number(value, 0, 64, &p->coverflow_spacing);
    if (strcmp(key, "background_crossfade") == 0) return flag(value, &p->background_crossfade);
    if (strcmp(key, "reduced_motion") == 0) return flag(value, &p->reduced_motion);
    if (strcmp(key, "navigation_sounds") == 0) return flag(value, &p->navigation_sounds);
    return true;
}

static bool parse_theme(ThemeResourceInfo *info, unsigned *version,
                        const char *key, const char *value) {
    if (strcmp(key, "manifest_version") == 0) return !*version && number(value, 1, 1, version);
    if (strcmp(key, "id") == 0)
        return ascii_token(value, strlen(value)) && copy_field(info->id, sizeof info->id, value);
    if (strcmp(key, "name") == 0) return copy_field(info->name, sizeof info->name, value);
    if (strcmp(key, "screen_width") == 0) return number(value, 1, 1920, &info->screen_width);
    if (strcmp(key, "screen_height") == 0) return number(value, 1, 1080, &info->screen_height);
    return true;
}

static bool parse_manifest(Manifest *manifest, char *data) {
    const char *section = "";
    unsigned version = 0;
    char *save;
    for (char *line = strtok_r(data, "\n", &save); line; line = strtok_r(NULL, "\n", &save)) {
        line = trim(line);
        if (!*line || *line == '#' || *line == ';') continue;
        size_t length = strlen(line);
        if (line[0] == '[') {
            if (length <= 2 || line[length - 1] != ']') return false;
            line[length - 1] = '\0';
            section = trim(line + 1);
            continue;
        }
        char *equals = strchr(line, '=');
        if (!equals) return false;
        *equals = '\0';
        const char *key = trim(line);
        const char *value = trim(equals + 1);
        if (strcmp(section, "theme") == 0) {
            if (!parse_theme(&manifest->info, &version, key, value)) return false;
        } else if (strcmp(section, "presentation") == 0) {
            if (!parse_presentation(&manifest->presentation, key, value)) return false;
        } else if (strncmp(section, "resource.", 9) == 0 && strcmp(key, "path") == 0) {
            const char *id = section + 9;
            ThemeResourceKind kind = resource_kind(id);
            if (kind != THEME_RESOURCE_COUNT && !add_resource(manifest, id, value, kind))
                return false;
        }
    }
    return version == 1 && (!!manifest->info.screen_width == !!manifest->info.screen_height);
}

ThemeResourceManager *theme_resource_create(const char *root, size_t cache_capacity) {
    if (!root || cache_capacity > THEME_RESOURCE_CACHE_MAX) return NULL;
    int fd = open(root, O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
    if (fd < 0) return NULL;
    ThemeResourceManager *manager = calloc(1, sizeof(*manager));
    if (!manager) { close(fd); return NULL; }
    manager->root_fd = fd;
    manager->cache_capacity = cache_capacity;
    manager->generation = 1;
    manager->manifest.presentation = defaults;
    if (cache_capacity) {
        manager->cache = calloc(cache_capacity, sizeof(*manager->cache));
        if (!manager->cache) {
            theme_resource_destroy(manager);
            return NULL;
        }
    }
    return manager;
}

void theme_resource_destroy(ThemeResourceManager *manager) {
    if (!manager) return;
    close(manager->root_fd);
    free(manager->manifest.resources);
    free(manager->cache);
    free(manager);
}

void theme_resource_invalidate(ThemeResourceManager *manager) {
    if (!manager) return;
    manager->cache_count = 0;
    manager->clock = 0;
    manager->generation++;
}

bool theme_resource_load_manifest(ThemeResourceManager *manager, const char *relative_path) {
    if (!manager) return false;
    size_t size;
    FILE *file = open_regular(manager, relative_path, MANIFEST_MAX_BYTES, &size);
    if (!file) return false;
    char data[MANIFEST_MAX_BYTES + 1];
    bool read_ok = fread(data, 1, size, file) == size &&
                   fgetc(file) == EOF && !ferror(file) && !memchr(data, '\0', size);
    fclose(file);
    if (!read_ok) return false;
    data[size] = '\0';

    Manifest candidate = { .presentation = defaults };
    if (!parse_manifest(&candidate, data)) {
        free(candidate.resources);
        return false;
    }
    free(manager->manifest.resources);
    manager->manifest = candidate;
    theme_resource_invalidate(manager);
    return true;
}

static bool probe_file(FILE *file, ThemeResourceDescriptor *resource) {
    unsigned char header[29];
    size_t size = fread(header, 1, sizeof header, file);
    if (resource->kind == THEME_RESOURCE_NAV_SOUND)
        return size >= 12 && memcmp(header, "RIFF", 4) == 0 && memcmp(header + 8, "WAVE", 4) == 0;

    bool png = size == sizeof header && memcmp(header, "\x89PNG\r\n\x1a\n", 8) == 0 &&
               header[24] == 8 && header[28] == 0;
    bool jpeg = size >= 2 && header[0] == 0xff && header[1] == 0xd8;
    bool bmp = size >= 2 && header[0] == 'B' && header[1] == 'M';
    if (!(png || jpeg || bmp) || fseek(file, 0, SEEK_SET) != 0) return false;
    int w, h, components;
    if (!stbi_info_from_file(file, &w, &h, &components) || w <= 0 || h <= 0) return false;
    int max_w = resource->kind == THEME_RESOURCE_BACKGROUND
                ? THEME_BACKGROUND_MAX_WIDTH : THEME_IMAGE_MAX_EDGE;
    int max_h = resource->kind == THEME_RESOURCE_BACKGROUND
                ? THEME_BACKGROUND_MAX_HEIGHT : THEME_IMAGE_MAX_EDGE;
    if (w > max_w || h > max_h) return false;
    resource->width = (unsigned)w;
    resource->height = (unsigned)h;
    return true;
}

static void cache_descriptor(ThemeResourceManager *manager, const ThemeResourceDescriptor *resource) {
    if (!manager->cache_capacity) return;
    size_t victim = 0;
    if (manager->cache_count < manager->cache_capacity) {
        victim = manager->cache_count++;
    } else {
        for (size_t i = 1; i < manager->cache_count; i++)
            if (manager->cache[i].stamp < manager->cache[victim].stamp) victim = i;
    }
    manager->cache[victim] = (CacheEntry){ *resource, ++manager->clock };
}

bool theme_resource_resolve(ThemeResourceManager *manager, const char *id,
                            ThemeResourceDescriptor *out) {
    if (!manager || !id || !out) return false;
    for (size_t i = 0; i < manager->cache_count; i++) {
        CacheEntry *cached = &manager->cache[i];
        if (strcmp(cached->descriptor.id, id) == 0) {
            cached->stamp = ++manager->clock;
            *out = cached->descriptor;
            return true;
        }
    }

    const ManifestResource *entry = find_resource(&manager->manifest, id);
    if (!entry) return false;
    ThemeResourceDescriptor candidate = { .kind = entry->kind, .generation = manager->generation };
    FILE *file = open_regular(manager, entry->path, THEME_RESOURCE_FILE_MAX, &candidate.file_size);
    if (!file) return false;
    bool supported = probe_file(file, &candidate);
    fclose(file);
    if (!supported) return false;
    strcpy(candidate.id, entry->id);
    strcpy(candidate.path, entry->path);
    cache_descriptor(manager, &candidate);
    *out = candidate;
    return true;
}

unsigned char *theme_resource_read(const ThemeResourceManager *manager,
                                   const ThemeResourceDescriptor *resource) {
    if (!manager || !resource || resource->generation != manager->generation) return NULL;
    const ManifestResource *entry = find_resource(&manager->manifest, resource->id);
    if (!entry || strcmp(entry->path, resource->path) != 0 || entry->kind != resource->kind)
        return NULL;
    size_t size;
    FILE *file = open_regular(manager, entry->path, THEME_RESOURCE_FILE_MAX, &size);
    if (!file) return NULL;
    unsigned char *bytes = size == resource->file_size ? malloc(size) : NULL;
    if (bytes && (fread(bytes, 1, size, file) != size || fgetc(file) != EOF || ferror(file))) {
        free(bytes);
        bytes = NULL;
    }
    fclose(file);
    return bytes;
}

static unsigned sound_le16(const unsigned char *p) {
    return (unsigned)p[0] | (unsigned)p[1] << 8;
}

static uint32_t sound_le32(const unsigned char *p) {
    return sound_le16(p) | (uint32_t)sound_le16(p + 2) << 16;
}

size_t theme_resource_sound_pcm(const ThemeResourceManager *manager,
                                const ThemeResourceDescriptor *resource,
                                int16_t pcm[THEME_SOUND_MAX_FRAMES]) {
    if (!resource || !pcm || resource->kind != THEME_RESOURCE_NAV_SOUND ||
        resource->file_size < 44 || resource->file_size > THEME_SOUND_MAX_BYTES) return 0;
    size_t size = resource->file_size;
    unsigned char *bytes = theme_resource_read(manager, resource);
    if (!bytes) return 0;
    bool format = false;
    const unsigned char *data = NULL;
    size_t frames = 0;
    if (memcmp(bytes, "RIFF", 4) || memcmp(bytes + 8, "WAVE", 4) ||
        sound_le32(bytes + 4) != size - 8) goto done;
    size_t offset = 12;
    while (size - offset >= 8) {
        const unsigned char *chunk = bytes + offset;
        uint32_t length = sound_le32(chunk + 4);
        offset += 8;
        if (length > size - offset || (length & 1u) > size - offset - length) goto done;
        if (!memcmp(chunk, "fmt ", 4)) {
            const unsigned char *fmt = bytes + offset;
            if (format || length < 16 || sound_le16(fmt) != 1 || sound_le16(fmt + 2) != 1 ||
                sound_le32(fmt + 4) != THEME_SOUND_RATE ||
                sound_le32(fmt + 8) != THEME_SOUND_RATE * 2 ||
                sound_le16(fmt + 12) != 2 || sound_le16(fmt + 14) != 16) goto done;
            format = true;
        } else if (!memcmp(chunk, "data", 4)) {
            if (data || length == 0 || (length & 1u) || length / 2 > THEME_SOUND_MAX_FRAMES) goto done;
            data = bytes + offset;
            frames = length / 2;
        }
        offset += length + (length & 1u);
    }
    if (format && data && offset == size) {
        for (size_t i = 0; i < frames; i++) {
            int sample = (int)sound_le16(data + i * 2);
            if (sample >= 32768) sample -= 65536;
            pcm[i] = (int16_t)(sample / 4);
        }
    } else frames = 0;
    free(bytes);
    return frames;

done:
    free(bytes);
    return 0;
}

const ThemeResourceInfo *theme_resource_info(const ThemeResourceManager *manager) {
    return manager ? &manager->manifest.info : NULL;
}

const ThemePresentation *theme_resource_presentation(const ThemeResourceManager *manager) {
    return manager ? &manager->manifest.presentation : NULL;
}

bool theme_resource_supports_screen(const ThemeResourceManager *manager, unsigned w, unsigned h) {
    const ThemeResourceInfo *info = theme_resource_info(manager);
    return info && (!info->screen_width || (info->screen_width == w && info->screen_height == h));
}

size_t theme_resource_cache_count(const ThemeResourceManager *manager) {
    return manager ? manager->cache_count : 0;
}

size_t theme_resource_allocation_bytes(const ThemeResourceManager *manager) {
    if (!manager) return 0;
    return sizeof(*manager) + manager->manifest.capacity * sizeof(*manager->manifest.resources) +
           manager->cache_capacity * sizeof(*manager->cache);
}

unsigned theme_resource_generation(const ThemeResourceManager *manager) {
    return manager ? manager->generation : 0;
}
