# MyWebServer

轻量级、高性能的 C++20 HTTP/1.1 服务器，实现基于 Reactor 的事件驱动模型，支持静态资源分发与简单动态业务。

## 项目简介

MyWebServer 通过 `epoll` + 非阻塞套接字实现单进程多连接的高并发 I/O，将连接的读写事件分发到线程池处理，同时用最小堆定时器管理长连接超时。项目默认提供一组静态网页，并通过 SQLite 连接池实现用户注册 / 登录演示。

## 功能亮点

- **事件驱动内核**：监听与客户端套接字均为非阻塞 fd，可按需配置 LT/ET 触发模式，保证主循环不会被慢客户端拖垮。
- **线程池请求处理**：Reactor 线程只负责收发事件，业务逻辑投递到固定规模线程池，兼顾延迟与吞吐。
- **连接生命周期管理**：最小堆定时器按访问时间刷新，主动清理超时长连接，保持资源可控。
- **HTTP 协议支持**：自研的 `HTTPRequest`/`HTTPResponse` 组件完成请求解析、响应拼装，下载文件通过 `mmap` 零拷贝写回。
- **数据库接入**：内置 SQLite 连接池，读写分离（写连接 + 多个只读连接），默认使用 `user` 表演示表单校验。
- **异步日志**：可切换同步/异步写入，支持日志轮转与队列刷盘，便于线上排障。

## 模块组成

- `src/server`：网络层（`tcp_server`、`epoller`、`HTTPConn`、`HTTPRequest`、`HTTPResponse`、`config`）
- `src/buffer`：环形缓冲区封装，提供高效的 `readv`/`writev` 支持
- `src/thread_pool`：简单可复用线程池
- `src/timer`：最小堆定时器，负责连接超时回收
- `src/logger`：异步日志与消息缓冲
- `src/database`：SQLite 单例与连接池封装
- `resource/`：静态页面、图片、视频等示例资源

## 快速开始

### 依赖

- CMake ≥ 3.28
- 支持 C++20 的编译器（建议 GCC 13+ 或 Clang 16+）
- SQLite3 开发库

### 构建与运行

```bash
cmake -S . -B build
cmake --build build
./build/WebServer [-p PORT] [-m TRIG] [-o LINGER] [-s SQL] [-t THREADS] [-c CLOSE_LOG] [-q LOG_QUEUE]
```

服务器启动后默认监听 `0.0.0.0:9999`，静态资源目录为项目根目录下的 `resource/`。

### 配置项

| 选项 | 默认值 | 含义 |
| ---- | ------ | ---- |
| `-p` | `9999` | 监听端口 |
| `-m` | `0`    | 触发模式：0=默认，1=连接 ET，2=监听 ET，3=全 ET |
| `-o` | `0`    | 是否开启 `SO_LINGER` 优雅关闭 |
| `-s` | `8`    | SQLite 只读连接池规模 |
| `-t` | `8`    | 线程池线程数 |
| `-c` | `0`    | 是否关闭日志（1 为关闭） |
| `-q` | `1024` | 异步日志队列容量 |

### 数据库准备

默认使用项目根目录下的 `db.sqlite3`。首次运行请手动建表：

```sql
CREATE TABLE IF NOT EXISTS user (
  id INTEGER PRIMARY KEY AUTOINCREMENT,
  username TEXT NOT NULL UNIQUE,
  password TEXT NOT NULL
);
```

可用任意 SQLite 客户端或 `sqlite3 db.sqlite3` 命令创建该表。

### 测试

```bash
cmake --build build --target test
ctest --test-dir build
```

目前提供 `logger` 单元测试，可在构建目录通过 `ctest` 运行。

## 规范

- 不使用异常，除非是 STL 自带
- 尽可能避免继承
