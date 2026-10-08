# General module

`sindre::general` 提供 C++17 项目的基础错误处理、文件系统、运行时、字符串、网络、命令行和诊断能力。

General 不是必须的 header-only 模块，公共 API 通过 `.h` 头文件暴露。

## Public headers

General 对外只提供七个稳定入口，根聚合头 `sindre/general.h` 会包含全部入口：

| 头文件 | 能力 |
| --- | --- |
| `sindre/general/core.h` | `Result`、`Error`、版本、范围、作用域清理、编码和 RLE |
| `sindre/general/system.h` | 路径、文件、目录、`glob`、文件监控、临时文件、环境、系统信息、JSON 配置、通知、托盘、对话框、文件选择、剪切板和自启动 |
| `sindre/general/runtime.h` | 线程池、同步/异步任务、取消、超时、进程和动态库 |
| `sindre/general/cli.h` | CLI 参数定义、解析和类型化访问 |
| `sindre/general/string.h` | CsString 编码字符串、转换和正则表达式 |
| `sindre/general/network.h` | URL、HTTP、GET/POST、重试、超时、上传、下载和 JSON 网络响应 |
| `sindre/general/diag.h` | 日志、诊断计时、条件检查和 Crashpad |

### Runtime

运行时接口统一使用小写下划线命名，例如 `sleep()`、`wait_for()`、
`is_cancelled()` 和 `get_token()`。可取消异步任务使用 `TaskOptions` 传递取消令牌、
截止时间和进度回调：

```cpp
sindre::general::CancellationSource source;
sindre::general::TaskOptions options;
options.token = source.get_token();
options.deadline = std::chrono::steady_clock::now() + std::chrono::seconds(5);

auto task = sindre::general::try_run_async(
    [](sindre::general::CancellationToken token) {
        while (!token.is_cancelled()) {
            auto status = sindre::general::sleep(0.01,
                                                sindre::general::TaskOptions{token});
            if (!status) return -1;
        }
        return 0;
    }, options);
```

`sleep()` 的参数单位固定为秒，支持小数，例如 `sleep(0.1)` 表示 100 毫秒；调用者不需要
引入 `std::chrono` 字面量。取消和截止时间是协作式语义：正在执行的用户函数必须主动检查令牌或使用
`sleep()` 等可取消操作；超时只会让任务结果返回 `timed_out`，不会强制终止任意 C++
函数。`ThreadPool::stop()` 会取消尚未开始的排队任务，并等待正在执行的任务结束，
因此线程池任务不得永久阻塞且应响应取消。

性能计时统一放在 `sindre::general::runtime`：

```cpp
sindre::general::runtime::scopedtimer timer("load_model");
load_model();
```

需要接收计时结果时传入回调；需要跨多个阶段手动读取时使用 `runtime::stopwatch`：

```cpp
sindre::general::runtime::scopedtimer timer(
    "load_model",
    [](std::string_view name, std::chrono::nanoseconds elapsed) {
        std::cout << name << ": " << elapsed.count() << " ns\n";
    });

sindre::general::runtime::stopwatch stopwatch;
stopwatch.start();
load_model();
auto elapsed = stopwatch.stop();
```

并行处理使用 `runtime::parallel_for()`；回调可以接收索引和取消令牌，返回 `void` 或
`Result<void>`：

```cpp
sindre::general::runtime::parallel_options options;
options.workers = 4;
options.thread_name = "mesh_worker";
options.progress = [](double value) {
    std::cout << value * 100.0 << "%\n";
};

auto result = sindre::general::runtime::parallel_for(
    0, mesh_count,
    [](std::size_t index, sindre::general::CancellationToken token) {
        if (token.is_cancelled()) return;
        process_mesh(index);
    },
    options);
```

当前线程命名使用：

```cpp
auto result = sindre::general::runtime::set_thread_name("inference_worker");
```

重试使用秒作为延迟单位，默认指数退避：

```cpp
sindre::general::runtime::retry_options options;
options.max_attempts = 3;
options.initial_delay = 0.1;
options.maximum_delay = 2.0;

auto result = sindre::general::runtime::retry(
    [](sindre::general::CancellationToken token) {
        return connect_service(token);
    },
    options);
```

三者都支持取消令牌和截止时间；并行循环和重试中的用户回调仍然必须避免永久阻塞。

线程池优先使用 `ThreadPool::create()` 获取结构化初始化错误；直接构造失败时会抛出
异常（编译器启用异常支持的构建），严格编译器无异常构建使用 `is_running()` 和
`get_error()` 检查状态。

公共 API 使用 `sindre::general::Result<T>` 表达可预期失败，错误包含 `code`、
`message` 和 `context`。第三方异常不会穿透 General 的公共错误边界。

成功状态下 `Result<T>` 除了 `value()`，还支持指针式访问：

```cpp
#include <iostream>

auto result = sindre::general::uuid4();
if (!result)
    return;

std::cout << result->size() << '\n';
std::cout << result.data()->substr(0, 8) << '\n';
```

`data()` 在失败时返回 `nullptr`；`operator->` 和 `operator*` 必须在成功状态下使用，
误用会像 `value()` 一样确定性终止。`data()`、`value_ptr()` 和 `error_ptr()` 只允许
在左值 `Result` 上调用，避免从临时对象取得悬空指针；对临时失败结果调用 `error()`
会返回一个独立的 `Error` 值。对临时成功结果调用 `value()` 也会取得独立的值
（可移动的值会从结果中移出）；`Result<void>::value()` 只检查成功状态。

连续操作可以使用 `and_then()`。回调必须返回另一个 `Result<U>`；前一步失败时不会执行
回调，原始 `Error` 会继续传递。异常构建中，回调抛出的标准异常会转换为
`result.and_then` 上下文的 `Result` 错误；`std::system_error` 保留原错误码，
`std::bad_alloc` 使用 `not_enough_memory`，其他异常使用 `invalid_argument`。
严格无异常构建不包含异常捕获路径。右值失败结果会移动错误，左值失败结果会复制错误：

```cpp
auto size = sindre::general::Result<std::string>::success("sindre")
    .and_then([](std::string text) {
        return sindre::general::Result<std::size_t>::success(text.size());
    });
if (!size)
    return 1;
```

## 示例

```cpp
#include <sindre/general.h>

auto entries = sindre::general::list_directory("models");
auto files = sindre::general::glob("models/*/*.onnx");
auto text = sindre::general::string::parse_int("42");
```

文件信息和哈希使用带动作前缀的接口：

```cpp
auto size = sindre::general::get_file_size("model.onnx");
auto info = sindre::general::get_file_info("model.onnx");
auto digest = sindre::general::calculate_file_sha256("model.onnx");
sindre::general::create_directories("cache/models");
sindre::general::copy_file("model.onnx", "cache/models/model.onnx");
sindre::general::move_path("cache/models/model.onnx", "cache/model.onnx");
sindre::general::remove_file("cache/model.onnx");
sindre::general::remove_directory("cache/models", true);
```

`remove_file()` 和 `remove_directory()` 返回 `Result<bool>`：成功但目标不存在时返回
`false`，目标被删除时返回 `true`。非递归删除非空目录会返回错误；递归删除需要显式
传入 `true`。

系统信息和环境变量也通过明确的 getter/setter 命名：

```cpp
auto home = sindre::general::system::get_environment_variable("HOME");
auto system = sindre::general::system::get_system_information();
auto executable = sindre::general::system::get_executable_path();
sindre::general::system::unset_environment_variable("OLD_SETTING");
```

`get_system_information()` 返回 OS 名称和版本、架构、编译器、主机名、当前用户名、
CPU 型号、逻辑 CPU 数量、物理内存总量与可用内存、非回环本地 IP 地址，以及当前系统卷的总容量和可用容量。`gpus`
是最佳努力结果：Windows 使用显示适配器枚举，Linux/WSL 使用 DRM/sysfs；驱动未提供
可识别信息时可能为空，不会伪造 GPU 数据。

Shell 命令通过 `system::shell_run()` 跨平台执行。默认捕获标准输出和错误输出，
默认超时为 30 秒；每个输出流默认限制为 16 MiB，可通过
`ShellOptions::maximum_output_bytes` 调整，设为 `0` 表示不限制。命令退出码非零仍
属于正常完成，启动失败、超时、取消和输出超限才通过 `Result` 返回错误：

```cpp
#include <sindre/general/system.h>
#include <iostream>

sindre::general::system::ShellOptions options;
options.working_directory = "scripts";
options.timeout_seconds = 10;

auto result = sindre::general::system::shell_run("python build.py", options);
if (result) {
    std::cout << result.value().stdout_text;
    if (result.value().exit_code != 0) {
        std::cerr << result.value().stderr_text;
    }
}
```

Windows 默认使用 `cmd.exe`，Linux/WSL 默认使用 `/bin/bash`。可通过
`ShellOptions::backend` 选择 `cmd`、`powershell`、`sh` 或 `bash`；POSIX 环境的
PowerShell 后端要求安装 `pwsh`。`timeout_seconds` 是整数秒，默认为 30，设置为 `0`
表示不限制。该接口接收的是可信 Shell 文本，
不要把未经校验的用户输入直接拼接到命令中。当前 General 没有公开的 argv 进程接口，
不可信参数应先严格校验，并尽量避免交给 Shell 解释。

不需要自定义参数时可以直接调用：

```cpp
auto result = sindre::general::system::shell_run("echo hello");
```

文本文件使用 `path::read_text()` / `path::write_text()`，二进制文件使用
`path::read_bytes()` / `path::write_bytes()`；二进制接口不执行 UTF-8 校验，适合模型、
压缩包和网络下载内容。

文件操作通过 `std::filesystem` 处理 Unicode 路径，不暴露 `dirent`。`glob()` 支持
`*`、`?` 和独立路径组件 `**`。文件哈希按流式块读取；MD5 只用于缓存、去重和
非安全完整性检查，安全校验应使用 SHA-256。

### 加密

General 提供不暴露 OpenSSL 类型的内存和单文件加解密接口。默认使用
AES-256-GCM 与 PBKDF2-HMAC-SHA256；每次加密自动生成随机 salt 和 nonce，密文带有
自描述头和认证标签。文本接口返回 Base64，二进制接口使用字节数组：

```cpp
auto encrypted = sindre::general::codec::encrypt("secret", "password");
auto plaintext = encrypted
    ? sindre::general::codec::decrypt(encrypted.value(), "password")
    : sindre::general::Result<std::string>::failure(encrypted.error());
```

文件接口采用同目录临时文件，只有完整写入、认证成功、刷新并替换目标后才完成；失败、
取消、密码错误或磁盘错误会删除临时文件，不会破坏已有目标。默认不覆盖已有目标，使用
`file::CryptoOptions::overwrite = true` 显式允许覆盖；选项还支持缓冲区大小、进度回调
和 `CancellationToken`：

```cpp
sindre::general::file::CryptoOptions options;
options.progress = [](std::uint64_t current, std::uint64_t total) {
    std::cout << current << "/" << total << '\n';
};
auto encrypted_file = sindre::general::file::encrypt(
    "plain.bin", "secret.sindre", "password", options);
auto decrypted_file = sindre::general::file::decrypt(
    "secret.sindre", "restored.bin", "password", options);
```

当前范围是单文件，不是目录、ZIP/TAR 或断点续传；密码不会写入错误上下文或日志。需要
加密目录时，先由上层选择归档格式和文件清单，再对归档文件调用此接口。

### JSON 与配置

General 默认集成固定版本的 simdjson，但用户不需要直接操作 simdjson DOM。普通 JSON
优先使用 `json::parse()` 读取为可复制的 `Value`，使用 `json::object()`、`json::array()`
和 `Value::to_json()` 构造及序列化：

```cpp
#include <sindre/general/system.h>

using namespace sindre::general;

auto payload = json::object({
    {"name", "sindre"},
    {"port", 8080},
    {"enabled", true},
    {"tags", json::array({"general", "中文"})},
});

auto text = json::Value(payload).to_json();
auto parsed = json::parse(text.value());
if (!parsed) return 1;

auto name = parsed.value().find("name")->get_string();
```

`Value` 支持 `null`、布尔、整数、浮点、字符串、对象和数组，并提供
`get_string()`、`get_int()`、`get_float()`、`get_bool()` 类型读取。`json::try_parse()`
保留为需要直接访问 simdjson DOM 的底层接口；普通业务代码使用 `json::parse()`。

应用配置使用 map 风格的 `config::Config`，键支持点号路径，类型不会被转换成字符串：

```cpp
config::Config config;
config.set("server.host", "127.0.0.1");
config.set("server.port", 8080);
config.set("server.enabled", true);

auto port = config.get_int("server.port");
auto output = config.to_json();
```

从 JSON 文件加载、默认值和环境变量覆盖仍然可用：

```cpp
auto defaults = config::Config::create_with_defaults({
    {"server.port", 8080},
    {"server.enabled", true},
});
auto loaded = config::Config::load_file("config.json", defaults);
if (loaded) loaded.value().apply_environment_overrides("MY_APP");
```

例如 `server.port` 对应环境变量 `MY_APP_SERVER_PORT`。配置序列化会自动恢复嵌套
对象；数组和 `null` 也会保留。

文件监控事件通过 `file_watch::EventType::created`、`modified` 和 `removed` 区分。
当前实现是跨平台轮询，不承诺原生文件通知的低延迟语义。

General 的核心系统实现只支持 Windows 和 Linux/WSL；其他平台在配置阶段被拒绝。Linux
使用用户级 systemd service，Windows 使用用户
Startup 目录脚本。

桌面快捷能力统一从 `sindre::general::desktop` 使用：Windows 调用 Win32/COM 原生 API；
Linux 不新增编译期桌面库，而是安全地调用运行时已安装的 `notify-send`、`zenity`、
`kdialog`、`wl-copy`、`wl-paste`、`xclip`、`xsel`、`pkexec` 和可选的 `yad`。命令不存
在或当前会话不支持时返回 `function_not_supported`，参数通过 argv 传递，不经过 shell。

```cpp
using namespace sindre::general::desktop;

auto notification = send_notification("sindre", "任务已完成");
auto selected = open_file_dialog({}, true);
auto directory = select_directory_dialog();
auto copied = set_clipboard_text("中文文本");
auto message = show_message_box("sindre", "是否继续？", MessageBoxType::question);
```

`open_file_dialog()`、`save_file_dialog()` 和 `select_directory_dialog()` 返回 UTF-8 安全的
`std::filesystem::path`；用户取消返回 `operation_canceled`。Linux 托盘只在安装 `yad`
时提供最小 `start_tray()`/`stop_tray()` 能力，不支持托盘菜单和事件回调。Linux 桌面命令
不是构建依赖，部署时应由应用检查并提示用户安装。桌面 API 不使用 GTK、Qt、SDL 或
shell 拼接命令。

日志、诊断和 Crashpad 统一从 `sindre/general/diag.h` 使用：

```cpp
#include <sindre/general/diag.h>

sindre::general::log::init_log(
    "sindre",
    sindre::general::log::Level::info,
    sindre::general::log::default_pattern,
    "logs/sindre.log",
    20 * 1024 * 1024, // 单个文件最大 20 MiB
    7,                // 保留 7 个轮转文件
    false);           // 启动时不立即轮转
sindre::general::log::info("service started");
auto check = sindre::general::diagnostics::check(true, "ready");
```

`filename` 为空时写入控制台；指定文件后启用按大小轮转。启用文件轮转时
`max_size_bytes` 和 `max_files` 必须大于零，`rotate_on_open=true` 会在每次初始化时
先轮转现有文件。
同一个 logger 已初始化时，重复调用是幂等的；需要改变输出目标或轮转策略时先调用
`shutdown()`。

模块或子系统不应通过 `init_log()` 改变宿主的全局默认 logger。即使应用没有调用
`init_log()`，也可以直接使用 `create_logger()`；它会基于 spdlog 自带的默认 logger 创建
模块 logger。应用需要自定义文件、格式、轮转或异步策略时，再先调用 `init_log()`。
`create_logger()` 创建的模块 logger 会复用默认 logger 的 sinks 和格式，只覆盖调用者明确
传入的日志级别；同名 logger 会被复用且不会覆盖已有配置，也不会替换全局默认 logger。

```cpp
auto image_logger = sindre::general::log::create_logger(
    "sindre.utils_2d.image", sindre::general::log::Level::info);
if (!image_logger)
    return image_logger.error();

image_logger.value()->info("image loaded");
```

`create_logger()` 返回 `Result<LoggerPtr>`，名称不能为空；只有默认 logger 被宿主显式移除等
异常情况下才会返回不可用错误。应用退出时由应用统一调用 `shutdown()`，模块 logger 不会被
用来替换宿主的默认 logger。

异步日志在同一个入口启用，默认仍是同步模式：

```cpp
sindre::general::log::init_log(
    "my_service",
    sindre::general::log::Level::info,
    sindre::general::log::default_pattern,
    "logs/my_service.log",
    20 * 1024 * 1024,
    7,
    false,  // rotate_on_open
    true,   // asynchronous
    8192,   // async_queue_size
    1,      // async_worker_threads
    sindre::general::log::AsyncOverflowPolicy::block);
```

`block` 保证日志不因队列满而丢失，但可能让产生日志的线程等待；
`overrun_oldest` 丢弃最旧日志，`discard_new` 丢弃新日志。异步队列参数只在
General 创建线程池时生效；如果宿主程序已经创建了 spdlog 全局线程池，General 会复用它。

Crashpad 单独启动，不会由 `init_log()` 隐式启动：

```cpp
sindre::general::crashpad::Options crashpad;
crashpad.handler = "crashpad_handler.exe";
crashpad.database = "crash-reports";
crashpad.metrics_dir = "crash-metrics";
crashpad.upload_url = "https://example.com/crashpad";
crashpad.annotations["application"] = "my_service";
crashpad.startup_timeout = std::chrono::seconds(10);
auto crashpad_status = sindre::general::crashpad::start(crashpad);
```

`start()` 会校验 handler、等待 handler 就绪，并且重复调用是幂等的。Windows 异步启动会
等待 handler 就绪；Linux 使用 Crashpad 的同步启动语义，因为当前固定 Linux 版本没有
Windows 的 `WaitForHandlerStart` 接口。Crashpad 未编译或后端不可用时返回
`std::errc::function_not_supported`。`metrics_dir` 是 Crashpad
自身的指标目录，不是崩溃转储目录；崩溃报告数据库由 `database` 指定。

RE2 是 General 的固定必需依赖，但公共命名空间使用 `sindre::general::regex`，不暴露
第三方库名称。具体实现源文件位于 `modules/general/src/`，该目录只放对应的
`.cpp` 实现；模板和公共类型直接放在 `include/sindre/general/*.h` 中。

### CLI

CLI 公共接口不暴露 argparse。使用 `Specification` 声明选项和位置参数，解析结果通过
`Result<Arguments>` 返回：

```cpp
sindre::general::cli::Specification spec;
spec.settings.program_name = "demo";
spec.settings.version = "1.0.0";
spec.add_option("--port", "-p")
    .required()
    .help("server port");
spec.add_flag("--verbose", "-v")
    .help("verbose output");
spec.add_option("--config", "-c")
    .default_value("config.json")
    .help("config file");
spec.add_positional("input")
    .required()
    .help("input file");

auto arguments = sindre::general::cli::parse_current(spec);
if (arguments && arguments.value().wants_help()) {
    std::cout << arguments.value().action_text;
}
```

`has()` 表示解析后存在有效值，默认值也算存在；`is_set()` 只表示用户是否显式传入。
支持 `--option value`、`--option=value`、短别名、位置参数和 `--` 终止符。重复选项、
未知选项、缺少值和位置参数数量错误都会返回 `Result` 错误。Windows 和 Linux
的当前进程参数会转换为 UTF-8，帮助和版本请求只返回状态，不会输出或退出宿主进程。

在编译器启用异常的正常构建中，CLI 的私有实现使用固定版本 argparse；启用
`-DSINDRE_NO_EXCEPTIONS=ON` 后，CMake 同时使用 `-fno-exceptions` 或 `/EHs-c-`，
argparse 不参与该构建，CLI 改用 General 内部无异常解析器。两条路径必须保持相同的
解析规则、错误 code/message/context 和 help/version 行为。

### 网络

`network.h` 只提供客户端 API，不提供稳定的 HTTP Server、WebSocket、SSE 或断点续传。
公共接口不暴露 cpp-httplib、OpenSSL 或 simdjson 类型；这些依赖只在 General runtime
内部使用。

常用请求可以直接传入完整 URL 字符串；需要重复使用或修改 URL 时，再解析为
`network::Url`：

```cpp
#include <sindre/general/network.h>

auto simple_response = sindre::general::network::get("https://example.com");
auto posted = sindre::general::network::post(
    "https://example.com/api", R"({"name":"sindre"})", "application/json");
auto posted_json = sindre::general::network::post_json(
    "https://example.com/api",
    sindre::general::json::object({
        {"name", "sindre"}, {"age", 18}, {"enabled", true}}));
if (!simple_response || !posted) return 1;

auto target = sindre::general::network::parse(
    "https://example.com/api?q=中文&tag=a%2Bb");
if (!target) return 1;

sindre::general::network::Request request;
request.method = sindre::general::network::Method::get;
request.target = target.value();
auto response = sindre::general::network::request(request);
if (!response) {
    // response.error().code/message/context
    return 1;
}
if (!response.value().is_success()) return 1;
```

`Result<Response>` 成功只表示传输完成并收到了 HTTP 响应，不代表状态码是 `2xx`。
使用 `is_success()` 或 `require_success()` 判断业务成功。`get_json()` 和
`request_json()` 返回高层 `json::Value`，并把非 `2xx` 与 JSON 解析失败统一转换成
`Result` 错误。
`post_json()` 接收 `json::Object` 并负责 JSON 类型转换和字符串转义，字段值支持 `null`、
布尔、整数、浮点数、字符串、对象和数组，Content-Type 自动设置为 `application/json`。

默认连接/读取/写入/总超时分别为 5 秒、30 秒、30 秒和 60 秒；默认最大内存响应为
16 MiB，默认不重试。需要重试时显式设置 `RetryPolicy`；POST、PATCH 等非幂等请求还必须
提供 `should_retry`。`CancellationToken`、总超时、重试等待、body 回调和进度回调都会
中止底层请求，不会让后台 socket 在 Future 销毁后继续运行。

大响应使用 `RequestOptions::on_body` 流式接收：回调返回 `false` 或抛出异常都会得到
统一错误，普通响应则受 `maximum_response_bytes` 限制。Header 名和值拒绝控制字符，错误
上下文包含操作、脱敏 URL 和尝试次数，不记录 Authorization、Cookie、请求体或证书内容。

下载使用同目录临时文件，只有完整接收、刷新并成功替换后才覆盖目标文件；失败、取消、
超时和 HTTP 非成功不会破坏原目标。上传使用单次打开的文件和分块 provider，第一阶段不
支持断点续传。上传下载均默认不重试，进度回调的未知总长度使用 `0`。

URL 百分号编码严格按 RFC3986 非保留字符处理，`+` 不转换为空格；表单编码应在业务层
单独实现。用户名、密码和 fragment 不允许进入 HTTP URL。

### 字符串

公共层只提供一个 `sindre::general::string::String` 和它的非拥有视图
`sindre::general::string::StringView`。用户不需要选择编码模板，
也不需要区分 UTF-8 字符串和 UTF-16 字符串。当前公共 `String` 固定使用 CsString
的 UTF-8 后端作为实现，转换和存储细节由 General 内部管理；这不是运行时自动选择
物理编码的承诺：

```cpp
#include <sindre/general/string.h>

using sindre::general::string::String;

String text("中文");
String windows_text(u"中文");
String number_text(42);
auto utf8 = windows_text.to_utf8();
auto utf16 = text.to_utf16();
auto number = number_text.to_int();
auto ratio = String("3.14").to_float();
auto enabled = String("yes").to_bool();
auto path = text.to_path();
auto view = text.get_view();
auto part = view.try_substr(0, 2);
```

`String` 的 `size()`、索引、查找和截取按 Unicode code point 工作。构造函数可以
直接接收常用的 UTF-8/UTF-16 输入；需要与外部 API 交互时使用 `to_utf8()`、
`to_utf16()`，不提供隐式 `std::string` 转换；类型解析直接使用 `to_int()`、
`to_float()`、`to_bool()`。可能失败的转换和操作返回带有 `code`、`message` 和
`context` 的 `Result`，编码互操作还提供 `try_to_utf8()`/`try_to_utf16()` 形式。
也可以使用 `String::from(value)` 创建受检查的字符串；支持数字、布尔值、路径以及
UTF-8/UTF-16/UTF-32/宽字符串输入。`String::from(...)` 负责把构造失败转换为
`Result`，但不会把所有非法 UTF-8 自动变成错误；当前 CsString 对部分非法输入采用
替换字符策略。需要严格拒绝非法 UTF-8 时，先调用 `valid_utf8()`，再构造字符串。
`to_code_point()` 用于取得只有一个 Unicode
code point 的字符串；`to_char()` 只接受 ASCII，Unicode 字符使用 `to_char32()`。
公共 API 不暴露 `BasicString`、`String16` 或 CsString 编码别名。

`StringView` 按 code point 查询，不复制底层文本。它提供 `find()`、`rfind()`、
`find_first_of()`、`find_last_of()`、`find_first_not_of()`、`find_last_not_of()`、
`starts_with()`、`ends_with()`、`contains()`、`count()`、`compare()`、
`try_substr()`、`remove_prefix()`、`remove_suffix()` 和
`try_for_each_code_point()`。视图只在源 `String` 存活且未发生使存储失效的修改时有效；
需要独立所有权时调用 `try_to_string()`。

可失败的修改使用统一的 `try_` 前缀，例如 `try_assign()`、`try_append()`、
`try_insert()`、`try_replace()`、`try_clear()`、`try_pop_back()`、
`try_swap()` 和 `try_shrink_to_fit()`。这些接口的索引和计数均按 Unicode code point，
不是 UTF-8 字节。

修改接口按 Unicode code point 索引：

```cpp
String value("a中文a");
auto status = value.replace(1, 2, String("世界"));
auto all_status = value.replace_all(String("a"), String("x"));
```

`replace()` 和 `replace_in_place()` 都修改当前对象，后端在替换长度变化时可以重新
分配内存；它们不承诺对 UTF-8 字节执行不安全的原地覆盖。

### 字符串的生产使用边界

当前 `String` 可以作为项目内广泛使用的通用字符串类型，适合配置、日志、CLI、
文件路径、网络字段和一般业务文本。它的稳定语义是：索引、长度、查找和修改都按
Unicode code point 计算，编码输入由 CsString 转换，公共错误通过 `Result<T>` 返回。
因此，字符串 API 不要求调用方在 UTF-8、UTF-16 和宽字符串之间维护多套类型。

生产代码应遵循以下规则：

1. 外部输入、文件内容和网络内容优先使用 `String::from(...)` 或
   `String::try_create_utf8(...)`，检查返回的 `Result`；直接构造函数适合已知有效的
   字面量和受控输入。
2. 需要跨库、跨进程或写入日志时，优先使用 `try_to_utf8()`、`try_to_utf16()`，
   处理失败的 `Error`；`to_utf8()`、`to_utf16()`、`to_stdstr()` 是已确认转换成功时
   使用的便利接口。
3. `StringView` 不拥有数据，不能跨越源 `String` 的生命周期，也不能在源字符串修改
   后继续使用；需要保存时调用 `try_to_string()`。
4. `size()` 不是 UTF-8 字节数，也不是用户看到的“字符数”。例如带组合音标或 emoji
   组合序列时，一个用户感知字符可能包含多个 code point。

当前明确不承诺以下高级 Unicode 语义：

| 能力 | 当前状态 | 说明 |
| --- | --- | --- |
| UTF-8 合法性校验 | 支持 | `valid_utf8()` 校验字节序列；CsString 构造负责其输入转换策略 |
| Unicode code point | 支持 | `size()`、查找、截取和遍历均按 code point |
| ASCII 大小写 | 支持 | 使用 `try_lower_ascii()`、`try_upper_ascii()` 或对应自由函数 |
| Unicode 大小写折叠 | 不承诺 | 不能用 ASCII 大小写替代中文、希腊文、土耳其文等 locale 规则 |
| Unicode normalization | 不承诺 | `normalize_utf8()` 当前只校验并返回文本，不执行 NFC/NFD 等规范化 |
| grapheme cluster | 不承诺 | UI 光标、删除和用户感知字符处理需要专用文本库 |
| locale 排序 | 不承诺 | `compare()` 是字符串比较，不是本地化排序 |

因此，当前版本可以作为生产级的“编码安全、code point 级通用字符串”使用；涉及
用户名规范化、国际化大小写、搜索排序、UI 光标或安全标识符时，仍需在业务层增加
Unicode normalization、grapheme 和 locale 策略。`String` 的公共模板已经适配
`SINDRE_NO_EXCEPTIONS`，但完整发布前仍应通过严格编译器无异常构建、安装消费者、模糊测试和
性能基准验证，不能仅凭单元测试宣称所有生产场景均已覆盖。

## CMake

链接唯一的 General target：

```cmake
target_link_libraries(app PRIVATE sindre::general)
```

默认配置为静态 General runtime：

```text
SINDRE_GENERAL_BUILD_LIBRARY=ON
SINDRE_GENERAL_SHARED=OFF
```

如果应用需要共享库，可设置 `SINDRE_GENERAL_SHARED=ON`；这不是 header-only 开关。

General 的全部依赖由 `thirds/general/Dependencies.cmake` 固定，不能通过
`SINDRE_WITH_*` 关闭，也不能用宿主环境中的同名 target 覆盖。缺少固定源码
或固定包时，CMake 在配置阶段直接失败。

默认构建：

```powershell
cmake -S . -B build/windows -G Ninja -DCMAKE_BUILD_TYPE=Release -DSINDRE_BUILD_TESTS=ON
cmake --build build/windows --parallel
ctest --test-dir build/windows --output-on-failure
```
