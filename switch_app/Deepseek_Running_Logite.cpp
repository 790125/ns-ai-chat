#include "Deepseek_Running_Logite.hpp"

// ============================================================
// renderChat — 在 SDL2 窗口中渲染聊天消息列表 + 输入栏
// ============================================================
void renderChat(SDL_Renderer* renderer, TTF_Font* font) {
    std::lock_guard<std::mutex> lock(g_chatMutex);

    // 清屏（深蓝色背景）
    SDL_SetRenderDrawColor(renderer, 30, 30, 50, 255);
    SDL_RenderClear(renderer);

    int y = 20;
    SDL_Color userColor = { 100, 200, 100, 255 };
    SDL_Color aiColor   = { 100, 180, 255, 255 };
    SDL_Color white     = { 255, 255, 255, 255 };
    SDL_Color hintColor = { 160, 160, 180, 255 };

    for (auto& msg : g_messages) {
        std::string prefix = (msg.role == "user") ? "You: " : "AI: ";
        std::string display = prefix + msg.text;

        SDL_Color& color = (msg.role == "user") ? userColor : aiColor;

        SDL_Surface* surf = TTF_RenderUTF8_Blended_Wrapped(
            font, display.c_str(), color, SCREEN_W - 80);
        if (surf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst = { 40, y, surf->w, surf->h };
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
            y += surf->h + 10;
        }
    }

    // ---- 底部输入栏 ----
    SDL_Rect inputBar = { 20, SCREEN_H - 70, SCREEN_W - 40, 50 };
    SDL_SetRenderDrawColor(renderer, 50, 50, 70, 255);
    SDL_RenderFillRect(renderer, &inputBar);

    // 输入栏边框
    SDL_SetRenderDrawColor(renderer, 100, 180, 255, 100);
    SDL_RenderDrawRect(renderer, &inputBar);

    // 显示当前输入缓冲区内容（带闪烁光标）
    std::string displayInput = g_inputBuffer;
    // 每 ~500ms 闪烁一次光标（用 SDL_GetTicks 实现）
    if ((SDL_GetTicks() / 500) % 2 == 0) {
        displayInput += "|";
    }
    if (!displayInput.empty()) {
        SDL_Surface* inputSurf = TTF_RenderUTF8_Blended(
            font, displayInput.c_str(), white);
        if (inputSurf) {
            SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, inputSurf);
            SDL_Rect dst = { 30, SCREEN_H - 62, inputSurf->w, inputSurf->h };
            SDL_RenderCopy(renderer, tex, nullptr, &dst);
            SDL_FreeSurface(inputSurf);
            SDL_DestroyTexture(tex);
        }
    }

    // 底部操作提示
    SDL_Rect hintBar = { 20, SCREEN_H - 20, SCREEN_W - 40, 20 };
    SDL_SetRenderDrawColor(renderer, 40, 40, 60, 255);
    SDL_RenderFillRect(renderer, &hintBar);

    // ---- 底部操作提示 ----
    const char* hintText;
    if (g_vkActive) {
        hintText = "Virtual Keyboard: D-Pad=Move  A=Select  B=Back  +=Hide  -=New";
    } else if (g_useGamepad) {
        hintText = "Gamepad: [+] Keyboard  [B] Quit  [-] New Chat";
    } else {
        hintText = "Keyboard: Type  |  Enter=Send  |  Esc=Quit  |  N=New";
    }

    SDL_Surface* hint = TTF_RenderUTF8_Blended(font, hintText, hintColor);
    if (hint) {
        SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, hint);
        SDL_Rect dst = { 40, SCREEN_H - 18, hint->w, hint->h };
        SDL_RenderCopy(renderer, tex, nullptr, &dst);
        SDL_FreeSurface(hint);
        SDL_DestroyTexture(tex);
    }

    // ---- 虚拟键盘渲染（全平台 — SEND 后 VK 保持活跃）----
    if (g_vkActive) {
        // 4 行字符布局
        static const char* rows[] = {
            "qwertyuiop",
            "asdfghjkl",
            "zxcvbnm,.!",
            nullptr  // 第四行特殊处理
        };
        static const char* spKeys[] = { "SPACE", "BACK", "SEND", "CANCEL", nullptr };

        int vkY = SCREEN_H - 200;   // 键盘区域顶部
        int keyW = 60;
        int keyH = 40;
        int gap  = 4;

        for (int r = 0; r < 3; r++) {
            int len = (int)strlen(rows[r]);
            int startX = (SCREEN_W - len * (keyW + gap)) / 2;
            for (int c = 0; c < len; c++) {
                int kx = startX + c * (keyW + gap);
                int ky = vkY + r * (keyH + gap);

                SDL_Rect keyRect = { kx, ky, keyW, keyH };
                bool selected = (g_vkRow == r && g_vkCol == c);

                // 按键背景
                SDL_SetRenderDrawColor(renderer, selected ? 100 : 60,
                                               selected ? 180 : 80,
                                               selected ? 255 : 120, 255);
                SDL_RenderFillRect(renderer, &keyRect);
                SDL_SetRenderDrawColor(renderer, 150, 200, 255, 80);
                SDL_RenderDrawRect(renderer, &keyRect);

                // 按键文字
                char label[2] = { rows[r][c], 0 };
                SDL_Surface* ks = TTF_RenderUTF8_Blended(font, label, white);
                if (ks) {
                    SDL_Texture* kt = SDL_CreateTextureFromSurface(renderer, ks);
                    SDL_Rect kdst = { kx + (keyW - ks->w) / 2,
                                      ky + (keyH - ks->h) / 2,
                                      ks->w, ks->h };
                    SDL_RenderCopy(renderer, kt, nullptr, &kdst);
                    SDL_FreeSurface(ks);
                    SDL_DestroyTexture(kt);
                }
            }
        }

        // 第四行：特殊按键
        int spStartX = (SCREEN_W - 4 * (120 + gap)) / 2;
        for (int c = 0; spKeys[c]; c++) {
            int kx = spStartX + c * (120 + gap);
            int ky = vkY + 3 * (keyH + gap);

            SDL_Rect keyRect = { kx, ky, 120, keyH };
            bool selected = (g_vkRow == 3 && g_vkCol == c);

            SDL_SetRenderDrawColor(renderer, selected ? 100 : 60,
                                           selected ? 180 : 80,
                                           selected ? 255 : 120, 255);
            SDL_RenderFillRect(renderer, &keyRect);
            SDL_SetRenderDrawColor(renderer, 150, 200, 255, 80);
            SDL_RenderDrawRect(renderer, &keyRect);

            SDL_Surface* ks = TTF_RenderUTF8_Blended(font, spKeys[c], white);
            if (ks) {
                SDL_Texture* kt = SDL_CreateTextureFromSurface(renderer, ks);
                SDL_Rect kdst = { kx + (120 - ks->w) / 2,
                                  ky + (keyH - ks->h) / 2,
                                  ks->w, ks->h };
                SDL_RenderCopy(renderer, kt, nullptr, &kdst);
                SDL_FreeSurface(ks);
                SDL_DestroyTexture(kt);
            }
        }
    }

    SDL_RenderPresent(renderer);
}
