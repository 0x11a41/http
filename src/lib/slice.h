#pragma once

#include <stdio.h>
#include <stdbool.h>
#include "../cutils/itypes.h"
#include "../cutils/utils.h"

typedef struct {
    char *s;
    usize len;
} Slice;

bool slice_cmp_str(Slice *slice, const char *str)
{
    for (usize i = 0; i < slice->len && str[i] != '\0'; i++) {
        if (slice->s[i] != str[i]) return false;
    }
    return true;
}

void slice_debug(Slice *slice)
{
    if (!DEBUG) return;
    for (usize i = 0; i < slice->len; i++) {
        printf("%c", slice->s[i]);
    }
}
