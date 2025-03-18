#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <libwebsockets.h>
#include <pthread.h>
#include <unistd.h>

#include "util.h"

// 这个谨慎改，和客户端可能对不上
static size_t PORT = 8080;
// 默认字符发送延迟：0.3秒（单位：微秒）
static size_t SEND_DELAY = 300000;
// 默认解析语言：中文（zh）
static char LANG[] = "zh";

#define BUFFER_SIZE 4096

static uint8_t record_buffer[BUFFER_SIZE] = {0};
static size_t record_buffer_len = 0;

static uint8_t send_buffer[BUFFER_SIZE] = {0};
static size_t send_pos = 0;
static size_t send_buffer_len = 0;

static size_t sentence_len = -1;

static pthread_mutex_t buffer_mutex = PTHREAD_MUTEX_INITIALIZER;

void send_token(uint8_t *addr, int length)
{
    printf("length: %d, word: %.*s\n", length, length, addr);
    // for (int i = 0; i < length; i++)
    //     printf("%x ", *(addr + i));
    // printf("\n");
    fflush(stdout);
}

void *get_word_thread_func(void *arg)
{
    while (1)
    {
        pthread_mutex_lock(&buffer_mutex);
        if (send_pos < send_buffer_len)
        {
            if (strcmp(LANG, "zh") == 0)
            {
                chinese_word next_word = decode_utf8(send_buffer, send_pos, send_buffer_len);

                // deal with "<eos>"
                if (*(char *)next_word.data == '<' && send_pos + 4 < send_buffer_len)
                {
                    if (strncmp((char *)next_word.data, "<eos>", 5) == 0)
                        next_word.length = 5;
                }
                send_pos += next_word.length;
                send_token(next_word.data, (int)next_word.length);
            }
            else if (strcmp(LANG, "en") == 0)
            {
                english_word_ptr next_word = (char *)send_buffer + send_pos;
                int word_len = 1;
                switch (*next_word)
                {
                case ',':
                    send_token((uint8_t *)next_word, word_len);
                    send_pos += word_len;
                    break;
                case '<':
                    if (send_pos + 4 < send_buffer_len && strncmp(next_word, "<eos>", 5) == 0)
                    {
                        word_len = 5;
                        send_token((uint8_t *)next_word, word_len);
                        send_pos += word_len;
                        break;
                    }
                default:
                    while (send_pos + word_len < send_buffer_len && (char)send_buffer[send_pos + word_len] != ' ')
                        word_len++;
                    send_pos += word_len;
                    send_token((uint8_t *)next_word, word_len);
                }
            }
        }
        pthread_mutex_unlock(&buffer_mutex);
        usleep(SEND_DELAY);
    }
    return NULL;
}

static int callback_http(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    return 0;
}

static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch (reason)
    {
    case LWS_CALLBACK_ESTABLISHED:
        printf("客户端已连接\n");
        break;

    case LWS_CALLBACK_RECEIVE:
        if (len > 0)
        {
            pthread_mutex_lock(&buffer_mutex);
            if (sentence_len == -1)
            {
                memcpy(record_buffer + record_buffer_len, in, len);
                record_buffer_len += len;

                memcpy(send_buffer + send_buffer_len, in, len);
                send_buffer_len += len;
            }
            else
            {
                if (len < sentence_len)
                {
                    memcpy(record_buffer + record_buffer_len, comma, strlen(comma));
                    record_buffer_len += strlen(comma);
                    memcpy(record_buffer + record_buffer_len, in, len);
                    record_buffer_len += len;

                    memcpy(send_buffer + send_buffer_len, comma, strlen(comma));
                    send_buffer_len += strlen(comma);
                    memcpy(send_buffer + send_buffer_len, in, len);
                    send_buffer_len += len;
                }
                else
                {
                    size_t append_len = len - sentence_len;
                    if (append_len)
                    {
                        memcpy(record_buffer + record_buffer_len - sentence_len, in, len);
                        record_buffer_len += append_len;

                        memcpy(send_buffer + send_buffer_len - sentence_len, in, len);
                        send_buffer_len += append_len;
                    }
                }
            }
            record_buffer[record_buffer_len] = '\0';
            sentence_len = len;
        }
        pthread_mutex_unlock(&buffer_mutex);

        break;

    case LWS_CALLBACK_CLOSED:
        pthread_mutex_lock(&buffer_mutex);

        memcpy(send_buffer + send_buffer_len, period, strlen(period));
        send_buffer_len += strlen(period);
        record_buffer_len = 0;
        sentence_len = -1;

        pthread_mutex_unlock(&buffer_mutex);
        printf("客户端断开连接\n");
        break;

    default:
        break;
    }
    return 0;
}

static struct lws_protocols protocols[] = {
    {"websocket", callback_websocket, 0, 0},
    {"http-only", callback_http, 0, 0},
    {NULL, NULL, 0, 0}};

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        if (strcmp(argv[i], "--port") == 0 && i + 1 < argc)
        {
            PORT = strtoul(argv[i + 1], NULL, 10);
            i++;
        }
        else if (strcmp(argv[i], "--send_delay") == 0 && i + 1 < argc)
        {
            float delay_in_seconds = strtof(argv[i + 1], NULL);
            SEND_DELAY = (size_t)(delay_in_seconds * 1000000);
            i++;
        }
        else if (strcmp(argv[i], "--lang") == 0 && i + 1 < argc)
        {
            memcpy(LANG, argv[i + 1], 2);
            i++;
        }
    }

    if (strcmp(LANG, "zh") == 0)
    {
        comma = malloc(strlen(Chinese_comma));
        memcpy(comma, Chinese_comma, strlen(Chinese_comma));
        period = malloc(strlen(Chinese_period));
        memcpy(period, Chinese_period, strlen(Chinese_period));
    }
    else if (strcmp(LANG, "en") == 0)
    {
        comma = malloc(strlen(English_comma));
        memcpy(comma, English_comma, strlen(English_comma));
        period = malloc(strlen(English_period));
        memcpy(period, English_period, strlen(English_period));
    }

    struct lws_context_creation_info info;
    struct lws_context *context;
    pthread_t print_thread;

    memset(&info, 0, sizeof(info));
    info.port = PORT;
    info.protocols = protocols;
    info.gid = -1;
    info.uid = -1;

    context = lws_create_context(&info);
    if (!context)
    {
        fprintf(stderr, "创建WebSocket上下文失败\n");
        return -1;
    }

    printf("服务器启动，监听端口 %ld...\n", PORT);

    if (pthread_create(&print_thread, NULL, get_word_thread_func, NULL) != 0)
    {
        fprintf(stderr, "打印线程创建失败\n");
        return -1;
    }

    while (1)
    {
        lws_service(context, 50);
    }

    lws_context_destroy(context);
    pthread_cancel(print_thread);
    pthread_join(print_thread, NULL);
    return 0;
}
