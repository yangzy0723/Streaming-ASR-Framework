#include <stdint.h>
#include <stddef.h>
#include <assert.h>
#include <ctype.h>
#include <stdio.h>

#include "util.h"

utf8_char decode_utf8(uint8_t *send_buffer, size_t send_pos, size_t send_buffer_len)
{
    uint8_t first_byte = (uint8_t)(*(send_buffer + send_pos));
    uint8_t highbits = first_byte >> 4;
    uint8_t byte_len = lookup[highbits];
    if (!byte_len || send_pos + byte_len > send_buffer_len)
        assert(0);
    utf8_char next_char;
    next_char.data = send_buffer + send_pos;
    next_char.length = byte_len;
    return next_char;
}

void trim(void **str, size_t* length) {
    char *begin = (char*)*str;
    char *end = begin + *length - 1;
    while (begin <= end && isspace(*begin)) {
        begin++;
        (*length)--;
    }
    while (end >= begin && isspace(*end)) {
        end--;
        (*length)--;
    }
    *str = begin;
}