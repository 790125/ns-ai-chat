#include "deepseek.hpp"

#include <SDL2/SDL.h>
#include <curl/curl.h>
#include <string>

// ============================================================
// 内部辅助函数
// ============================================================

// libcurl 写回调：将响应数据追加到 std::string
static size_t writeCallback(void* contents, size_t size, size_t nmemb, void* userp) {
    size_t total = size * nmemb;
    auto* str = static_cast<std::string*>(userp);
    str->append(static_cast<char*>(contents), total);
    return total;
}

// ============================================================
// SDL 线程数据（替代 std::thread，保证 Switch 兼容性）
// ============================================================
struct RequestData {
    DeepSeekClient* self;
    std::string     bodyStr;
    DeepSeekClient::Callback callback;
};

int httpThreadFunc(void* ptr) {
    RequestData* data = static_cast<RequestData*>(ptr);
    DeepSeekClient* self = data->self;
    std::string bodyStr = std::move(data->bodyStr);
    auto callback = std::move(data->callback);
    delete data;  // 线程接管数据所有权

    CURL* curl = curl_easy_init();
    if (!curl) {
        callback("Network init failed", true, true);
        return 1;
    }

    std::string responseBuffer;

    // 根据是否配置代理决定 URL
    std::string url;
    bool useDirect = self->m_proxyHost.empty();
    if (useDirect) {
        url = "https://api.deepseek.com/v1/chat/completions";
    } else {
        url = "http://" + self->m_proxyHost + ":" +
              std::to_string(self->m_proxyPort) + "/v1/chat/completions";
    }

    // 构造 HTTP 请求头
    struct curl_slist* headers = nullptr;
    headers = curl_slist_append(headers,
        ("Authorization: Bearer " + self->m_apiKey).c_str());
    headers = curl_slist_append(headers, "Content-Type: application/json");

    // 设置 curl 选项
    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    curl_easy_setopt(curl, CURLOPT_POSTFIELDS, bodyStr.c_str());
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &responseBuffer);
    curl_easy_setopt(curl, CURLOPT_BUFFERSIZE, 1024L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 10L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 60L);

#ifdef __SWITCH__
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 0L);
    curl_easy_setopt(curl, CURLOPT_SSL_VERIFYHOST, 0L);
#else
    if (useDirect) {
        curl_easy_setopt(curl, CURLOPT_SSL_VERIFYPEER, 1L);
    }
#endif

    // 执行请求
    CURLcode res = curl_easy_perform(curl);
    curl_slist_free_all(headers);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK) {
        callback("Error: " + std::string(curl_easy_strerror(res)), true, true);
        return 1;
    }

    // 解析响应
    try {
        auto j = json::parse(responseBuffer);
        if (j.contains("choices") && !j["choices"].empty()) {
            std::string content = j["choices"][0]["message"]["content"];
            self->m_messages.push_back({{"role", "assistant"}, {"content", content}});
            callback(content, true, false);
        }
    } catch (...) {
        callback("Parse error", true, true);
    }

    return 0;
}

// ============================================================
// DeepSeekClient 成员函数实现
// ============================================================

DeepSeekClient::DeepSeekClient(const std::string& proxy_host, int proxy_port)
    : m_proxyHost(proxy_host), m_proxyPort(proxy_port) {
}

void DeepSeekClient::setApiKey(const std::string& key) {
    m_apiKey = key;
}

void DeepSeekClient::setSystemPrompt(const std::string& prompt) {
    m_systemPrompt = prompt;
}

void DeepSeekClient::newConversation() {
    m_messages.clear();
    if (!m_systemPrompt.empty()) {
        m_messages.push_back({
            {"role", "system"},
            {"content", m_systemPrompt}
        });
    }
}

void DeepSeekClient::sendMessage(const std::string& message, Callback callback) {
    // 添加用户消息到历史
    m_messages.push_back({{"role", "user"}, {"content", message}});

    // 构建请求 JSON
    json body;
    body["model"]    = "deepseek-chat";
    body["messages"] = m_messages;
    body["stream"]   = false;

    // 将数据打包到堆上，传递给 SDL 线程（SDL_CreateThread 兼容所有平台）
    RequestData* data = new RequestData;
    data->self     = this;
    data->bodyStr  = body.dump();
    data->callback = callback;

    SDL_CreateThread(httpThreadFunc, "http_req", data);
}
