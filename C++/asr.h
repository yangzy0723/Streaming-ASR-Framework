#include <vector>
#include <cstdint>

#define UNUSED(x) (void)(x)

#define CONTEXT_SIZE 4096
#define SEND_DELAY 300000
#define PORT 8080

enum SendStatus {
    SEND_IN_PROGRESS,
    WAITING,
    SEND_COMPLETED
};

enum Language {
    ZH_CN,
    EN_US
};

// 中文逗号的字节码
static std::vector<uint8_t> Chinese_comma = { 
    static_cast<uint8_t>(0xEF), 
    static_cast<uint8_t>(0xBC), 
    static_cast<uint8_t>(0x8C)
};
static std::vector<uint8_t> English_comma = {static_cast<uint8_t>(',')}; 

// 中文句号的字节码
static std::vector<uint8_t> Chinese_period = { 
    static_cast<uint8_t>(0xE3), 
    static_cast<uint8_t>(0x80), 
    static_cast<uint8_t>(0x82)
};
static std::vector<uint8_t> English_period = {static_cast<uint8_t>('.')};

const static size_t lookup[] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4};

typedef struct
{
    uint8_t *data;
    size_t length;
} utf8_char;

typedef utf8_char Chinese_word;
typedef char English_word_head;

// 解析utf-8字符
static utf8_char decode_utf8();