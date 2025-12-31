#define _GNU_SOURCE
#include <cJSON.h>
#include <netdb.h>
#include <netinet/in.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include "agora_server.h"
#include "app_config.h"

#define HOST "api.agora.io"
#define PORT 443
#define BUFFER_SIZE 8192
#define BASE_PATH "/cn/api/conversational-ai-agent/v2/projects"
#define BASE_PATH_OVERSEAS "/api/conversational-ai-agent/v2/projects"

// 配置结构体
typedef struct {
  char auth_header[128];
  char host[32];
  char app_id[64];
  char base_path[128];
  int port;
} Config;

// 请求结构体
typedef struct {
  const char *method;
  const char *path;
  const char *body;
  const char *content_type;
} HttpRequest;

// 响应结构体
typedef struct {
  int status_code;
  char *body;
  size_t body_length;
} HttpResponse;

// 全局配置
static Config config = {0};

typedef struct {
    const char *code;           // 如 "zh-CN", "en-US"
    const char *asr_language;   // ASR 识别语言
    const char *greeting;       // 问候语
    const char *failure;        // 无法回答时的回复
    const char *agent_id;       // 对应的大模型 agentId
} LanguageConfig;

static const LanguageConfig languages[] = {
    {
        .code          = "en-US",
        .asr_language  = "en-US",
        .greeting      = "Hey, I'm right here. How was your day?",
        .failure       = "Sorry, I can't answer this question.",
        .agent_id      = "917a2c89-cbb8-4bc0-823c-d7afcafb86a5"
    },
    {
        .code          = "zh-CN",
        .asr_language  = "zh-CN",
        .greeting      = "嘿，你今天过得怎么样？",
        .failure       = "抱歉，我无法回答这个问题。",
        .agent_id      = "4c5e5bbe-1b94-42ed-92d2-bbb71e86e2c3"
    },
    {
        .code          = "ja-JP",
        .asr_language  = "ja-JP",
        .greeting      = "こんにちは、今日はどんな一日でしたか？",
        .failure       = "申し訳ありませんが、この質問には答えられません。",
        // .agent_id      = ""
    },
    {
        .code          = "en-IN",
        .asr_language  = "en-IN",
        .greeting      = "Hello, how has your day been?",
        .failure       = "Sorry, I am unable to answer that question.",
        // .agent_id      = ""
    },
};

// 根据语言代码查找配置（返回 NULL 表示不支持）
static const LanguageConfig *get_language_config(const char *lang_code) {
    if (!lang_code) return &languages[0];  // 默认英文

    for (size_t i = 0; i < sizeof(languages) / sizeof(languages[0]); i++) {
        if (strcmp(languages[i].code, lang_code) == 0) {
            return &languages[i];
        }
    }
    return &languages[0];  // 不支持时 fallback 到英文
}

// 动态生成 JOIN 请求的 JSON body
static char *build_join_json_body(const char *channel,
                                  const char *token,
                                  uint32_t uid,
                                  const char *language_code)
{
    const LanguageConfig *lang = get_language_config(language_code);

    char *json_body = NULL;
    int ret = asprintf(&json_body,
        "{"
        "\"name\":\"%s\","
        "\"properties\":{"
        "\"channel\":\"%s\","
        "\"token\":\"%s\","
        "\"agent_rtc_uid\":\"%u\","
        "\"remote_rtc_uids\":[\"*\"],"
        "\"asr\": {"
          "\"language\": \"%s\""
        "},"
        "\"advanced_features\": {\"enable_aivad\": false},"
        "\"vad\": {"
          "\"interrupt_duration_ms\": 160,"
          "\"prefix_padding_ms\": 300,"
          "\"silence_duration_ms\": 480,"
          "\"threshold\": 0.5"
        "},"
        "\"llm\":{"
          "\"url\":\"https://workflow.amazingchat.ai/api/audio/chat\","
          "\"api_key\":\"eyJhbGciOiJIUzI1NiJ9.eyJzdWIiOiJhNzc3ZTZhNi03ZDYyLTRiOTYtODU5MC0xMWJjYjdlYjkxZWQiLCJuYW1lIjoidGhpcmQgY2FsbCIsInR5cGUiOiJhcGlfa2V5IiwiaWF0IjoxNzY2MDMwNzQxLCJqdGkiOiJlZjQ4YjE5ZC1kYTQ1LTQzMDEtODNiYS03NWQxYTMyZjlmZjYifQ.yWyTMYF_UJy7H6iPmYQDZdzFAhX0Cvly7HXWW5yVP8w\","
          "\"system_messages\":[{\"role\": \"system\", \"content\": \"\"}],"
          "\"greeting_message\":\"%s\","
          "\"failure_message\":\"%s\","
          "\"max_history\":10,"
          "\"input_modalities\":[\"text\"],"
          "\"output_modalities\":[\"text\",\"audio\"],"
          "\"params\":{"
            "\"extendData\":{},"
            "\"agentId\": \"%s\","
            "\"sampleRate\":32000,"
            "\"response_mode\":\"streaming\","
            "\"conversation_id\":\"skg\","
            "\"user\":\"skg_user1\""
          "}"
        "},"
        "\"tts\": {"
            "\"vendor\": \"null\","
            "\"enable\": false,"
            "\"params\":{"
               "\"sample_rate\":32000"
            "}"
        "}"
      "}"
    "}",
        channel,                    // name
        channel,                    // channel
        token,                      // token
        uid,                        // agent_rtc_uid
        lang->asr_language,         // asr_language
        lang->greeting,             // greeting_message
        lang->failure,              // failure_message
        lang->agent_id              // agentId
    );

    if (ret < 0) return NULL;
    return json_body;
}

const char *change_param_json_data =
    "{"
    "\"properties\": {"
    "\"token\": \" %s"
    "\","
    "\"llm\": {"
    "\"system_messages\": ["
    "{\"role\": \"system\",\"content\": \"You are a helpful assistant. xxx\"},"
    "{\"role\": \"system\",\"content\": \"Previously, user has talked about "
    "their favorite hobbies with some key topics: xxx\"}"
    "],"
    "\"params\": {"
    "\"model\": \"abab6.5s-chat\","
    "\"max_token\": 1024"
    "}"
    "}"
    "}"
    "}";

const char *speak_message_json_data =
    "{"
    "\"text\": \"你好，这是一条自定义播报消息。\","
    "\"priority\": \"INTERRUPT\","
    "\"interruptible\": true"
    "}";

const char *interrupt_agent_json_data = "{}";

/* 
 * 外部初始化配置
 * @param app_id          你的项目 App ID（必须由调用方提供）
 * @param auth_header     Basic Auth 头部值（username:password 经过 base64 编码）
 * @param is_overseas     是否海外版本（true 表示海外，base_path 不带 /cn）
 */
void agora_config_init(const char *app_id, const char *auth_header, bool is_overseas) {
    if (!app_id || !auth_header) {
        fprintf(stderr, "agora_config_init: app_id or auth_header is NULL\n");
        return;
    }

    strncpy(config.app_id, app_id, sizeof(config.app_id) - 1);
    strncpy(config.auth_header, auth_header, sizeof(config.auth_header) - 1);
    strcpy(config.host, HOST);
    config.port = PORT;

    // 根据是否海外选择 base_path
    if (is_overseas) {
        snprintf(config.base_path, sizeof(config.base_path), "%s", BASE_PATH_OVERSEAS);
    } else {
        snprintf(config.base_path, sizeof(config.base_path), "%s", BASE_PATH);
    }

    // 确保字符串安全结束
    config.app_id[sizeof(config.app_id)-1] = '\0';
    config.auth_header[sizeof(config.auth_header)-1] = '\0';
    config.base_path[sizeof(config.base_path)-1] = '\0';

    printf("agora_config_init: app_id=%s, auth_header=%s, base_path=%s, host=%s:%d %s region\n",
           config.app_id, config.auth_header, config.base_path, config.host, config.port,
           is_overseas ? "OVERSEAS" : "DOMESTIC");
}

// 创建SSL上下文
static SSL_CTX *create_ssl_context(void) {
  SSL_CTX *ctx = SSL_CTX_new(TLS_client_method());
  if (!ctx) {
    fprintf(stderr, "Error creating SSL context\n");
    ERR_print_errors_fp(stderr);
    return NULL;
  }

  SSL_CTX_set_options(
      ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_COMPRESSION);
  return ctx;
}

// 建立TCP连接
static int create_socket(const char *hostname, int port) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("Error opening socket");
    return -1;
  }

  struct hostent *server = gethostbyname(hostname);
  if (!server) {
    fprintf(stderr, "Error: No such host\n");
    close(sockfd);
    return -1;
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  memcpy(&server_addr.sin_addr.s_addr, server->h_addr, server->h_length);
  server_addr.sin_port = htons(port);

  if (connect(sockfd, (struct sockaddr *)&server_addr, sizeof(server_addr)) <
      0) {
    perror("Error connecting");
    close(sockfd);
    return -1;
  }

  return sockfd;
}

/* 安全获取字符串（返回 NULL 表示不存在或不是 string） */
static const char *cjson_get_string(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsString(item) && item->valuestring != NULL)
        return item->valuestring;
    return NULL;
}

/* 安全获取整数（不存在返回 0） */
static long long cjson_get_int(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsNumber(item))
        return (long long)item->valuedouble;
    return 0;
}

/* 安全获取布尔值 */
static int cjson_get_bool(cJSON *obj, const char *key)
{
    cJSON *item = cJSON_GetObjectItemCaseSensitive(obj, key);
    if (cJSON_IsBool(item))
        return cJSON_IsTrue(item);
    return 0;
}

/* 打印整个 JSON（调试用，和你原来的 parse_response_json 功能一样） */
static void print_cjson_object(cJSON *obj)
{
    if (!obj) {
        printf("  (null)\n");
        return;
    }

    const char *key;
    cJSON *child = NULL;

    cJSON_ArrayForEach(child, obj) {
        key = child->string;   // 在 object 中，child->string 就是 key
        if (!key) continue;

        if (cJSON_IsString(child))
            printf("  %s: %s\n", key, child->valuestring ? child->valuestring : "null");
        else if (cJSON_IsNumber(child))
            printf("  %s: %lld\n", key, (long long)child->valuedouble);
        else if (cJSON_IsBool(child))
            printf("  %s: %s\n", key, cJSON_IsTrue(child) ? "true" : "false");
        else if (cJSON_IsNull(child))
            printf("  %s: null\n", key);
        else if (cJSON_IsObject(child) || cJSON_IsArray(child))
            printf("  %s: [complex object]\n", key);
    }
}

// 构建HTTP请求字符串
char *build_http_request(const HttpRequest *req, const char *agent_id) {
  char *request = NULL;
  int length = 0;

  if (strcmp(req->method, "POST") == 0) {
    if (req->body) {
      length = snprintf(NULL, 0,
                        "%s %s HTTP/1.1\r\n"
                        "Host: %s\r\n"
                        "Authorization: Basic %s\r\n"
                        "%s"  // Content-Type if present
                        "Content-Length: %zu\r\n"
                        "Connection: close\r\n"
                        "\r\n"
                        "%s",
                        req->method, req->path, config.host, config.auth_header,
                        req->content_type ? req->content_type : "",
                        req->body ? strlen(req->body) : 0,
                        req->body ? req->body : "");
    } else {
      length =
          snprintf(NULL, 0,
                   "%s %s HTTP/1.1\r\n"
                   "Host: %s\r\n"
                   "Authorization: Basic %s\r\n"
                   "Content-Length: 0\r\n"
                   "Connection: close\r\n"
                   "\r\n",
                   req->method, req->path, config.host, config.auth_header);
    }
  } else {  // GET
    length = snprintf(NULL, 0,
                      "%s %s HTTP/1.1\r\n"
                      "Host: %s\r\n"
                      "Authorization: Basic %s\r\n"
                      "Content-Length: 0\r\n"
                      "Connection: close\r\n"
                      "\r\n",
                      req->method, req->path, config.host, config.auth_header);
  }

  request = malloc(length + 1);
  if (!request) {
    return NULL;
  }

  if (strcmp(req->method, "POST") == 0 && req->body) {
    snprintf(request, length + 1,
             "%s %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Authorization: Basic %s\r\n"
             "%s"
             "Content-Length: %zu\r\n"
             "Connection: close\r\n"
             "\r\n"
             "%s",
             req->method, req->path, config.host, config.auth_header,
             req->content_type ? req->content_type : "", strlen(req->body),
             req->body);
  } else if (strcmp(req->method, "POST") == 0) {
    snprintf(request, length + 1,
             "%s %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Authorization: Basic %s\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n"
             "\r\n",
             req->method, req->path, config.host, config.auth_header);
  } else {
    snprintf(request, length + 1,
             "%s %s HTTP/1.1\r\n"
             "Host: %s\r\n"
             "Authorization: Basic %s\r\n"
             "Content-Length: 0\r\n"
             "Connection: close\r\n"
             "\r\n",
             req->method, req->path, config.host, config.auth_header);
  }

  return request;
}

// 从HTTP响应中提取JSON
char *extract_json_from_response(const char *response) {
  const char *json_start = strstr(response, "\r\n\r\n");
  if (!json_start) {
    json_start = strstr(response, "\n\n");
  }

  if (json_start) {
    json_start += 4;
    const char *brace = strchr(json_start, '{');
    if (brace) {
      return strdup(brace);
    }
  }

  return NULL;
}

// 统一的HTTPS请求发送函数
static HttpResponse *send_https_request(const HttpRequest *req,
                                        const char *agent_id_for_path) {
  SSL_CTX *ctx = NULL;
  SSL *ssl = NULL;
  int sockfd = -1;
  HttpResponse *response = NULL;

  // 构建完整路径
  char full_path[1024];
  if (agent_id_for_path && strlen(agent_id_for_path) > 0) {
    snprintf(full_path, sizeof(full_path), req->path, config.base_path, config.app_id,
             agent_id_for_path);
  } else {
    snprintf(full_path, sizeof(full_path), req->path, config.base_path, config.app_id);
  }

  HttpRequest complete_req = {.method = req->method,
                              .path = full_path,
                              .body = req->body,
                              .content_type = req->content_type};

  // 构建HTTP请求
  char *http_request = build_http_request(&complete_req, agent_id_for_path);
  if (!http_request) {
    fprintf(stderr, "Failed to build HTTP request\n");
    return NULL;
  }

  // 创建SSL上下文和连接
  ctx = create_ssl_context();
  if (!ctx) goto cleanup;

  sockfd = create_socket(config.host, config.port);
  if (sockfd < 0) goto cleanup;

  ssl = SSL_new(ctx);
  if (!ssl) {
    fprintf(stderr, "Error creating SSL structure\n");
    goto cleanup;
  }

  SSL_set_fd(ssl, sockfd);
  if (SSL_connect(ssl) <= 0) {
    fprintf(stderr, "SSL connection failed\n");
    ERR_print_errors_fp(stderr);
    goto cleanup;
  }

  // 发送请求
  printf("Sending %s request to %s...\n", req->method, complete_req.path);
  int bytes_sent = SSL_write(ssl, http_request, strlen(http_request));
  if (bytes_sent <= 0) {
    fprintf(stderr, "SSL_write failed\n");
    goto cleanup;
  }
  printf("Request sent (%d bytes)\n\n", bytes_sent);

  // 接收响应
  response = malloc(sizeof(HttpResponse));
  if (!response) goto cleanup;

  response->body = NULL;
  response->body_length = 0;
  response->status_code = 0;

  char buffer[BUFFER_SIZE];
  ssize_t bytes_received;

  printf("Receiving response...\n");
  do {
    memset(buffer, 0, sizeof(buffer));
    bytes_received = SSL_read(ssl, buffer, sizeof(buffer) - 1);

    if (bytes_received > 0) {
      buffer[bytes_received] = '\0';
      printf("%s", buffer);

      // 解析状态码（从第一行）
      if (response->status_code == 0 && strstr(buffer, "HTTP/1.1") != NULL) {
        sscanf(buffer, "HTTP/1.1 %d", &response->status_code);
      }

      // 累积响应体
      char *new_body =
          realloc(response->body, response->body_length + bytes_received + 1);
      if (!new_body) {
        fprintf(stderr, "Memory allocation failed\n");
        free(response->body);
        free(response);
        response = NULL;
        goto cleanup;
      }
      response->body = new_body;
      memcpy(response->body + response->body_length, buffer, bytes_received);
      response->body_length += bytes_received;
      response->body[response->body_length] = '\0';
    }
  } while (bytes_received > 0);

  printf("\nResponse received (%zu bytes, status: %d)\n\n",
         response->body_length, response->status_code);

cleanup:
  free(http_request);

  if (ssl) {
    SSL_shutdown(ssl);
    SSL_free(ssl);
  }

  if (sockfd >= 0) {
    close(sockfd);
  }

  if (ctx) {
    SSL_CTX_free(ctx);
  }

  return response;
}

// 释放响应内存
static void free_response(HttpResponse *response) {
  if (response) {
    free(response->body);
    free(response);
  }
}

// 解析响应JSON
static int parse_response_json(const char *json_str)
{
    printf("\nResponse JSON:\n%s\n\n", json_str);

    cJSON *root = cJSON_Parse(json_str);
    if (!root) {
        const char *error_ptr = cJSON_GetErrorPtr();
        if (error_ptr)
            fprintf(stderr, "cJSON Parse error before: %s\n", error_ptr);
        else
            fprintf(stderr, "cJSON Parse error (unknown)\n");
        return -1;
    }

    print_cjson_object(root);
    cJSON_Delete(root);
    return 0;
}

/**
 * 创建对话式 AI Agent
 * @param token       RTC token
 * @param channel     频道名
 * @param out_agent_id  调用者提供的缓冲区，至少 128 字节
 * @param buf_size     out_agent_id 缓冲区大小
 * @param language_code 语言代码，如 "zh-CN", "en-US"
 * @return 0 成功，<0 失败
 */
int send_join_request(const char *token, const char *channel, uint32_t uid,
                      char *out_agent_id, size_t buf_size,
                      const char *language_code)
{
    if (!token || !channel || !out_agent_id || buf_size < 128) {
        fprintf(stderr, "Invalid parameters for join request\n");
        return -1;
    }

    char *json_body = build_join_json_body(channel, token, uid, language_code);
    if (!json_body) {
        fprintf(stderr, "Failed to build JSON body\n");
        return -1;
    }

    HttpRequest req = {
        .method = "POST",
        .path = "%s/%s/join",
        .body = json_body,
        .content_type = "Content-Type: application/json\r\n",
    };

    HttpResponse *resp = send_https_request(&req, NULL);
    free(json_body);

  if (!resp) return -1;

  int ret = -1;
  if (resp->status_code == 200 || resp->status_code == 409) {
    char *json = extract_json_from_response(resp->body);
    if (json) {
      cJSON *root = cJSON_Parse(json);
      if (root) {
          const char *aid = cjson_get_string(root, "agent_id");
          if (aid) {
              strncpy(out_agent_id, aid, buf_size - 1);
              out_agent_id[buf_size - 1] = '\0';
              if (resp->status_code == 200) {
                printf("Successfully obtained agent_id: %s\n", out_agent_id);
                ret = 0;
              } else {
                printf("Agent already exists with agent_id: %s\n", out_agent_id);
                ret = 1;
              }
          } else {
              fprintf(stderr, "agent_id field not found or not string\n");
          }
          cJSON_Delete(root);
      } else {
        fprintf(stderr, "Failed to parse JSON in JOIN response\n");
      }
        free(json);
      }
    }

  free_response(resp);
  return ret;
}

int send_leave_request(char *agent_id) {
  if (strlen(agent_id) == 0) {
    printf("No agent_id available for leave request\n");
    return -1;
  }

  HttpRequest req = {.method = "POST",
                     .path = "%s/%s/agents/%s/leave",
                     .body = NULL,
                     .content_type = NULL};

  HttpResponse *resp = send_https_request(&req, agent_id);
  if (!resp) return -1;

  // 检查响应状态码
  if (resp->status_code != 200) {
    printf("✗ request failed with status: %d\n", resp->status_code);
    free_response(resp);
    return -1;
  }

  printf("Leave request completed with status: %d\n", resp->status_code);
  free_response(resp);
  return 0;
}

int send_update_request(char *agent_id, char *token) {
  if (strlen(agent_id) == 0) {
    printf("No agent_id available for update request\n");
    return -1;
  }

  char *json_body = NULL;
  if (asprintf(&json_body, change_param_json_data, token) < 0) {
    fprintf(stderr, "asprintf failed\n");
    return -1;
  }

  HttpRequest req = {.method = "POST",
                     .path = "%s/%s/agents/%s/update",
                     .body = json_body,
                     .content_type = "Content-Type: application/json\r\n"};

  HttpResponse *resp = send_https_request(&req, agent_id);
  if (!resp) return -1;

  // 检查响应状态码
  if (resp->status_code != 200) {
    printf("✗ request failed with status: %d\n", resp->status_code);
    free_response(resp);
    return -1;
  }

  char *json = extract_json_from_response(resp->body);
  if (json) {
    parse_response_json(json);
    free(json);
  }

  free_response(resp);
  return 0;
}

int send_get_status_request(char *agent_id) {
  if (strlen(agent_id) == 0) {
    printf("No agent_id available for get status request\n");
    return -1;
  }

  HttpRequest req = {.method = "GET",
                     .path = "%s/%s/agents/%s",
                     .body = NULL,
                     .content_type = NULL};

  HttpResponse *resp = send_https_request(&req, agent_id);
  if (!resp) return -1;

  // 检查响应状态码
  if (resp->status_code != 200) {
    printf("✗ request failed with status: %d\n", resp->status_code);
    free_response(resp);
    return -1;
  }

  char *json = extract_json_from_response(resp->body);
  if (json) {
    parse_response_json(json);
    free(json);
  }

  free_response(resp);
  return 0;
}

int send_get_list_request(char *agent_id) {
  HttpRequest req = {.method = "GET",
                     .path = "%s/%s/agents",
                     .body = NULL,
                     .content_type = NULL};

  HttpResponse *resp = send_https_request(&req, NULL);
  if (!resp) return -1;

  // 检查响应状态码
  if (resp->status_code != 200) {
    printf("✗ request failed with status: %d\n", resp->status_code);
    free_response(resp);
    return -1;
  }

  char *json = extract_json_from_response(resp->body);
  if (json) {
    parse_response_json(json);
    free(json);
  }

  free_response(resp);
  return 0;
}

int send_speak_request(char *agent_id) {
  if (strlen(agent_id) == 0) {
    printf("No agent_id available for speak request\n");
    return -1;
  }

  HttpRequest req = {.method = "POST",
                     .path = "%s/%s/agents/%s/speak",
                     .body = speak_message_json_data,
                     .content_type = "Content-Type: application/json\r\n"};

  HttpResponse *resp = send_https_request(&req, agent_id);
  if (!resp) return -1;

  // 检查响应状态码
  if (resp->status_code != 200) {
    printf("✗ request failed with status: %d\n", resp->status_code);
    free_response(resp);
    return -1;
  }

  char *json = extract_json_from_response(resp->body);
  if (json) {
    parse_response_json(json);
    free(json);
  }

  free_response(resp);
  return 0;
}

// 实现打断智能体请求函数
int send_interrupt_request(char *agent_id) {
  if (strlen(agent_id) == 0) {
    printf("No agent_id available for interrupt request\n");
    return -1;
  }

  // 根据文档，打断接口的请求体是空对象 {}
  HttpRequest req = {.method = "POST",
                     .path = "%s/%s/agents/%s/interrupt",
                     .body = interrupt_agent_json_data,
                     .content_type = "Content-Type: application/json\r\n"};

  HttpResponse *resp = send_https_request(&req, agent_id);
  if (!resp) return -1;

  // 检查响应状态码
  if (resp->status_code != 200) {
    printf("✗ request failed with status: %d\n", resp->status_code);
    free_response(resp);
    return -1;
  }

  char *json = extract_json_from_response(resp->body);
  if (json) {
    printf("✓ Interrupt request successful\n");
    free(json);
  } else {
    printf("✗ No JSON response received\n");
  }

  free_response(resp);
  return 0;
}

// 实现获取智能体短期记忆请求函数
int send_get_history_request(char *agent_id) {
  if (strlen(agent_id) == 0) {
    printf("No agent_id available for get history request\n");
    return -1;
  }

  HttpRequest req = {.method = "GET",
                     .path = "%s/%s/agents/%s/history",
                     .body = NULL,
                     .content_type = NULL};

  HttpResponse *resp = send_https_request(&req, agent_id);
  if (!resp) return -1;

  // 检查响应状态码
  if (resp->status_code != 200) {
    printf("✗ request failed with status: %d\n", resp->status_code);
    free_response(resp);
    return -1;
  }

  char *json = extract_json_from_response(resp->body);
  if (json) {
    free(json);
  }

  free_response(resp);
  return 0;
}

void agora_openssl_init(void) {
  // 初始化OpenSSL
  SSL_library_init();
  OpenSSL_add_all_algorithms();
  SSL_load_error_strings();
  // ERR_load_crypto_strings();
}

void agora_openssl_deinit(void) {
  // 清理OpenSSL
  EVP_cleanup();
  ERR_free_strings();
}

int agora_test() {
  int ret = 0;
  agora_openssl_init();
  printf("=== Starting Agora API Tests ===\n\n");
  char agent_id[128] = {0};
  /*
  -A 1 中文agent
  -A 2 英文agent
./agora_module  -i a38a96a8b0674f79b17497c068cb24a8 -c convaiconsole_10086 -u 10086 -A 2 -d -t 007eJxTYFi8aMduBufXOZ9186dpskkxttgZ5BxoaUmVeXvvrfxB54sKDOaJiRaJFsbmKRbmliZJ5gaWBpYmpibmpsaWpslmxobmz0uCMxsCGRnkXvIxMTJAIIgvzJCcn1eWmAkki/NzUuMNDQwszBgYAOI0Isw=
  */
  char *token =
      "007eJxTYCgt3Jn74MX0GytOranS3bz8dNL7vqvqT38lLGZNjvj8a+4jBYZEY4tES7NEiyQDM3OTNHPLJENzE0vzZAMzi+QkIxOgeL91ZkMgI4MdsyILIwMEgvjCDMn5eWWJmUCyOD8nNd7QwMDCjIEBAMM9J08=";
  char *channel = "convaiconsole_10086";

  // 创建对话式智能体
  printf("Sending JOIN request...\n");
  if (send_join_request(token, channel, 251156, agent_id, sizeof(agent_id), "zh-CN") != 0) {
    printf("✗ JOIN request failed\n");
    ret = 1;
  }

  // 播报自定义消息
  printf("Sending SPEAK request (custom message)...\n");
  if (send_speak_request(agent_id) != 0) {
    printf("✗ SPEAK request failed\n");
    ret = 1;
  }

  // 打断智能体
  printf("Sending INTERRUPT request...\n");
  if (send_interrupt_request(agent_id) != 0) {
    printf("✗ INTERRUPT request failed\n");
    ret = 1;
  }

  // 获取智能体短期记忆
  printf("Sending GET HISTORY request...\n");
  if (send_get_history_request(agent_id) != 0) {
    printf("✗ GET HISTORY request failed\n");
    ret = 1;
  }

  // 查询智能体状态
  printf("Sending GET STATUS request...\n");
  if (send_get_status_request(agent_id) != 0) {
    printf("✗ GET STATUS request failed\n");
    ret = 1;
  }

  // 获取智能体列表
  printf("Sending GET LIST request...\n");
  if (send_get_list_request(agent_id) != 0) {
    printf("✗ GET LIST request failed\n");
    ret = 1;
  }

  // 更新智能体配置
  printf("Sending UPDATE request...\n");
  if (send_update_request(agent_id, token) != 0) {
    printf("✗ UPDATE request failed\n");
    ret = 1;
  }

  // 停止对话式智能体
  printf("Sending LEAVE request...\n");
  if (send_leave_request(agent_id) != 0) {
    printf("✗ LEAVE request failed\n");
    ret = 1;
  }

  printf("\n=== All operations completed ===\n");
  if (strlen(agent_id) > 0) {
    printf("Final agent_id: %s\n", agent_id);
  }

  agora_openssl_deinit();

  return ret;
}