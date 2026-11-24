#ifndef FROGUI_FILELIST_H
#define FROGUI_FILELIST_H

#include <stdbool.h>

#define FILELIST_MAX_BYTES (1024u * 1024u)
#define FILELIST_MAX_FIELD 255u

enum FilelistDetail {
    FILELIST_ORIGINAL_TITLE, FILELIST_RELEASE_DATE, FILELIST_DEVELOPER,
    FILELIST_PUBLISHER, FILELIST_GENRE, FILELIST_PLAYERS, FILELIST_DESCRIPTION,
    FILELIST_DETAIL_COUNT
};

typedef void (*filelist_accept_fn)(void *context, const char *filename,
                                   const char *title, const char *const *details);

/* Malformed rows are skipped. False means the complete input was not read or
 * exceeded FILELIST_MAX_BYTES, so the caller should roll back. */
bool filelist_read(const char *path, filelist_accept_fn accept, void *context);

#endif
