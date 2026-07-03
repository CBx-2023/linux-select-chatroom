# Linux Select Chatroom

这是《Linux 高级编程》课程设计项目：基于 C++、TCP Socket 和 `select` I/O 多路复用实现的多人聊天室。

项目包含一个 Linux 服务端和一个终端客户端。服务端监听客户端连接，维护在线用户状态，支持群聊广播、私聊、在线用户列表、正常退出、异常断开清理和运行日志记录。

## 快速开始

进入代码目录并编译：

```sh
cd code
make
```

启动服务端：

```sh
./server 8888
```

在多个终端启动客户端：

```sh
./client 127.0.0.1 8888 alice
./client 127.0.0.1 8888 bob
./client 127.0.0.1 8888 carol
```

清理构建产物：

```sh
make clean
```

## 客户端命令

| 输入 | 功能 |
| --- | --- |
| `hello everyone` | 发送群聊消息 |
| `/w bob hello` | 向 `bob` 发送私聊 |
| `/list` | 查看在线用户 |
| `/quit` | 退出聊天室 |

昵称长度为 1 到 20 个字符，只允许英文字母、数字和下划线。

## 项目结构

```text
.
├── code/                       # C++ 源码、测试、论文和运行说明
│   ├── include/                # 头文件
│   ├── src/                    # 服务端、客户端和公共模块实现
│   ├── tests/                  # C++ 测试程序
│   ├── Makefile                # 构建脚本
│   ├── README.md               # 代码目录内的详细运行说明
│   ├── course_paper.md         # 课程论文正文
│   └── validation_checklist.md # 手动验收清单
├── docs/                       # 设计文档和展示页面
└── issues/                     # CSV 任务拆分和 review 记录
```

## 核心设计

- 服务端使用单进程 `select` 监听一个 server socket 和多个 client socket。
- 客户端也使用 `select` 同时监听标准输入和服务端 socket。
- 通信协议是按行分隔的文本协议，客户端命令包括 `LOGIN`、`MSG`、`PRIVATE`、`LIST`、`QUIT`。
- 服务端响应包括 `OK`、`ERROR`、`SYSTEM`、`CHAT`、`PRIVATE`、`USERLIST`。
- 单条协议消息最大 1024 字节。
- 服务端通过 `server.log` 记录启动、登录、退出、异常断开、群聊、私聊和错误事件。

## 交付物

- 源码和构建脚本：[code/](code/)
- 详细运行说明：[code/README.md](code/README.md)
- 课程论文正文：[code/course_paper.md](code/course_paper.md)
- 设计文档：[docs/superpowers/specs/2026-06-22-chatroom-design.md](docs/superpowers/specs/2026-06-22-chatroom-design.md)
- 验收清单：[code/validation_checklist.md](code/validation_checklist.md)

## 验证

已完成的验证覆盖：

- `make -C code clean && make -C code`
- 协议解析、客户端状态管理、日志、显示格式化、输入转换测试
- 客户端连接和客户端 `select` 循环测试
- 服务端监听、登录生命周期、消息路由和错误处理测试
- 端到端聊天室行为验证记录

更多手动测试步骤见 [code/validation_checklist.md](code/validation_checklist.md)。
