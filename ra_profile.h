#ifndef FROGUI_RA_PROFILE_H
#define FROGUI_RA_PROFILE_H

#include <stdbool.h>

#define RA_PROFILE_USERNAME_BYTES 320

typedef struct {
    char username[RA_PROFILE_USERNAME_BYTES + 1];
    bool avatar;
} RaProfile;

bool ra_profile_read(const char *directory, RaProfile *out);

#endif
