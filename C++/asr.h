#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

#define UNUSED(x) (void) (x)

#define CONTEXT_SIZE 4096

enum SendStatus { SEND_IN_PROGRESS, WAITING, SEND_COMPLETED };

enum Language { ZH_CN, EN_US };

typedef struct {
    uint8_t * data;
    size_t    length;
} utf8_char;

typedef utf8_char Chinese_word;
typedef char      English_word_head;

// 解析utf-8字符
static utf8_char decode_utf8();

