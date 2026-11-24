#define _POSIX_C_SOURCE 200809L
#include "filelist.h"

#include <fcntl.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

/* Every byte may be represented as an escaped quote. */
enum { EXTRA_COLUMNS = 16 };
#define ROW_BYTES ((3 + EXTRA_COLUMNS) * (2 * FILELIST_MAX_FIELD + 3) + 5)

typedef struct {
    int columns[EXTRA_COLUMNS];
    size_t count;
    bool has_header;
} FilelistLayout;

static bool utf8_field(const char *field) {
    const unsigned char *p = (const unsigned char *)field;
    while (*p) {
        unsigned lead = *p++;
        if (lead < 0x20 || lead == 0x7f) return false;
        if (lead < 0x80) continue;
        if (lead < 0xc2 || lead > 0xf4) return false;
        unsigned n = lead < 0xe0 ? 1 : lead < 0xf0 ? 2 : 3;
        if ((lead == 0xc2 && *p < 0xa0) ||
            (lead == 0xe0 && *p < 0xa0) || (lead == 0xed && *p >= 0xa0) ||
            (lead == 0xf0 && *p < 0x90) || (lead == 0xf4 && *p >= 0x90)) return false;
        while (n--) {
            if ((*p & 0xc0) != 0x80) return false;
            p++;
        }
    }
    return true;
}

static bool read_field(const char **cursor, char output[FILELIST_MAX_FIELD + 1]) {
    const char *p = *cursor;
    bool quoted = *p == '"';
    size_t size = 0;
    if (quoted) p++;
    while (*p && (quoted || *p != ',')) {
        char value = *p++;
        if (value == '"') {
            if (!quoted) return false;
            if (*p != '"') {
                quoted = false;
                break;
            }
            p++;
        }
        if (size == FILELIST_MAX_FIELD) return false;
        output[size++] = value;
    }
    if (quoted || (*p && *p != ',')) return false;
    output[size] = 0;
    *cursor = p;
    return true;
}

static int detail_column(const char *name) {
    static const char *const names[FILELIST_DETAIL_COUNT] = {
        "Original Title", "Release Date", "Developer", "Publisher",
        "Genre", "Players", "Description"
    };
    for (int i = 0; i < FILELIST_DETAIL_COUNT; i++)
        if (!strcmp(name, names[i])) return i;
    if (!strcmp(name, "ReleaseYear") || !strcmp(name, "Release Year"))
        return FILELIST_RELEASE_DATE;
    return -1;
}

static void read_header(const char *cursor, FilelistLayout *layout) {
    unsigned seen = 0;
    layout->has_header = true;
    while (*cursor && layout->count < EXTRA_COLUMNS) {
        char name[FILELIST_MAX_FIELD + 1];
        cursor++;
        if (!read_field(&cursor, name) || !utf8_field(name)) break;
        int field = detail_column(name);
        if (field >= 0 && (seen & (1u << field))) field = -1;
        if (field >= 0) seen |= 1u << field;
        layout->columns[layout->count++] = field;
    }
}

static void read_row(char *row, size_t bytes, bool first, FilelistLayout *layout,
                     filelist_accept_fn accept, void *context) {
    if (bytes && row[bytes - 1] == '\r') bytes--;
    row[bytes] = 0;
    const char *p = row;
    if (first && bytes >= 3 && !memcmp(row, "\xef\xbb\xbf", 3)) p += 3;
    char filename[FILELIST_MAX_FIELD + 1], title[FILELIST_MAX_FIELD + 1];
    char third[FILELIST_MAX_FIELD + 1];
    if (!read_field(&p, filename) || *p != ',') return;
    p++;
    if (!read_field(&p, title) || *p != ',') return;
    p++;
    if (!read_field(&p, third)) return;
    bool header = first && !strcmp(filename, "Rom.ext") &&
                  !strcmp(title, "Display Title") && !strcmp(third, "ChineseTitle");
    if (header) {
        read_header(p, layout);
        return;
    }
    if (!filename[0] || !strcmp(filename, ".") || !strcmp(filename, "..") ||
        strpbrk(filename, "/\\:") || !utf8_field(filename) ||
        !utf8_field(title) || !utf8_field(third)) return;
    if (*p && !layout->has_header) return;

    char values[FILELIST_DETAIL_COUNT][FILELIST_MAX_FIELD + 1];
    const char *details[FILELIST_DETAIL_COUNT];
    for (int i = 0; i < FILELIST_DETAIL_COUNT; i++) details[i] = "";
    for (size_t col = 0; *p && col < layout->count; col++) {
        int field = layout->columns[col];
        char ignored[FILELIST_MAX_FIELD + 1];
        char *value = field < 0 ? ignored : values[field];
        p++;
        if (!read_field(&p, value)) break;
        if (field >= 0 && utf8_field(value)) details[field] = value;
    }
    accept(context, filename, title, details);
}

bool filelist_read(const char *path, filelist_accept_fn accept, void *context) {
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC | O_NOFOLLOW);
    if (fd < 0) return false;
    struct stat info;
    if (fstat(fd, &info) != 0 || !S_ISREG(info.st_mode) || info.st_size < 0 ||
        info.st_size > FILELIST_MAX_BYTES) {
        close(fd);
        return false;
    }
    FILE *file = fdopen(fd, "rb");
    if (!file) { close(fd); return false; }

    char row[ROW_BYTES];
    size_t total = 0, used = 0;
    bool first = true, invalid = false;
    FilelistLayout layout = {0};
    int ch;
    while ((ch = fgetc(file)) != EOF) {
        if (++total > FILELIST_MAX_BYTES) break;
        if (ch == '\n') {
            if (!invalid) read_row(row, used, first, &layout, accept, context);
            first = false;
            invalid = false;
            used = 0;
        } else if (ch == 0 || used == sizeof row - 1) {
            invalid = true;
        } else if (!invalid) {
            row[used++] = (char)ch;
        }
    }
    bool complete = ch == EOF && !ferror(file) && total == (size_t)info.st_size;
    if (complete && !invalid && used) read_row(row, used, first, &layout, accept, context);
    if (fclose(file) != 0) complete = false;
    return complete;
}
