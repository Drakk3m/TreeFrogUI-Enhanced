#ifndef FROGUI_FILELIST_H
#define FROGUI_FILELIST_H

#include <stdbool.h>

#define FILELIST_MAX_BYTES (1024u * 1024u)
#define FILELIST_MAX_FIELD 255u

typedef void (*filelist_accept_fn)(void *context, const char *filename,
                                   const char *title);

/* Malformed rows are skipped. False means the complete input was not read or
 * exceeded FILELIST_MAX_BYTES, so the caller should roll back. */
bool filelist_read(const char *path, filelist_accept_fn accept, void *context);

#endif
