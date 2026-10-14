#pragma once

#include <string>
#include <functional>
#include <vector>
#include <nlohmann/json.hpp>

using json = nlohmann::json;

class DeepSeekClient {
public:
    using Callback = std::function<void(const std::string& chunk, bool done, bool error)>;

    // 代理运行在 Yuzu 宿主机上，Switch 用纯 HTTP 连接
    // proxy_host 为空 → 直连 api.deepseek.com (HTTPS)
    DeepSeekClient(const std::string& proxy_host, int proxy_port);

    void setApiKey(const std::string& key);
    void sendMessage(const std::string& message, Callback callback);
    void setSystemPrompt(const std::string& prompt);

    // 会话管理
    void newConversation();
    const std::vector<json>& getHistory() const { return m_messages; }

private:
    friend int httpThreadFunc(void* ptr);  // SDL 线程回调需要访问成员

    std::string m_apiKey;
    std::string m_proxyHost;  // 宿主机 IP（Yuzu 内通常是 192.168.x.x）
    int m_proxyPort;          // 代理端口，如 8888
    std::string m_systemPrompt;
    std::vector<json> m_messages;
};
