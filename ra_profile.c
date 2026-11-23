#include "ra_profile.h"
#include <errno.h>
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#ifndef O_NOFOLLOW
#define O_NOFOLLOW 0
#endif

typedef struct {
    const unsigned char *p, *end;
} Json;
static void ws(Json *j) {
    while (j->p < j->end && (*j->p == 32 || *j->p == 9 || *j->p == 10 || *j->p == 13))
        j->p++;
}
static bool take(Json *j, unsigned char c) {
    ws(j);
    if (j->p >= j->end || *j->p != c)
        return false;
    j->p++;
    return true;
}
static int hx(unsigned char c) {
    if (c >= 48 && c <= 57)
        return c - 48;
    if (c >= 65 && c <= 70)
        return c - 55;
    if (c >= 97 && c <= 102)
        return c - 87;
    return -1;
}
static bool u16(Json *j, unsigned *v) {
    if ((size_t)(j->end - j->p) < 4)
        return false;
    unsigned x = 0;
    for (int i = 0; i < 4; i++) {
        int h = hx(j->p[i]);
        if (h < 0)
            return false;
        x = (x << 4) | (unsigned)h;
    }
    j->p += 4;
    *v = x;
    return true;
}
static bool ctl(unsigned c) { return c < 32 || (c >= 127 && c <= 159); }
static bool put(char *out, size_t cap, size_t *n, unsigned c) {
    size_t k = c < 128 ? 1 : c < 2048 ? 2 : c < 65536 ? 3 : 4;
    if (*n + k >= cap)
        return false;
    if (k == 1)
        out[(*n)++] = (char)c;
    else if (k == 2) {
        out[(*n)++] = (char)(192 | (c >> 6));
        out[(*n)++] = (char)(128 | (c & 63));
    } else if (k == 3) {
        out[(*n)++] = (char)(224 | (c >> 12));
        out[(*n)++] = (char)(128 | ((c >> 6) & 63));
        out[(*n)++] = (char)(128 | (c & 63));
    } else {
        out[(*n)++] = (char)(240 | (c >> 18));
        out[(*n)++] = (char)(128 | ((c >> 12) & 63));
        out[(*n)++] = (char)(128 | ((c >> 6) & 63));
        out[(*n)++] = (char)(128 | (c & 63));
    }
    out[*n] = 0;
    return true;
}
static bool raw(Json *j, unsigned *c) {
    const unsigned char *p = j->p;
    if (p >= j->end)
        return false;
    if (*p < 128) {
        *c = *p++;
        j->p = p;
        return true;
    }
    size_t n;
    unsigned x;
    if (*p >= 194 && *p <= 223) {
        n = 2;
        x = *p & 31;
    } else if (*p >= 224 && *p <= 239) {
        n = 3;
        x = *p & 15;
    } else if (*p >= 240 && *p <= 244) {
        n = 4;
        x = *p & 7;
    } else
        return false;
    if ((size_t)(j->end - p) < n)
        return false;
    for (size_t i = 1; i < n; i++) {
        if ((p[i] & 192) != 128)
            return false;
        x = (x << 6) | (p[i] & 63);
    }
    if ((n == 3 && x < 2048) || (n == 4 && x < 65536) || x > 1114111 || (x >= 55296 && x <= 57343))
        return false;
    j->p = p + n;
    *c = x;
    return true;
}
static bool str(Json *j, char *out, size_t cap, size_t *n, size_t *points, bool reject) {
    ws(j);
    if (j->p >= j->end || *j->p++ != 34)
        return false;
    *n = 0;
    *points = 0;
    for (;;) {
        if (j->p >= j->end)
            return false;
        unsigned c = *j->p;
        if (c == 34) {
            j->p++;
            out[*n] = 0;
            return true;
        }
        if (c < 32)
            return false;
        if (c == 92) {
            j->p++;
            if (j->p >= j->end)
                return false;
            c = *j->p++;
            if (c == 34 || c == 92 || c == 47) {
            } else if (c == 98)
                c = 8;
            else if (c == 102)
                c = 12;
            else if (c == 110)
                c = 10;
            else if (c == 114)
                c = 13;
            else if (c == 116)
                c = 9;
            else if (c == 117) {
                unsigned lo;
                if (!u16(j, &c))
                    return false;
                if (c >= 55296 && c <= 56319) {
                    if ((size_t)(j->end - j->p) < 6 || j->p[0] != 92 || j->p[1] != 117)
                        return false;
                    j->p += 2;
                    if (!u16(j, &lo) || lo < 56320 || lo > 57343)
                        return false;
                    c = 65536 + ((c - 55296) << 10) + (lo - 56320);
                } else if (c >= 56320)
                    return false;
            } else
                return false;
        } else if (!raw(j, &c))
            return false;
        if (ctl(c))
            return false;
        if (!put(out, cap, n, c))
            return false;
        (*points)++;
        if (reject && *points > 80)
            return false;
    }
}
static bool lit(Json *j, const char *s) {
    size_t n = strlen(s);
    if ((size_t)(j->end - j->p) < n || memcmp(j->p, s, n))
        return false;
    j->p += n;
    return true;
}
bool ra_profile_read(const char *directory, RaProfile *out) {
    if (!directory || !out)
        return false;
    char path[512];
    int n = snprintf(path, sizeof path, "%s/profile.json", directory);
    if (n < 0 || (size_t)n >= sizeof path)
        return false;
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_NOFOLLOW);
    if (fd < 0)
        return false;
    struct stat st;
    bool ok = fstat(fd, &st) == 0 && S_ISREG(st.st_mode) && st.st_size >= 0 &&
              (uintmax_t)st.st_size <= 4096;
    if (!ok) {
        close(fd);
        return false;
    }
    unsigned char data[4096];
    size_t used = 0;
    while (used < (size_t)st.st_size) {
        ssize_t got = read(fd, data + used, (size_t)st.st_size - used);
        if (got < 0) {
            if (errno == EINTR)
                continue;
            ok = false;
            break;
        }
        if (!got) {
            ok = false;
            break;
        }
        used += (size_t)got;
    }
    close(fd);
    if (!ok)
        return false;
    Json j = {data, data + used};
    char key[64], value[RA_PROFILE_USERNAME_BYTES + 1];
    size_t nb, np;
    bool hu = false, ha = false;
    RaProfile t = {{0}, false};
    if (!take(&j, 123))
        return false;
    for (;;) {
        if (!str(&j, key, sizeof key, &nb, &np, false) || !take(&j, 58))
            return false;
        if (!strcmp(key, "username")) {
            if (hu || !str(&j, t.username, sizeof t.username, &nb, &np, true) || !nb ||
                t.username[0] == 32 || t.username[nb - 1] == 32 || t.username[0] == 9 ||
                t.username[nb - 1] == 9 || t.username[0] == 10 || t.username[nb - 1] == 10 ||
                t.username[0] == 13 || t.username[nb - 1] == 13)
                return false;
            hu = true;
        } else if (!strcmp(key, "avatar")) {
            if (ha)
                return false;
            ws(&j);
            if (j.p < j.end && *j.p == 110) {
                if (!lit(&j, "null"))
                    return false;
                t.avatar = false;
            } else {
                if (!str(&j, value, sizeof value, &nb, &np, false) || strcmp(value, "avatar.png"))
                    return false;
                t.avatar = true;
            }
            ha = true;
        } else
            return false;
        ws(&j);
        if (j.p >= j.end)
            return false;
        if (*j.p == 125) {
            j.p++;
            break;
        }
        if (*j.p != 44)
            return false;
        j.p++;
    }
    ws(&j);
    if (j.p != j.end || !hu || !ha)
        return false;
    *out = t;
    return true;
}
