# LingChat-C++-Termux
   本应用现仅在termux环境中编译和运行过，其他linux环境请自行修改makefile中的库名，以及依赖库、库的路径等  
   本项目是LingChat的二创，有兴趣的可以前往LingChat的仓库(https://github.com/SlimeBoyOwO/LingChat)  
   欢迎前往博客获得更好的观感[博客链接](https://yun-lover.github.io/2025/08/06/first/)
## 一、编译和运行
### 1.工具链安装
   首先我们需要安装编译的工具链以及依赖库
``` bash
pkg install curl libcurl clang make openssl nlohmann-json git
```
### 2.克隆仓库以获取文件
```bash
git clone https://github.com/yun-helpr/LingChat-C--Termux.git
```
这里我们可以重命名一下文件夹
```bash
mv LingChat-C--Termux chat
```
### 3.编译
   由于一些原因，我这里并不能提供发行版，所以请自行编译
   至于如何编译，仓库里有makefile,所以只需
```bash
cd chat
make
```
   我在makefile里加了依赖库验证，所以没有装的会提醒  
   那么终端则会输出
```terminal
~/chat $ make
arm-linux-androideabi-clang++ -std=c++17 -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -MMD -MP -MF build/dep/AIEngine.d -c backend/src/AIEngine.cpp -o build/obj/AIEngine.o
arm-linux-androideabi-clang++ -std=c++17 -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -MMD -MP -MF build/dep/ConfigManager.d -c backend/src/ConfigManager.cpp -o build/obj/ConfigManager.o
arm-linux-androideabi-clang++ -std=c++17 -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -MMD -MP -MF build/dep/HTTPClient.d -c backend/src/HTTPClient.cpp -o build/obj/HTTPClient.o
arm-linux-androideabi-clang++ -std=c++17 -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -MMD -MP -MF build/dep/Logger.d -c backend/src/Logger.cpp -o build/obj/Logger.o
arm-linux-androideabi-clang++ -std=c++17 -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -MMD -MP -MF build/dep/SessionManager.d -c backend/src/SessionManager.cpp -o build/obj/SessionManager.o
arm-linux-androideabi-clang++ -std=c++17 -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -MMD -MP -MF build/dep/WebSocketServer.d -c backend/src/WebSocketServer.cpp -o build/obj/WebSocketServer.o
arm-linux-androideabi-clang++ -std=c++17 -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -MMD -MP -MF build/dep/main.d -c backend/src/main.cpp -o build/obj/main.o
cc -Ibackend/include -Wall  -O0 -g3 -DDEBUG -I/data/data/com.termux/files/usr/include -DOPENSSL_API_3_0 -DUSE_WEBSOCKET -MMD -MP -MF build/dep/civetweb.d -c backend/src/civetweb.c -o build/obj/civetweb.o
arm-linux-androideabi-clang++ -L/data/data/com.termux/files/usr/lib build/obj/AIEngine.o build/obj/ConfigManager.o build/obj/HTTPClient.o build/obj/Logger.o build/obj/SessionManager.o build/obj/WebSocketServer.o build/obj/main.o build/obj/civetweb.o -lcurl -lssl -lcrypto -lpthread -o build/bin/backend_server
~/chat $
```
### 4.修改.env配置文件
   你需要修改api端点及key才能使用,我添加了RAG系统，让ai有了永久记忆，可在env中配置(embedding模型的配置和RAG系统的启用，默认关闭)
```env
# ==================================================
#                      配置文件
# ==================================================

[Server]
BACKEND_PORT="8765" 
DOCUMENT_ROOT="frontend"

[API]
API_BASE_URL="https://api.deepseek.com/v1" #这里填写api端点
DEEPSEEK_API_KEY="your api key" # 这里填写你的apikey
VOICE_API_URL="" #这里原本是打算做一个语音合成，但手机合成太慢了，所以废弃了。
EMBEDDING_MODEL = "text-embedding-v2" #这里填写embedding模型的名称，未开启RAG的可以不填，这里的模型名称并不是真实可调用的
EMBEDDING_API_URL = "https://api.deepseek.com/v1"  # 这里填写embedding的api端点，未开启RAG的可以不填，这里的api端点也并不是真实可调用的
EMBEDDING_VECTOR_DIMENSION = "1024" # 这里填写embedding模型的向量的维度，这个由模型决定，请自行查询自己所使用的embedding模型的向量的维度，这里的向量的维度并不是真实的

[Database]
# 轻量级RAG的记忆存储文件，它将自动被创建
MEMORY_DB_PATH = "memory.json"

[AI]
MODEL="deepseek-chat" # 这里填写你所调用的模型名称
TEMPERATURE=0.7 # 这里填写的数值表示模型思维的发散程度，越低发散程度越高，反之亦然。
MAX_HISTORY_TURNS="10" # 这里则数值则表示模型的记忆长度，由于目前市面上绝大多数大模型api都是无状态的，所以我们每次调用模型都需要将上文一同告诉模型，但这样太费token,所以要加以限制，所以模型只会记得包括你这句话的前十句话，但不包括RAG系统。
ENABLE_RAG = false # 这里控制RAG的开关


[Character]
CHARACTER_NAME="钦灵" # 这里是ai的名称，具体如何体现请看web界面
CHARACTER_IDENTITY="可爱的狼娘" # 这里是ai的身份，具体如何体现请看web界面

[UI]
BACKGROUND_DAY_PATH="assets/bg/白天.jpeg" # 这里是白天的背景，当然你也可以换成别的
BACKGROUND_NIGHT_PATH="assets/bg/夜晚.jpg" # 这里是夜晚的背景，同样你也可以换成别的
CHARACTER_SPRITE_DIR="assets/char/ling/" # 这里是人物立绘的文件夹，具体作用可见下文的立绘情绪标签映射表
SFX_DIR="assets/sfx/" #这里是音效文件夹，具体作用可见下文的音效情绪标签映射表
# 注意，所有包含前端资源的文件夹均在frontend中

[EmotionMap] # 这里是立绘情绪标签映射表，你可以将assets/char/ling/中的文件名针对该映射表进行修改(不包括后缀)
高兴=兴奋 # 这里实际在文件夹中的文件名应当是"兴奋.png",下面一样，英文的是我懒惰改了
害羞=害羞
生气=厌恶
无语=speechless
惊讶=surprised
哭泣=sad
担心=sad
情动=love
调皮=playful
认真=neutral
疑惑=neutral
尴尬=shy
紧张=sad
自信=happy
害怕=sad
厌恶=angry
羞耻=shy
旁白=兴奋 # 旁边讲话时的立绘，我不想做多人物了
default=兴奋 # 当情绪标签无法识别时默认返回的立绘

[EmotionSfxMap] # 这里是音效情绪标签映射表，工作逻辑同上，文件夹是assets/sfx/
高兴=喜爱.wav
害羞=喜爱.wav
生气=喜爱.wav

[SystemPrompt] # 这里是ai的系统提示词，你可以在项目的根目录下的prompt.txt中修改
PROMPT_FILE="prompt.txt"
    
```
### 5.运行
   当你编译好之后，请执行以下命令
```bash
make run
```

   便可以启动服务了，如果所有的流程都对的话，那么终端应该输出    
```terminal
~/chat $ ./backend_server
[信息] CivetWeb 库已初始化 (SSL+WebSocket)。
[信息] ConfigManager: 已成功加载并解析配置文件: .env
[信息] HTTPClient 已初始化 (无SSL初始化)。
[信息] AIEngine 已初始化，模型: deepseek-chat, API URL: https://api.deepseek.com/v1
[信息] SessionManager 已初始化，最大对话历史记录: 10 轮
[信息] WebSocketServer: WebSocketServer 已初始化。
[信息] WebSocketServer: 正在启动 WebSocket 服务器于 8765...
[信息] WebSocketServer: 正在从以下目录提供静态文件: /data/data/com.termux/files/home/chat/frontend
[信息] WebSocketServer: WebSocket 服务器启动成功。
[信息] WebSocketServer: WebSocket 端点已注册: /websocket
[信息] 服务器已成功启动。主线程进入等待模式。按 Ctrl+C 关闭。
```

   接下来就是访问web界面了，在浏览器的地址栏输入 http://localhost:8765/ ,那么你就可以看到前端并进行对话了   
   那么此时termux终端会输出日志
```terminal
~/chat $ ./backend_server
[信息] CivetWeb 库已初始化 (SSL+WebSocket)。
[信息] ConfigManager: 已成功加载并解析配置文件: .env
[信息] HTTPClient 已初始化 (无SSL初始化)。
[信息] AIEngine 已初始化，模型: deepseek-chat, API URL: https://api.deepseek.com/v1
[信息] SessionManager 已初始化，最大对话历史记录: 10 轮
[信息] WebSocketServer: WebSocketServer 已初始化。
[信息] WebSocketServer: 正在启动 WebSocket 服务器于 8765...
[信息] WebSocketServer: 正在从以下目录提供静态文件: /data/data/com.termux/files/home/chat/frontend
[信息] WebSocketServer: WebSocket 服务器启动成功。
[信息] WebSocketServer: WebSocket 端点已注册: /websocket
[信息] 服务器已成功启动。主线程进入等待模式。按 Ctrl+C 关闭。
[信息] WebSocketServer: 新 WebSocket 连接请求已授权，等待就绪...
[信息] WebSocketServer: WebSocket 连接已就绪，并已清空会话历史。
[信息] WebSocketServer: 发送 WebSocket 数据: {"payload":{"character_identity":"可爱的狼娘","character_name":"灵","ui_config":{"background_day":"assets/bg/白天.jpeg","background_night":"assets/bg/夜晚.jpg","character_sprite_dir":"assets...
```
   注意，可能会输出一些类似   
    *** 1753620119.065422618 2925130916 mg_start2:20630: [listening_ports] -> [8765]    
   这样的日志，可以忽略，有能力的可以看
### 6.关闭
   这个只要用过电脑的应该都会，你只需要同时按下ctrl+c即可停止程序，终端则会输出
```terminal
~/chat $ ./backend_server
[信息] CivetWeb 库已初始化 (SSL+WebSocket)。
[信息] ConfigManager: 已成功加载并解析配置文件: .env
[信息] HTTPClient 已初始化 (无SSL初始化)。
[信息] AIEngine 已初始化，模型: deepseek-chat, API URL: https://api.deepseek.com/v1
[信息] SessionManager 已初始化，最大对话历史记录: 10 轮
[信息] WebSocketServer: WebSocketServer 已初始化。
[信息] WebSocketServer: 正在启动 WebSocket 服务器于 8765...
[信息] WebSocketServer: 正在从以下目录提供静态文件: /data/data/com.termux/files/home/chat/frontend
[信息] WebSocketServer: WebSocket 服务器启动成功。
[信息] WebSocketServer: WebSocket 端点已注册: /websocket
[信息] 服务器已成功启动。主线程进入等待模式。按 Ctrl+C 关闭。
[信息] WebSocketServer: 新 WebSocket 连接请求已授权，等待就绪...
[信息] WebSocketServer: WebSocket 连接已就绪，并已清空会话历史。
[信息] WebSocketServer: 发送 WebSocket 数据: {"payload":{"character_identity":"可爱的狼娘","character_name":"灵","ui_config":{"background_day":"assets/bg/白天.jpeg","background_night":"assets/bg/夜晚.jpg","character_sprite_dir":"assets...
^C
[信息] 收到停止信号，准备关闭服务器...
[信息] 服务器主循环已退出，正在清理...
[信息] WebSocketServer: 正在停止 WebSocket 服务器...
[信息] WebSocketServer: WebSocket 连接正在关闭 (ID: 2832217416): /websocket
[信息] WebSocketServer: 活跃连接已清空。服务器现在可以接受新连接。
[信息] WebSocketServer: WebSocket 服务器已停止。
[信息] WebSocketServer: WebSocketServer 已销毁。
[信息] 程序已优雅关闭。
~/chat $
```
   好了，到此为止这个小程序就运行起来了   
   由于代码是ai写的，尽管我进行了大量测试，但肯定还是有bug,如正则表达式匹配失效，输出日文(这也是为了ai语音合成，但也废弃了，我懒的删提示词和正则了)，如果有bug,可以先去问ai,搞不定了再问大佬(不要问我，因为我也半斤八两)   
   仓库中你可能会看到backend_server这个文件，那是我编译的，你可以选择试着运行，如果报错，还请自行编译   
   
## 二、文件结构
```文件结构
~/chat $ tree
.
├── backend
│   ├── include
│   │   ├── AIEngine.hpp
│   │   ├── ConfigManager.hpp
│   │   ├── HTTPClient.hpp
│   │   ├── Logger.hpp
│   │   ├── MemoryManager.hpp
│   │   ├── SessionManager.hpp
│   │   ├── WebSocketServer.hpp
│   │   ├── civetweb.c
│   │   └── civetweb.h
│   └── src
│       ├── AIEngine.cpp
│       ├── ConfigManager.cpp
│       ├── HTTPClient.cpp
│       ├── Logger.cpp
│       ├── MemoryManager.cpp
│       ├── SessionManager.cpp
│       ├── WebSocketServer.cpp
│       ├── civetweb.c
│       ├── handle_form.inl
│       ├── http2.inl
│       ├── main.cpp
│       ├── match.inl
│       ├── md5.inl
│       ├── mod_duktape.inl
│       ├── mod_lua.inl
│       ├── mod_lua_shared.inl
│       ├── mod_mbedtls.inl
│       ├── mod_zlib.inl
│       ├── openssl_dl.inl
│       ├── response.inl
│       ├── sha1.inl
│       ├── sort.inl
│       ├── timer.inl
│       └── wolfssl_extras.inl
├── backend_server
├── build
│   ├── dep
│   │   ├── AIEngine.d
│   │   ├── ConfigManager.d
│   │   ├── HTTPClient.d
│   │   ├── Logger.d
│   │   ├── MemoryManager.d
│   │   ├── SessionManager.d
│   │   ├── WebSocketServer.d
│   │   ├── civetweb.d
│   │   └── main.d
│   └── obj
│       ├── AIEngine.o
│       ├── ConfigManager.o
│       ├── HTTPClient.o
│       ├── Logger.o
│       ├── MemoryManager.o
│       ├── SessionManager.o
│       ├── WebSocketServer.o
│       ├── civetweb.o
│       └── main.o
├── frontend
│   ├── app.js
│   ├── assets
│   │   ├── bg
│   │   │   ├── 夜晚.jpg
│   │   │   └── 白天.jpeg
│   │   ├── char
│   │   │   └── ling
│   │   │       ├── 伤心.png
│   │   │       ├── 兴奋.png
│   │   │       ├── 厌恶.png
│   │   │       ├── 头像.png
│   │   │       ├── 害怕.png
│   │   │       └── 害羞.png
│   │   └── sfx
│   │       └── 喜爱.wav
│   ├── index.html
│   └── style.css
├── makefile
├── memory.json
├── prompt.txt
├── readme.md
└── 文件结构.txt

13 directories, 69 files
~/chat $
```
