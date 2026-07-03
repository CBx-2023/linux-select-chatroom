# 基于 TCP Socket 与 select 的多人聊天室系统

## 摘要

本课程设计实现了一个运行在 Linux 系统上的多人聊天室系统。系统采用 C++ 编写，基于 TCP Socket 完成客户端和服务端之间的可靠通信，服务端使用单进程 `select` I/O 多路复用模型同时管理多个客户端连接。客户端连接服务端后使用昵称登录，支持群聊消息广播、用户私聊、在线用户列表查询、正常退出、异常断开处理和服务端日志记录。

本项目重点体现 Linux 高级编程中的网络套接字、文件描述符、I/O 多路复用、文本协议设计、异常处理和资源清理。系统不依赖数据库和图形界面，所有在线状态均由服务端在内存中维护，关键运行事件写入 `server.log`，便于调试、测试和课程论文取证。

关键词：Linux；TCP Socket；select；多人聊天室；I/O 多路复用；C++

## 1. 课题背景与设计目标

《Linux 高级编程》课程设计要求自主设计一个运行在 Linux 系统上的软件，并利用 Socket 编程实现服务端和客户端。多人聊天室是典型的网络类应用：服务端需要监听客户端连接，接收某个客户端发送的消息，并将消息分发给其他客户端；客户端需要连接服务端，发送用户输入，并实时接收服务端转发的消息。

本项目选择“基于 TCP Socket 与 select 的多人聊天室系统”作为课程设计题目，主要目标如下：

1. 实现一个 TCP 聊天室服务端和一个 TCP 聊天室客户端。
2. 服务端使用单进程加 `select` 的 I/O 多路复用方式管理多个客户端连接。
3. 客户端登录时设置昵称，服务端维护在线用户列表。
4. 支持群聊消息广播。
5. 支持用户之间一对一私聊。
6. 支持查询当前在线用户。
7. 支持正常退出和异常断开处理。
8. 服务端记录登录、退出、异常断开、群聊、私聊和错误事件。

为控制课程设计复杂度，本系统不实现账号密码认证、不使用数据库、不实现离线消息、不实现历史消息查询、不实现图形界面，也不实现管理员权限和踢人功能。

## 2. 系统总体架构

系统由两个可执行程序组成：

- `server`：聊天室服务端。负责创建监听 Socket、绑定端口、接受客户端连接、维护在线用户状态、解析客户端命令、分发群聊和私聊消息，并将关键事件写入日志文件。
- `client`：聊天室客户端。负责连接服务端、发送登录命令、读取用户输入、发送聊天命令、接收服务端响应，并将服务端消息格式化显示在终端。

代码目录结构如下：

```text
code/
  Makefile
  README.md
  course_paper.md
  course_paper_outline.md
  validation_checklist.md
  include/
    client.h
    client_info.h
    client_manager.h
    display.h
    input_handler.h
    logger.h
    protocol.h
    server.h
  src/
    client.cpp
    client_manager.cpp
    display.cpp
    input_handler.cpp
    logger.cpp
    protocol.cpp
    server.cpp
  tests/
    ...
```

系统采用分层设计：

- 协议层：`protocol.h` / `protocol.cpp` 定义客户端命令、服务端响应、昵称规则、消息长度限制和按行拆包缓冲。
- 服务端状态层：`ClientManager` 维护客户端 fd、昵称和登录状态。
- 服务端业务层：`ChatServer` 负责监听连接、调用 `select`、处理命令、转发消息和清理连接。
- 日志层：`Logger` 将关键事件追加写入 `server.log`。
- 客户端交互层：`ChatClient` 负责网络连接和事件循环，`InputHandler` 负责用户输入到协议命令的转换，`Display` 负责服务端消息的终端显示。

本文档是可直接整理为 Word/PDF 的 Markdown 论文正文。文中以“图 N 建议截取”标明截图插入位置；提交正式论文时，应按附录截图清单插入真实代码截图和运行截图。

图 1 建议截取最终 `code/` 目录，用于展示项目结构。

## 3. TCP Socket 通信原理

TCP 是面向连接、可靠、有序的字节流协议。聊天室系统要求消息可靠到达，并且客户端与服务端之间需要保持长连接，因此选择 TCP 作为传输层协议。

服务端启动流程如下：

1. 调用 `socket(AF_INET, SOCK_STREAM, 0)` 创建 TCP Socket。
2. 调用 `setsockopt` 设置 `SO_REUSEADDR`，降低端口复用时的调试成本。
3. 调用 `bind` 将 Socket 绑定到指定端口。
4. 调用 `listen` 进入监听状态。
5. 当监听 fd 可读时，调用 `accept` 接收新客户端连接。

服务端关键代码位置为 `src/server.cpp`：

```cpp
listen_fd_ = socket(AF_INET, SOCK_STREAM, 0);
setsockopt(listen_fd_, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
bind(listen_fd_, reinterpret_cast<sockaddr*>(&address), sizeof(address));
listen(listen_fd_, kListenBacklog);
```

客户端启动流程如下：

1. 解析命令行参数：服务端 IP、端口和昵称。
2. 调用 `socket` 创建 TCP Socket。
3. 使用 `inet_pton` 将 IP 字符串转换为网络地址。
4. 调用 `connect` 连接服务端。
5. 连接成功后立即发送 `LOGIN <nickname>`。

客户端关键代码位置为 `src/client.cpp`：

```cpp
socket_fd_ = socket(AF_INET, SOCK_STREAM, 0);
server_addr.sin_port = htons(static_cast<uint16_t>(options.port));
inet_pton(AF_INET, options.server_ip.c_str(), &server_addr.sin_addr);
connect(socket_fd_, reinterpret_cast<sockaddr*>(&server_addr), sizeof(server_addr));
send_all("LOGIN " + options.nickname + "\n");
```

需要注意的是，TCP 提供的是字节流，而不是消息边界。一次 `recv` 可能读到半条消息，也可能读到多条消息连在一起。因此本系统使用换行符作为协议消息结束标志，并在每个连接上维护接收缓冲区，用来处理 TCP 半包和粘包。

## 4. select I/O 多路复用设计

如果服务端为每个客户端创建一个线程，虽然实现直观，但线程数量会随连接数增加而增长，代码同步和资源管理也会变复杂。本项目采用单进程 `select` 模型，所有客户端 Socket 都作为 Linux 文件描述符加入 `fd_set`，服务端在一个事件循环中统一处理连接和消息。

服务端 `select` 主循环的核心步骤如下：

1. 清空并重新构造 `fd_set`。
2. 将监听 fd 加入集合。
3. 将所有客户端 fd 加入集合。
4. 计算最大 fd。
5. 调用 `select(max_fd + 1, &read_fds, nullptr, nullptr, timeout)`。
6. 如果监听 fd 可读，说明有新连接，调用 `accept`。
7. 如果客户端 fd 可读，说明有消息或断开事件，调用 `recv` 处理。

关键代码位于 `src/server.cpp` 的 `ChatServer::run_once`：

```cpp
FD_ZERO(&read_fds);
FD_SET(listen_fd_, &read_fds);

for (int fd : clients_.fds()) {
    FD_SET(fd, &read_fds);
    if (fd > max_fd) {
        max_fd = fd;
    }
}

int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, timeout_ptr);
```

客户端也使用 `select`，同时监听标准输入 fd 和服务端 Socket fd。这样客户端既能读取用户输入，又能及时显示其他用户发来的消息，不需要创建额外线程。

客户端关键代码位于 `src/client.cpp` 的 `ChatClient::run_event_loop`：

```cpp
FD_ZERO(&read_fds);
FD_SET(socket_fd_, &read_fds);
FD_SET(input_fd, &read_fds);

int max_fd = socket_fd_ > input_fd ? socket_fd_ : input_fd;
int ready = select(max_fd + 1, &read_fds, nullptr, nullptr, nullptr);
```

图 2 建议截取服务端 `select` 循环代码。图 3 建议截取客户端 `select` 循环代码。

## 5. 通信协议设计

客户端和服务端使用基于文本行的简单协议，每条协议消息以换行符结束。文本协议便于调试，也便于使用终端或简单脚本模拟客户端请求。

客户端发送命令如下：

```text
LOGIN <nickname>
MSG <content>
PRIVATE <target_nickname> <content>
LIST
QUIT
```

服务端返回或广播消息如下：

```text
OK <message>
ERROR <reason>
SYSTEM <message>
CHAT <nickname> <content>
PRIVATE <from_nickname> <content>
USERLIST <nickname1>,<nickname2>,...
```

协议约束如下：

- 客户端连接后必须先发送 `LOGIN <nickname>`。
- 昵称长度为 1 到 20 个字符，只允许英文字母、数字和下划线。
- 单条协议消息最大长度为 1024 字节。
- 未登录客户端只能发送 `LOGIN` 或 `QUIT`。
- `MSG` 表示群聊消息。
- `PRIVATE` 表示私聊消息。
- `LIST` 表示查询在线用户列表。
- `QUIT` 表示主动退出聊天室。

协议解析函数位于 `src/protocol.cpp`。当解析失败时，函数返回 `CommandType::Invalid`，并在 `error` 字段中保存错误原因。服务端再根据错误原因生成 `ERROR <reason>` 响应。

例如，昵称合法性检查如下：

```cpp
bool is_valid_nickname(const std::string& nickname) {
    if (nickname.empty() || nickname.size() > kMaxNicknameLength) {
        return false;
    }
    for (char c : nickname) {
        if (c != '_' && !is_ascii_letter_or_digit(c)) {
            return false;
        }
    }
    return true;
}
```

按行拆包由 `LineBuffer` 完成。它将新读到的字节追加到内部缓冲区，遇到换行符时输出完整消息，未遇到换行符的部分继续保留，等待下一次 `recv`。

图 4 建议截取 `protocol.h` 中的协议类型定义，图 5 建议截取 `parse_client_command` 和 `LineBuffer::append` 的实现。

## 6. 服务端实现

### 6.1 客户端状态管理

服务端使用 `ClientManager` 管理在线客户端。每个客户端用 `ClientInfo` 表示，包含：

- `fd`：客户端 Socket 文件描述符。
- `nickname`：登录后的昵称。
- `logged_in`：是否已经登录。
- `receive_buffer`：按行接收缓冲区。

`ClientManager` 使用两个映射结构：

- `clients_by_fd_`：根据 fd 查找客户端信息。
- `fd_by_nickname_`：根据昵称查找 fd，用于私聊和重复昵称检测。

这样设计的好处是：服务端在处理 Socket 可读事件时可以通过 fd 快速定位客户端，在处理私聊时可以通过昵称快速定位目标用户。

### 6.2 登录与退出处理

客户端连接后先处于未登录状态。服务端只允许未登录客户端发送 `LOGIN` 或 `QUIT`。收到 `LOGIN` 后，服务端检查昵称是否重复，并调用 `ClientManager::login` 写入昵称和登录状态。

登录成功后，服务端返回：

```text
OK login successful
```

并向其他已登录用户广播：

```text
SYSTEM <nickname> online
```

当客户端发送 `QUIT` 时，服务端关闭对应 fd，从 `ClientManager` 中删除客户端，写入退出日志，并向其他用户广播下线通知。若 `recv` 返回 0 或发生不可恢复错误，服务端按异常断开处理，同样清理状态和广播通知。

### 6.3 群聊、私聊和在线列表

群聊处理流程如下：

1. 服务端确认发送者已经登录。
2. 使用 `make_chat(sender, content)` 生成 `CHAT` 消息。
3. 遍历所有已登录客户端。
4. 跳过发送者自己。
5. 将消息发送给其他在线用户。
6. 写入群聊日志。

私聊处理流程如下：

1. 服务端确认发送者已经登录。
2. 根据目标昵称查找目标客户端。
3. 若目标不存在，向发送者返回 `ERROR user not online`。
4. 若目标存在，仅向目标客户端发送 `PRIVATE <from> <content>`。
5. 写入私聊日志。

在线列表处理较简单：服务端调用 `clients_.online_users()` 生成昵称数组，再通过 `make_userlist` 返回给请求者。在线列表只返回给请求者，不广播给其他用户。

### 6.4 错误处理和资源清理

服务端需要处理多种异常情况：

- 昵称为空：返回 `ERROR nickname required`。
- 昵称非法：返回 `ERROR invalid nickname`。
- 昵称重复：返回 `ERROR nickname already used`。
- 未登录发送聊天命令：返回 `ERROR please login first`。
- 私聊目标不存在：返回 `ERROR user not online`。
- 命令格式错误：返回 `ERROR invalid command`。
- 消息超过 1024 字节：返回 `ERROR message too long`。
- 客户端异常断开：关闭 fd、删除状态、记录日志、广播下线通知。
- `send` 或 `recv` 失败：记录错误并清理受影响连接，不影响其他客户端。

服务端发送消息时使用 `send_response`，它会循环调用 `send`，直到整条响应都发送完成，避免一次 `send` 未发送完整数据的问题。

### 6.5 服务端日志

`Logger` 将关键事件追加写入 `server.log`。日志事件包括：

- `STARTUP`：服务端启动端口。
- `LOGIN`：用户登录。
- `QUIT`：用户正常退出。
- `ABNORMAL_DISCONNECT`：用户异常断开。
- `GROUP_CHAT`：群聊消息。
- `PRIVATE_CHAT`：私聊消息。
- `ERROR`：重要错误。

日志写入失败时，`Logger` 不抛出异常，而是在标准错误输出提示。这样即使日志文件暂时不可写，服务端也不会因为日志问题直接崩溃。

图 6 建议截取 `ChatServer::process_client_line`、`handle_group_message`、`handle_private_message` 和 `handle_disconnect` 的关键代码。图 7 建议截取 `Logger` 的日志写入代码。

## 7. 客户端实现

### 7.1 启动和登录

客户端启动命令格式为：

```sh
./client 127.0.0.1 8888 alice
```

客户端从命令行读取服务端 IP、端口和昵称。参数合法后创建 TCP Socket，并连接服务端。连接成功后，客户端立即发送：

```text
LOGIN alice
```

如果服务端返回 `OK`，客户端进入交互循环；如果服务端返回 `ERROR` 或关闭连接，客户端显示失败原因并退出。

### 7.2 用户输入转换

客户端通过 `InputHandler` 将用户输入转换为协议命令：

| 用户输入 | 协议命令 |
| --- | --- |
| `hello everyone` | `MSG hello everyone` |
| `/w bob hello` | `PRIVATE bob hello` |
| `/list` | `LIST` |
| `/quit` | `QUIT` |

空输入会被忽略。`/w` 命令缺少目标或消息内容时，客户端在本地提示格式错误，不向服务端发送非法命令。

### 7.3 消息显示

服务端返回的是协议文本，不适合直接展示给普通用户。客户端使用 `Display` 将服务端消息转换为更容易阅读的终端格式：

| 服务端消息 | 终端显示 |
| --- | --- |
| `SYSTEM alice online` | `[system] alice online` |
| `CHAT alice hello` | `[group] alice: hello` |
| `PRIVATE bob hi` | `[private] bob: hi` |
| `USERLIST alice,bob` | `Online users: alice,bob` |
| `OK login successful` | `OK: login successful` |
| `ERROR user not online` | `Error: user not online` |

图 8 建议截取 `InputHandler::parse` 和 `Display::format_server_line` 的关键代码。

## 8. 编译与运行方法

项目提供 `Makefile`，在 `code/` 目录下执行：

```sh
make
```

编译后生成：

```text
server
client
```

启动服务端：

```sh
./server 8888
```

在不同终端启动多个客户端：

```sh
./client 127.0.0.1 8888 alice
./client 127.0.0.1 8888 bob
./client 127.0.0.1 8888 carol
```

客户端进入交互循环后，可以直接输入文本发送群聊，也可以使用 `/w`、`/list`、`/quit` 命令。

图 9 建议截取 `make` 编译成功截图。图 10 建议截取服务端启动和多个客户端登录截图。

## 9. 测试过程与结果

本项目包含两类验证：

1. 自动化测试：位于 `code/tests/`，覆盖协议解析、客户端状态管理、服务端监听、登录生命周期、消息路由、错误处理、客户端输入转换和显示格式化。
2. 手动端到端测试：按照 `validation_checklist.md` 启动服务端和多个客户端，验证真实聊天流程。

### 9.1 自动化测试覆盖

主要测试文件如下：

- `protocol_tests.cpp`：验证换行拆包、昵称规则、命令解析、错误返回、1024 字节限制和响应生成。
- `client_manager_tests.cpp`：验证客户端添加、登录、重复昵称检测、查找、在线列表和删除。
- `server_listener_tests.cpp`：验证服务端监听端口并接受多个客户端连接。
- `server_lifecycle_tests.cpp`：验证登录、重复昵称、非法昵称、未登录命令、正常退出和异常断开。
- `server_routing_tests.cpp`：验证群聊广播、私聊只到目标用户、在线列表只返回请求者和聊天日志。
- `server_hardening_tests.cpp`：验证非法命令、超长消息、send/recv 失败时服务端仍可用。
- `client_connection_tests.cpp`：验证客户端参数解析、连接、登录成功和登录失败。
- `client_select_loop_tests.cpp`：验证客户端能同时处理用户输入和服务端消息。
- `input_handler_tests.cpp`：验证普通文本、`/w`、`/list`、`/quit` 的转换。
- `display_tests.cpp`：验证服务端响应的终端显示格式。
- `logger_tests.cpp`：验证日志事件写入和日志失败时的降级处理。

图 11 建议截取自动化测试运行结果。

### 9.2 手动端到端测试

手动测试步骤如下：

1. 执行 `make` 编译服务端和客户端。
2. 启动 `./server 8888`。
3. 启动三个客户端：`alice`、`bob`、`carol`。
4. 在 `alice` 输入普通文本，确认 `bob` 和 `carol` 收到群聊消息。
5. 在 `alice` 输入 `/w bob hello`，确认只有 `bob` 收到私聊消息。
6. 在客户端输入 `/list`，确认返回在线用户列表。
7. 再启动一个昵称为 `alice` 的客户端，确认重复昵称被拒绝。
8. 发送非法命令，确认服务端返回错误且继续运行。
9. 发送超过 1024 字节的消息，确认返回 `ERROR message too long`。
10. 输入 `/quit`，确认用户退出并广播下线通知。
11. 直接关闭某个客户端终端，确认服务端检测异常断开并清理状态。
12. 查看 `server.log`，确认日志中包含启动、登录、退出、异常断开、群聊、私聊和错误事件。

本次论文整理前重新执行了 `make -C code clean && make -C code`，服务端和客户端均完成编译。随后重新编译并运行 `protocol_tests`、`client_manager_tests`、`logger_tests`、`display_tests`、`input_handler_tests`、`client_connection_tests`、`client_select_loop_tests`、`server_listener_tests`、`server_lifecycle_tests`、`server_routing_tests` 和 `server_hardening_tests`，测试进程均以退出码 0 结束。集成验收记录还显示，已检查完整 Linux C++ 聊天室项目、全部模块 CSV、构建产物、协议/服务端/客户端行为、交付文档和端到端证据。对于最终提交论文，应将上述手动测试过程截图插入本节对应位置。

图 12 建议截取三客户端群聊截图。图 13 建议截取私聊只到目标用户的截图。图 14 建议截取 `/list` 输出截图。图 15 建议截取重复昵称和超长消息错误截图。图 16 建议截取 `server.log`。

## 10. 关键代码说明

### 10.1 协议解析

协议解析是服务端正确处理客户端请求的基础。`parse_client_command` 先去除可选的 `\r`，再检查消息长度是否超过 1024 字节，之后根据第一个空格拆分命令字和参数。对于不同命令，解析函数分别检查参数是否完整，并返回结构化的 `Command`。

这种设计的优点是服务端业务逻辑不需要直接处理字符串细节，只需要判断 `CommandType` 并读取对应字段。

### 10.2 服务端事件循环

服务端每次进入 `run_once` 都重新构造 `fd_set`。这是因为 `select` 会修改传入的集合，不能复用上一次调用后的集合。监听 fd 和所有客户端 fd 都加入集合后，服务端调用 `select` 阻塞等待事件。若监听 fd 可读，则接收新连接；若客户端 fd 可读，则读取该客户端数据。

这种模型避免了为每个客户端创建线程，也能体现 Linux 文件描述符和 I/O 多路复用思想。

### 10.3 消息路由

群聊消息通过遍历在线客户端实现。服务端跳过发送者自己，只向其他已登录用户发送 `CHAT` 响应。私聊消息通过昵称查找目标客户端，目标不存在时向发送者返回错误；目标存在时只向目标 fd 发送 `PRIVATE` 响应。

这种分发方式使群聊、私聊和在线列表三类业务边界清晰，便于测试。

### 10.4 客户端交互

客户端使用 `select` 同时监听标准输入和服务端 Socket。用户输入普通文本时，`InputHandler` 转换成 `MSG`；输入 `/w` 时转换成 `PRIVATE`；输入 `/list` 时转换成 `LIST`；输入 `/quit` 时转换成 `QUIT` 并退出循环。服务端发来的协议消息由 `Display` 转成带标签的终端文本。

## 11. 总结与改进方向

本课程设计完成了一个基于 Linux TCP Socket 和 `select` 的多人聊天室系统。系统通过文本协议实现客户端和服务端协作，通过 `ClientManager` 维护在线用户状态，通过服务端消息路由逻辑完成群聊、私聊和在线列表，通过 `Logger` 记录运行事件，并通过客户端 `select` 循环实现用户输入和服务端消息的并发处理。

本项目的主要收获包括：

1. 掌握了 TCP Socket 服务端的创建、绑定、监听和连接接收流程。
2. 理解了 TCP 是字节流协议，需要应用层自己处理消息边界。
3. 掌握了 `select` 同时监听多个文件描述符的基本用法。
4. 理解了服务端并发连接管理、异常断开处理和资源清理的重要性。
5. 通过协议层、状态管理层、服务端业务层和客户端交互层的划分，提高了代码可读性和可测试性。

后续可以在不改变核心架构的基础上扩展以下功能：

- 增加账号密码认证。
- 使用数据库保存用户和聊天记录。
- 支持历史消息查询。
- 增加图形界面客户端。
- 增加管理员管理功能。
- 将 `select` 替换为更适合大量连接的 `epoll`。

这些扩展均超出本次课程设计范围，因此本文只作为未来改进方向说明。

## 附录 A：论文截图清单

提交 Word 或 PDF 版本时，应将以下截图插入正文对应位置：

1. `code/` 目录结构截图。
2. `make` 编译成功截图。
3. 服务端启动截图。
4. 三个客户端登录截图。
5. 群聊广播截图。
6. 私聊只到目标用户截图。
7. `/list` 在线用户列表截图。
8. 重复昵称拒绝截图。
9. 非法命令或超长消息错误截图。
10. `/quit` 和异常断开处理截图。
11. `server.log` 日志截图。
12. 协议解析、服务端 `select` 循环、消息路由、客户端输入处理等关键代码截图。

## 附录 B：主要文件索引

| 文件 | 作用 |
| --- | --- |
| `include/protocol.h` / `src/protocol.cpp` | 协议常量、命令解析、响应生成、按行缓冲 |
| `include/server.h` / `src/server.cpp` | 服务端监听、`select` 循环、命令处理、消息路由 |
| `include/client_manager.h` / `src/client_manager.cpp` | 在线客户端状态管理 |
| `include/logger.h` / `src/logger.cpp` | 服务端日志记录 |
| `include/client.h` / `src/client.cpp` | 客户端连接、登录和交互事件循环 |
| `include/input_handler.h` / `src/input_handler.cpp` | 用户输入到协议命令的转换 |
| `include/display.h` / `src/display.cpp` | 服务端响应的终端显示格式化 |
| `tests/` | 协议、服务端、客户端和日志的自动化测试 |
