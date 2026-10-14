# NS AI Chat — 任天堂 Switch 上的 AI 对话系统

[![Platform](https://img.shields.io/badge/platform-Switch%20%7C%20Windows-blue)](https://github.com)
[![Language](https://img.shields.io/badge/language-C%2B%2B%20%7C%20Python-orange)](https://github.com)
[![License](https://img.shields.io/badge/license-MIT-green)](LICENSE)

在 Nintendo Switch 游戏机上运行的**跨平台 AI 聊天客户端**，通过 DeepSeek API 实现实时智能对话。支持 SDL2 图形界面、手柄操控、中文输入，以及 Windows/Switch 双平台编译。

<p align="center">
  <img src="switch_app/icon.jpg" width="200" alt="App Icon">
</p>

## 系统架构

```
┌──────────────────────────────┐     HTTP      ┌────────────────────┐     HTTPS      ┌──────────────┐
│   Nintendo Switch / PC       │ ────────────→ │   PC 代理 (Python)  │ ────────────→ │  DeepSeek API │
│   (C++ / SDL2)              │ ←──────────── │   FastAPI :8888     │ ←──────────── │  (云端 AI)    │
└──────────────────────────────┘               └────────────────────┘               └──────────────┘
         │                                              │
    手柄 + 虚拟键盘输入                             纯 HTTP ↔ HTTPS 中转
    SDL2 图形渲染                                  （Switch 不支持 HTTPS）
```

### 核心模块

| 模块 | 文件 | 职责 |
|------|------|------|
| 主程序 & UI 循环 | `Ns_Practice.cpp` | SDL2 窗口管理、事件循环、手柄/键盘输入 |
| 聊天界面渲染 | `Deepseek_Running_Logite.cpp` | 消息气泡渲染、虚拟键盘、中文 RTL 排版 |
| HTTP 客户端 | `DeepSeekClient.cpp` | libcurl 异步 HTTP、对话历史管理、JSON 解析 |
| API 代理 | `pc_proxy/Ns_Pc_Agent.py` | FastAPI 代理中转、流式响应透传 |

### Agent 特性

- ✅ **多轮对话管理** — 完整 conversation history 维护
- ✅ **System Prompt 可配置** — 自定义 AI 角色和行为
- ✅ **异步 HTTP 请求** — `SDL_CreateThread` 后台请求，UI 不阻塞
- ✅ **手柄虚拟键盘** — D-Pad 导航 + A/B 选择 + 150ms 防抖
- ✅ **跨平台编译** — `#ifdef __SWITCH__` 条件编译，一套代码双平台
- ✅ **中文完整支持** — Noto Sans SC 字体 + UTF-8 渲染 + 退格正确处理

## 快速开始

### 环境要求

**Switch 端（C++）：**
- devkitPro + libnx（Switch 编译工具链）
- SDL2、SDL2_ttf、SDL2_image
- libcurl + nlohmann/json
- 或 Visual Studio 2022（Windows 调试）

**PC 端（Python 代理）：**
- Python 3.10+
- pip install -r pc_proxy/requirements.txt

### 1. 设置 API Key

```bash
# Windows (PowerShell)
$env:DEEPSEEK_API_KEY = "sk-你的密钥"

# Linux / macOS
export DEEPSEEK_API_KEY="sk-你的密钥"
```

> ⚠️ **永远不要把 API Key 写进源码或提交到 Git！**  
> 本项目已移除硬编码 Key，改用环境变量读取。

### 2. 启动 PC 代理

```bash
cd pc_proxy
pip install -r requirements.txt
python Ns_Pc_Agent.py
# 代理运行在 http://0.0.0.0:8888
```

### 3. 编译 & 运行 Switch 客户端

**Windows 调试（推荐先在这上面测试）：**
```bash
# 用 Visual Studio 打开 switch_app/Ns_Practice.sln
# 按 F5 编译运行
# Windows 上直接走 HTTPS，无需 PC 代理
```

**Switch 实机 / Yuzu 模拟器：**
```bash
cd switch_app
make -f Makefile
# 将生成的 Ns_Deep_Ai.nro 复制到 SD 卡 /switch/ 目录
# 修改 Ns_Practice.cpp 中代理 IP 为你的 PC 内网 IP（默认 192.168.1.100:8888）
```

## 操作说明

| 按键 | 功能 |
|------|------|
| **+** | 打开虚拟键盘 |
| **-** | 新建对话 |
| **B** | 退出 / 关闭键盘 |
| **D-Pad** | 虚拟键盘光标移动 |
| **A** | 选择字符 / SPACE / BACK / SEND |

**Windows 下：**
- 直接键盘输入
- Enter → 发送
- Esc → 退出
- N → 新建对话

## 项目结构

```
ns-ai-chat/
├── .gitignore                    # Git 排除规则（编译产物、机密文件）
├── .env.example                  # 环境变量模板
├── README.md
├── switch_app/                   # C++ 客户端
│   ├── Ns_Practice.sln           # Visual Studio 解决方案
│   ├── Ns_Practice.vcxproj       # VS 项目文件
│   ├── Ns_Practice.cpp           # 主程序入口
│   ├── DeepSeekClient.cpp        # DeepSeek API 客户端
│   ├── Deepseek_Running_Logite.cpp # 聊天 UI 渲染
│   ├── deepseek.hpp              # API 客户端头文件
│   ├── Deepseek_Running_Logite.hpp # UI 头文件
│   ├── Makefile                  # Switch (libnx) 编译
│   └── romfs/fonts/              # 中文字体资源
│       └── NotoSansSC-Regular.ttf
└── pc_proxy/                     # Python 代理
    ├── Ns_Pc_Agent.py            # FastAPI HTTP→HTTPS 代理
    └── requirements.txt          # Python 依赖
```

## 技术栈

| 层 | 技术 |
|----|------|
| 图形 & 输入 | SDL2 + SDL2_ttf + SDL2_image |
| 网络 | libcurl（C++ 端）/ httpx（Python 端） |
| JSON | nlohmann/json |
| 服务端 | FastAPI + uvicorn（Python） |
| 编译 | Visual Studio 2022 / devkitPro (libnx) |
| AI API | DeepSeek Chat Completions（OpenAI 兼容格式） |

## License

MIT — 随意使用、修改和分发。
