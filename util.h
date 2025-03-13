const static size_t lookup[] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4};

typedef struct
{
    uint8_t *data;
    size_t length;
} utf8_char;

// Decodes a UTF-8 string and get the next char
utf8_char decode_utf8(uint8_t *send_buffer, size_t send_pos, size_t send_buffer_len);