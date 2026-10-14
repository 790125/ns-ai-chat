#pragma once

#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <SDL2/SDL_image.h>

#include <string>
#include <deque>
#include <mutex>

// ============================================================
// 聊天消息结构体（在 main.cpp 和 render 模块间共享）
// ============================================================
struct ChatMessage {
    std::string role;    // "user" 或 "assistant"
    std::string text;
    bool streaming;      // AI 正在流式输出
};

// 屏幕尺寸常量
const int SCREEN_W = 1280;
const int SCREEN_H = 720;

// 全局变量声明（定义在 Ns_Practice.cpp）
extern std::deque<ChatMessage> g_messages;
extern std::mutex g_chatMutex;
extern std::string g_inputBuffer;
extern bool g_useGamepad;       // 运行环境是否启用手柄输入

// 虚拟键盘状态（手柄操控的屏幕键盘）
extern bool   g_vkActive;
extern int    g_vkRow;
extern int    g_vkCol;

// ============================================================
// 聊天界面渲染函数（在 Deepseek_Running_Logite.cpp 实现）
// ============================================================
void renderChat(SDL_Renderer* renderer, TTF_Font* font);
