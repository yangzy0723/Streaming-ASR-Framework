#define UNUSED(x) (void)(x)

#define WORD_LENGTH 128

static const char Chinese_comma[] = "，";
static const char English_comma[] = ",";
static char* comma __attribute__((unused));

static const char Chinese_period[] = "。";
static const char English_period[] = ". ";
static char* period __attribute__((unused));

const static size_t lookup[] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4};

typedef struct
{
    uint8_t *data;
    size_t length;
} utf8_char;

typedef utf8_char chinese_word;
typedef char* english_word_ptr;

// Decodes a UTF-8 string and get the next char
utf8_char decode_utf8(uint8_t *send_buffer, size_t send_pos, size_t send_buffer_len);

// Trim leading and trailing spaces
void trim(void **str, size_t* length);