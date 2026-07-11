#pragma once

#include <stdio.h>
#include <stdbool.h>
#include <string.h>
#include "../cutils/itypes.h"
#include "../cutils/utils.h"

typedef struct {
    char *s;
    usize len;
} Slice;

bool slice_cmp_str(Slice *slice, const char *str)
{
    usize n = strlen(str);
    if (n != slice->len) return false;

    for (usize i = 0; i < n; i++) {
        if (slice->s[i] != str[i]) return false;
    }
    return true;
}

bool slice_begins_with_str(Slice *slice, const char *str)
{
    usize n = strlen(str);
    if (n > slice->len) return false;

    for (usize i = 0; i < n; i++) {
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
