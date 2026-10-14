// Ns_Practice.cpp — Switch AI Chat (跨平台: Switch / Windows / Yuzu)
// SDL2 + DeepSeek API, 支持键盘和手柄双层输入

// Windows 下 SDL2 会通过 SDL_main.h 将 main 宏替换为 SDL_main，
// 需要 SDL_MAIN_HANDLED 阻止此行为，保留原始 main 作为入口点。
// Switch (libnx) 环境下由系统约定处理入口点，不做干预。
#ifndef __SWITCH__
#define SDL_MAIN_HANDLED
#endif

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include "Deepseek_Running_Logite.hpp"  // ChatMessage, SCREEN_W/H, renderChat, 全局变量声明
#include "deepseek.hpp"                 // DeepSeekClient 类

#include <string>
#include <cstdlib>
#include <deque>
#include <mutex>
#include <cstring>

// ============================================================
// 全局变量定义（在 Deepseek_Running_Logite.hpp 中声明为 extern）
// ============================================================
std::deque<ChatMessage> g_messages;
std::mutex g_chatMutex;
DeepSeekClient* g_deepseek = nullptr;
std::string g_inputBuffer;   // 当前输入缓冲区
bool g_useGamepad = false;   // 运行环境是否启用手柄
bool g_vkActive  = false;    // 虚拟键盘是否激活
int  g_vkRow     = 0;        // 虚拟键盘光标行
int  g_vkCol     = 0;        // 虚拟键盘光标列

// ============================================================
// 键盘输入函数（Switch 软键盘 / PC 控制台后备——仅在 Switch 使用）
// ============================================================
#ifdef __SWITCH__
#include <switch.h>

// 调用 Switch 原生软件键盘（阻塞调用，yuzu 中可用鼠标/键盘操作）
std::string showSwitchKeyboard(const std::string& hint) {
    SwkbdConfig kbd;
    swkbdCreate(&kbd, 0);
    swkbdConfigMakePresetDefault(&kbd);
    swkbdConfigSetHeaderText(&kbd, hint.c_str());
    swkbdConfigSetOkButtonText(&kbd, "Send");
    swkbdConfigSetType(&kbd, SwkbdType_Normal);

    char buf[512] = { 0 };
    Result rc = swkbdShow(&kbd, buf, sizeof(buf));
    swkbdClose(&kbd);

    if (R_SUCCEEDED(rc) && buf[0] != '\0') {
        return std::string(buf);
    }
    return "";
}
#endif

// ============================================================
// 发送消息（提取为公共函数，被键盘和手柄共用）
// ============================================================
static void doSendMessage(DeepSeekClient& ds, const std::string& text) {
    if (text.empty()) return;

    // 添加用户消息
    {
        std::lock_guard<std::mutex> lock(g_chatMutex);
        g_messages.push_back({ "user", text, false });
        g_messages.push_back({ "assistant", "", true });
    }

    // 发送到 DeepSeek
    ds.sendMessage(text,
        [](const std::string& chunk, bool done, bool error) {
            std::lock_guard<std::mutex> lock(g_chatMutex);
            for (auto& msg : g_messages) {
                if (msg.streaming) {
                    if (error) {
                        msg.text = "Error: " + chunk;
                        msg.streaming = false;
                    } else if (done) {
                        msg.text = chunk;
                        msg.streaming = false;
                    } else {
                        msg.text += chunk;
                    }
                    break;
                }
            }
        });
}

// ============================================================
// 新建对话
// ============================================================
static void doNewConversation(DeepSeekClient& ds) {
    ds.newConversation();
    std::lock_guard<std::mutex> lock(g_chatMutex);
    g_messages.clear();
    g_messages.push_back({ "assistant", "New conversation started.", false });
}

// ============================================================
// 主函数
// ============================================================
int main(int argc, char* argv[]) {
    // --------------------------------------------------------
    // §1. 环境检测：决定输入方案
    // --------------------------------------------------------
    // 三层判断：
    //   __SWITCH__                → 原生 Switch：手柄 + Swkbd 软键盘
    //   !__SWITCH__ && 手柄存在   → Windows + Yuzu 模拟器：键盘 + 手柄双支持
    //   !__SWITCH__ && 无手柄     → 纯 Windows：仅键盘
    // --------------------------------------------------------
#ifdef __SWITCH__
    socketInitializeDefault();
    nxlinkStdio();
    romfsInit();                       // 挂载 RomFS（字体资源）—— 缺失会导致 romfs:/ 路径失效！

    g_useGamepad = true;             // Switch 上始终用手柄
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);
#else
    // Windows: 先初始化 joystick 子系统以检测手柄数量
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_JOYSTICK);

    int joystickCount = SDL_NumJoysticks();
    if (joystickCount > 0) {
        g_useGamepad = true;         // Yuzu 模拟器中检测到手柄
        for (int i = 0; i < joystickCount; i++) {
            SDL_JoystickOpen(i);
        }
    }
#endif
    // 启用 SDL 文本输入 — 全平台生效（Yuzu 中宿主键盘通过 SDL 事件传入）
    SDL_StartTextInput();
    TTF_Init();

    SDL_Window* win = SDL_CreateWindow("AI Chat (DeepSeek)",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_W, SCREEN_H, SDL_WINDOW_SHOWN);
    SDL_Renderer* renderer = SDL_CreateRenderer(win, -1,
        SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

    // --------------------------------------------------------
    // §2. 字体加载
    // --------------------------------------------------------
    TTF_Font* font = nullptr;
#ifdef __SWITCH__
    font = TTF_OpenFont("romfs:/fonts/NotoSansSC-Regular.ttf", 22);
#else
    const char* fontPaths[] = {
        "C:\\Windows\\Fonts\\msyh.ttc",
        "C:\\Windows\\Fonts\\simhei.ttf",
        "C:\\Windows\\Fonts\\simsun.ttc",
        "NotoSansSC-Regular.ttf",
    };
    for (const char* path : fontPaths) {
        font = TTF_OpenFont(path, 22);
        if (font) break;
    }
#endif
    if (!font) {
#ifdef __SWITCH__
        fprintf(stderr, "FATAL: Failed to load font from romfs!\n");
#else
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "Font Error", "Failed to load a Chinese font!", win);
#endif
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }

    // --------------------------------------------------------
    // §3. DeepSeek 客户端初始化
    // --------------------------------------------------------
#ifdef __SWITCH__
    // Switch/Yuzu: 经宿主机代理中转（纯 HTTP 避免 SSL 问题）
    DeepSeekClient ds("192.168.1.100", 8888);
#else
    // Windows 直连: 空 proxy → 直接 HTTPS 访问 api.deepseek.com
    DeepSeekClient ds("", 0);
#endif
    // ⚠️ 从环境变量读取 API Key，绝对不能硬编码到源码里
    const char* apiKey = std::getenv("DEEPSEEK_API_KEY");
    if (!apiKey) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR,
            "API Key Missing",
            "Please set the DEEPSEEK_API_KEY environment variable.\n\n"
            "Windows:  set DEEPSEEK_API_KEY=sk-xxxxx\n"
            "Switch:  export DEEPSEEK_API_KEY=sk-xxxxx",
            win);
        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(win);
        TTF_Quit();
        SDL_Quit();
        return 1;
    }
    ds.setApiKey(apiKey);
    ds.setSystemPrompt("You are a helpful AI assistant. Answer concisely.");

    g_deepseek = &ds;

    // 启动提示
    {
        std::lock_guard<std::mutex> lock(g_chatMutex);
        if (g_useGamepad) {
            g_messages.push_back({ "assistant",
                "DeepSeek AI ready! Press [+] for keyboard, [-] New, [B] Quit.", false });
        } else {
            g_messages.push_back({ "assistant",
                "DeepSeek AI ready! Type your message and press Enter.", false });
        }
    }

    // --------------------------------------------------------
    // §4. 主循环：统一的输入事件处理
    // --------------------------------------------------------
    bool running = true;
    g_inputBuffer.clear();

    while (running) {
        SDL_Event e;
        while (SDL_PollEvent(&e)) {
            switch (e.type) {

            // ---------- 窗口关闭 ----------
            case SDL_QUIT:
                running = false;
                break;

            // ---------- 键盘输入 ----------
            // 注意：Switch/Yuzu 上 SDL_TEXTINPUT 不触发，但保留兼容性
            case SDL_TEXTINPUT: {
                g_inputBuffer += e.text.text;
                break;
            }

            case SDL_KEYDOWN: {
                switch (e.key.keysym.sym) {

                case SDLK_RETURN:       // Enter: 发送（VK 模式下无效，由手柄 + 键处理）
                case SDLK_KP_ENTER:
                    if (!g_vkActive) {
                        doSendMessage(ds, g_inputBuffer);
                        g_inputBuffer.clear();
                    }
                    break;

                case SDLK_ESCAPE:       // Esc: 退出 / VK 模式下关闭 VK
                    if (g_vkActive) {
                        g_vkActive = false;
                    } else {
                        running = false;
                    }
                    break;

                case SDLK_n:            // N: 新建对话
                    if (SDL_GetModState() & KMOD_CTRL) break;
                    doNewConversation(ds);
                    g_inputBuffer.clear();
                    g_vkActive = false;
                    break;

                case SDLK_BACKSPACE:    // 退格
                    if (!g_inputBuffer.empty()) {
                        size_t p = g_inputBuffer.size() - 1;
                        while (p > 0 && (g_inputBuffer[p] & 0xC0) == 0x80) p--;
                        g_inputBuffer.erase(p);
                    }
                    break;
                }
                break;
            }

            // ---------- 手柄 HAT (D-Pad) — 虚拟键盘导航 ----------
            case SDL_JOYHATMOTION: {
                if (!g_vkActive) break;
                static Uint32 lastHat = 0;
                Uint32 now = SDL_GetTicks();
                if (now - lastHat < 150) break;  // 防抖 150ms
                lastHat = now;

                int val = e.jhat.value;
                if (val & SDL_HAT_UP)    { if (g_vkRow > 0) g_vkRow--; }
                if (val & SDL_HAT_DOWN)  { if (g_vkRow < 3) g_vkRow++; }
                if (val & SDL_HAT_LEFT)  { if (g_vkCol > 0) g_vkCol--; }
                if (val & SDL_HAT_RIGHT) { g_vkCol++; }
                // 限制列范围（每行长度不同）
                static const int maxCols[] = {9, 8, 9, 3};
                if (g_vkCol > maxCols[g_vkRow]) g_vkCol = maxCols[g_vkRow];
                break;
            }

            // ---------- 手柄按键 (虚拟键盘 或 正常模式) ----------
            case SDL_JOYBUTTONDOWN: {
                if (!g_useGamepad) break;

                int btn = e.jbutton.button;

                // ======== 虚拟键盘模式（全平台共用）========
                // D-Pad 按钮映射（Switch: Up=13 Down=14 Left=15 Right=16）
                if (g_vkActive) {
                    static Uint32 lastDpadBtn = 0;
                    Uint32 now = SDL_GetTicks();
                    if ((btn >= 13 && btn <= 16) && (now - lastDpadBtn < 150)) break;
                    if (btn >= 13 && btn <= 16) lastDpadBtn = now;

                    switch (btn) {
                    case 13: if (g_vkRow > 0) g_vkRow--; break;          // D-Up
                    case 14: if (g_vkRow < 3) g_vkRow++; break;          // D-Down
                    case 15: if (g_vkCol > 0) g_vkCol--; break;          // D-Left
                    case 16: g_vkCol++; break;                           // D-Right
                    case 0: { // A → 选择字符
                        // 字符行 0-2
                        static const char* rows[] = { "qwertyuiop", "asdfghjkl", "zxcvbnm,.!" };
                        if (g_vkRow < 3) {
                            g_inputBuffer += rows[g_vkRow][g_vkCol];
                        } else {
                            // 特殊按键行
                            switch (g_vkCol) {
                            case 0: g_inputBuffer += " "; break;          // SPACE
                            case 1: // BACK
                                if (!g_inputBuffer.empty()) {
                                    size_t p = g_inputBuffer.size() - 1;
                                    while (p > 0 && (g_inputBuffer[p] & 0xC0) == 0x80) p--;
                                    g_inputBuffer.erase(p);
                                }
                                break;
                            case 2: // SEND — 发送后 VK 保持活跃，不清空键盘状态
                                doSendMessage(ds, g_inputBuffer);
                                g_inputBuffer.clear();
                                // g_vkActive 保持 true，键盘不消失
                                g_vkRow = 0;
                                g_vkCol = 0;
                                break;
                            case 3: g_vkActive = false; break;           // CANCEL → 关闭键盘
                            }
                        }
                        break;
                    }
                    case 1: // B → 退格 / 空缓冲时关闭键盘
                        if (!g_inputBuffer.empty()) {
                            size_t p = g_inputBuffer.size() - 1;
                            while (p > 0 && (g_inputBuffer[p] & 0xC0) == 0x80) p--;
                            g_inputBuffer.erase(p);
                        } else {
                            g_vkActive = false;
                        }
                        break;
                    case 10: // + → 切换键盘显示/隐藏
                        g_vkActive = false;
                        break;
                    case 11: // - → 新建对话
                        doNewConversation(ds);
                        g_inputBuffer.clear();
                        // VK 保持活跃，不清空
                        break;
                    }
                    // 重新限制列范围
                    { static const int mc[] = {9,8,9,3}; if (g_vkCol > mc[g_vkRow]) g_vkCol = mc[g_vkRow]; }
                    break;
                }

                // ======== 正常模式（VK 未激活时）========
                switch (btn) {
                case 10:  // + 键 → 打开虚拟键盘
                    g_vkActive = true;
                    g_vkRow = 0;
                    g_vkCol = 0;
                    break;
                case 1:   // B 键 → 退出
                    running = false;
                    break;
                case 11:  // - 键 → 新建对话
                    doNewConversation(ds);
                    g_inputBuffer.clear();
                    break;
                }
                break;
            }

            } // switch(e.type)
        } // while(PollEvent)

        renderChat(renderer, font);
        SDL_Delay(16); // ~60 FPS
    }

    // --------------------------------------------------------
    // §5. 清理
    // --------------------------------------------------------
    SDL_StopTextInput();
    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(win);
    TTF_Quit();
    SDL_Quit();

#ifdef __SWITCH__
    romfsExit();
    socketExit();
#endif

    return 0;
}
