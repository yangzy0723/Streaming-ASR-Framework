#include "asr.h"

#include <assert.h>
#include <libwebsockets.h>

#include <iostream>
#include <string>
#include <thread>

Language LANG = ZH_CN; // 语言设置（"zh" 或 "en"）
size_t SEND_DELAY = 300;
size_t PORT = 8080;

static FILE *log_file = NULL;

// 中文逗号的字节码
static std::vector<uint8_t> Chinese_comma = {static_cast<uint8_t>(0xEF), static_cast<uint8_t>(0xBC),
                                             static_cast<uint8_t>(0x8C)};
static std::vector<uint8_t> English_comma = {static_cast<uint8_t>(',')};

// 中文句号的字节码
static std::vector<uint8_t> Chinese_period = {static_cast<uint8_t>(0xE3), static_cast<uint8_t>(0x80),
                                              static_cast<uint8_t>(0x82)};
static std::vector<uint8_t> English_period = {static_cast<uint8_t>('.')};

const static size_t lookup[] = {1, 1, 1, 1, 1, 1, 1, 1, 0, 0, 0, 0, 2, 2, 3, 4};

static std::vector<uint8_t> record_buffer; // 存储所有接收到的文本
static std::vector<uint8_t> send_buffer;   // 用于处理和提取单词的缓冲区

static size_t record_buffer_len = 0; // record_buffer 的长度
static size_t send_buffer_len = 0;   // send_buffer 的长度
static size_t send_pos = 0;          // 已处理的发送缓冲区位置

static size_t sentence_len = (size_t)-1; // 上次接收到的句子长度（-1 表示新句子）

static SendStatus sendStatus = WAITING; // ASR序列投送状态
bool first_word_flag = true;

static inline void add_word(std::string word)
{
    printf("%s\n", word.c_str());
}

static inline void send_word(std::string word)
{
    if (LANG == EN_US && !first_word_flag && word != "," && word != "." && word != "<eof>")
    {
        word = " " + word;
    }

    first_word_flag = false;

    add_word(word);

    if (PERIOD(word))
    {
        std::string eos = "<eos>";
        add_word(eos);
    }
}

static inline void my_log_emit_function(int level, const char *line)
{
    if (log_file)
    {
        fprintf(log_file, "%s", line);
        fflush(log_file);
    }
}

static inline utf8_char decode_utf8()
{
    uint8_t first_byte = send_buffer[send_pos];
    uint8_t highbits = first_byte >> 4;
    uint8_t byte_len = lookup[highbits];
    if (!byte_len || send_pos + byte_len > send_buffer_len)
    {
        assert(0);
    }
    utf8_char next_char;
    next_char.data = (uint8_t *)malloc(byte_len);
    for (int i = 0; i < byte_len; i++)
    {
        next_char.data[i] = send_buffer[send_pos + i];
    }
    next_char.length = byte_len;
    return next_char;
}

// 处理单词提取的函数（逐词处理，确保完整单词）
void *get_word_thread_func()
{
    while (sendStatus != SEND_COMPLETED)
    {
        std::this_thread::sleep_for(std::chrono::milliseconds(SEND_DELAY));
        if (send_pos < send_buffer_len)
        {
            switch (LANG)
            {
            case ZH_CN:
            {
                Chinese_word next_word = decode_utf8();
                if (*(next_word.data) == '<' && send_pos + 4 < send_buffer_len)
                {
                    std::string extracted(send_buffer.begin() + send_pos, send_buffer.begin() + send_pos + 5);
                    if (extracted == "<eof>")
                    {
                        next_word.length = 5;
                        sendStatus = SEND_COMPLETED;
                    }
                }
                printf("word: %s; byte_len: %ld\n", next_word.data, next_word.length);
                std::string word(send_buffer.begin() + send_pos,
                                 send_buffer.begin() + send_pos + next_word.length);
                send_word(word);
                send_pos += next_word.length;
                break;
            }
            case EN_US:
            {
                while (send_pos < send_buffer_len && (char)send_buffer[send_pos] == ' ')
                {
                    send_pos++;
                }
                English_word_head next_word_head = (char)send_buffer[send_pos];
                int word_len = 1;
                switch (next_word_head)
                {
                case ',':
                    send_word(",");
                    break;
                case '.':
                    send_word(".");
                    break;
                case '<':
                    if (send_pos + 4 < send_buffer_len)
                    {
                        std::string extracted(send_buffer.begin() + send_pos,
                                              send_buffer.begin() + send_pos + 5);
                        if (extracted == "<eof>")
                        {
                            word_len = 5;
                            send_word("<eof>");
                            sendStatus = SEND_COMPLETED;
                            break;
                        }
                    }
                default:
                    while (send_pos + word_len < send_buffer_len &&
                           (char)send_buffer[send_pos + word_len] != ' ' &&
                           (char)send_buffer[send_pos + word_len] != ',' &&
                           (char)send_buffer[send_pos + word_len] != '.')
                    {
                        word_len++;
                    }
                    if (send_pos + word_len == send_buffer_len)
                    {
                        word_len = 0;
                    }
                    if (word_len)
                    {
                        std::string extracted(send_buffer.begin() + send_pos,
                                              send_buffer.begin() + send_pos + word_len);
                        send_word(extracted);
                    }
                }
                send_pos += word_len;
                while (send_pos + word_len < send_buffer_len &&
                       (char)send_buffer[send_pos + word_len] == ' ')
                {
                    send_pos++;
                }
            }
            default:;
            }
        }
    }
    return NULL;
}

// WebSocket 回调函数
static int callback_websocket(struct lws *wsi, enum lws_callback_reasons reason, void *user, void *in, size_t len)
{
    switch (reason)
    {
    case LWS_CALLBACK_ESTABLISHED:
        sendStatus = SEND_IN_PROGRESS;
        // std::cout << std::endl << "客户端已连接" << std::endl;
        break;

    case LWS_CALLBACK_RECEIVE:
    {
        if (len > 0)
        {
            // 将接受到的字节存入 vector<uint8_t> received
            std::vector<uint8_t> received(static_cast<uint8_t *>(in), static_cast<uint8_t *>(in) + len);
            // std::string extracted(received.begin(), received.end());
            // printf("%s\n", extracted.c_str());
            // for (unsigned char c : extracted) {
            //     printf("%02X ", c);
            // }
            // printf("\n");
            // 更新缓冲区
            if (sentence_len == (size_t)-1)
            {
                // 新句子：直接追加
                send_buffer.insert(send_buffer.end(), received.begin(), received.end());
                record_buffer.insert(record_buffer.end(), received.begin(), received.end());

                send_buffer_len += len;
                record_buffer_len += len;
            }
            else
            {
                if (len < sentence_len)
                {
                    // 句子延续
                    std::vector<uint8_t> comma = LANG == ZH_CN ? Chinese_comma : English_comma;
                    // 添加逗号
                    send_buffer.insert(send_buffer.end(), comma.begin(), comma.end());
                    record_buffer.insert(record_buffer.end(), comma.begin(), comma.end());
                    // 再继续追加内容
                    send_buffer.insert(send_buffer.end(), received.begin(), received.end());
                    record_buffer.insert(record_buffer.end(), received.begin(), received.end());

                    send_buffer_len += comma.size() + len;
                    record_buffer_len += comma.size() + len;
                }
                else
                {
                    // 句子更新：用新句子直接覆盖旧句子
                    // 此时sentence_len <= len
                    size_t append_len = len - sentence_len;
                    // 覆盖
                    size_t start_pos = record_buffer.size() - sentence_len;
                    if (record_buffer.size() < start_pos + len)
                    {
                        record_buffer.resize(start_pos + len);
                    }
                    std::copy(received.begin(), received.end(), record_buffer.begin() + start_pos);

                    start_pos = send_buffer.size() - sentence_len;
                    if (send_buffer.size() < start_pos + len)
                    {
                        send_buffer.resize(start_pos + len);
                    }
                    std::copy(received.begin(), received.end(), send_buffer.begin() + start_pos);

                    // 更新长度
                    send_buffer_len += append_len;
                    record_buffer_len += append_len;
                }
            }
        }
        sentence_len = len;
    }
    break;

    case LWS_CALLBACK_CLOSED:
    {
        // 连接中断时添加句号
        std::vector<uint8_t> period = LANG == ZH_CN ? Chinese_period : English_period;
        send_buffer.insert(send_buffer.end(), period.begin(), period.end());
        send_buffer_len += period.size();

        // 重置缓冲区
        record_buffer.clear();
        record_buffer_len = 0;
        sentence_len = -1;

        sendStatus = WAITING;
        // std::cout << std::endl << "客户端连接中断" << std::endl;
    }
    break;
    default:
        break;
    }

    return 0;
}

// 协议定义
static struct lws_protocols protocols[] = {
    {"websocket", callback_websocket, 0, 0}, // WebSocket 协议
    {NULL, NULL, 0, 0}                       // 结束标志
};

// int asr() {
//     // 处理 libwebsockets 日志
//     log_file = fopen("websockets.log", "a");
//     if (!log_file)
//     {
//         std::cerr << "无法打开日志文件" << std::endl;
//         return EXIT_FAILURE;
//     }
//     lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, my_log_emit_function);

//     // 配置 libwebsockets 上下文
//     struct lws_context_creation_info info{};

//     memset(&info, 0, sizeof(info));
//     info.port = PORT;           // 监听端口
//     info.protocols = protocols; // 协议列表
//     info.gid = -1;              // 默认组 ID
//     info.uid = -1;              // 默认用户 ID

//     // 创建 libwebsockets 上下文
//     struct lws_context *context = lws_create_context(&info);
//     if (!context)
//     {
//         std::cerr << "libwebsockets上下文创建失败" << std::endl;
//         return 1;
//     }

//     // 创建 word 发送线程
//     std::thread split_word_thread(get_word_thread_func);

//     std::cout << "WebSocket服务器启动，监听端口: 8080" << std::endl;

//     // 主事件循环
//     while (sendStatus != SEND_COMPLETED)
//     {
//         lws_service(context, 50); // 50ms 超时
//     }

//     if (split_word_thread.joinable())
//     {
//         split_word_thread.join();
//     }
//     lws_context_destroy(context);
//     return 0;
// }

int main(int argc, char *argv[])
{
    for (int i = 1; i < argc; i++)
    {
        std::string arg = argv[i];

        if (arg == "--lang" && i + 1 < argc)
        {
            std::string langArg = argv[++i];
            if (langArg == "zh")
            {
                LANG = ZH_CN;
            }
            else if (langArg == "en")
            {
                LANG = EN_US;
            }
        }
        else if (arg == "--send_delay" && i + 1 < argc)
        {
            SEND_DELAY = std::stoul(argv[++i]);
        }
        else if (arg == "--port" && i + 1 < argc)
        {
            PORT = std::stoul(argv[++i]);
        }
    }

    // 处理 libwebsockets 日志
    log_file = fopen("websockets.log", "a");
    if (!log_file)
    {
        std::cerr << "无法打开日志文件" << std::endl;
        return EXIT_FAILURE;
    }
    lws_set_log_level(LLL_ERR | LLL_WARN | LLL_NOTICE | LLL_INFO | LLL_DEBUG, my_log_emit_function);

    // 配置 libwebsockets 上下文
    struct lws_context_creation_info info{};

    memset(&info, 0, sizeof(info));
    info.port = PORT;           // 监听端口
    info.protocols = protocols; // 协议列表
    info.gid = -1;              // 默认组 ID
    info.uid = -1;              // 默认用户 ID

    // 创建 libwebsockets 上下文
    struct lws_context *context = lws_create_context(&info);
    if (!context)
    {
        std::cerr << "libwebsockets上下文创建失败" << std::endl;
        return 1;
    }

    // 创建 word 发送线程
    std::thread split_word_thread(get_word_thread_func);

    std::cout << "WebSocket服务器启动，监听端口: 8080" << std::endl;

    // 主事件循环
    while (sendStatus != SEND_COMPLETED)
    {
        lws_service(context, 50); // 50ms 超时
    }

    if (split_word_thread.joinable())
    {
        split_word_thread.join();
    }
    lws_context_destroy(context);
    return 0;
}