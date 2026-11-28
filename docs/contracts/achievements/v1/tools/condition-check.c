/* Host-only syntax gate against the pinned rcheevos source. */
#include "rc_runtime_types.h"
#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) return 2;
    int bytes = rc_trigger_size(argv[1]);
    if (bytes < 1 || bytes > 4096) return 1;
    void *buffer = malloc((size_t)bytes);
    if (!buffer) return 2;
    int valid = rc_parse_trigger(buffer, argv[1], NULL, 0) != NULL;
    free(buffer);
    if (!valid) return 1;
    printf("%d\n", bytes);
    return 0;
}
