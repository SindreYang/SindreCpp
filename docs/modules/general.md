# General module

`sindre::general` 提供 C++17 项目的基础错误处理、文件系统、运行时、字符串、网络、命令行和诊断能力。

General 不是必须的 header-only 模块，公共 API 通过 `.h` 头文件暴露。

## Public headers

General 对外只提供七个稳定入口，根聚合头 `sindre/general.h` 会包含全部入口：

| 头文件 | 能力 |
| --- | --- |
| `sindre/general/core.h` | `Result`、`Error`、版本、指针、范围、作用域清理、编码和 Eigen 数据桥接 |
| `sindre/general/system.h` | 路径、文件、目录、`glob`、文件监控、临时文件、环境、系统信息、JSON 配置、通知、托盘、剪切板和自启动 |
| `sindre/general/runtime.h` | 线程池、同步/异步任务、取消、超时、进程和动态库 |
| `sindre/general/cli.h` | CLI 参数定义、解析和类型化访问 |
| `sindre/general/string.h` | UTF-8、字符串处理、转换和正则表达式 |
| `sindre/general/network.h` | URL、HTTP、GET/POST、重试、超时、上传、下载和 JSON 网络响应 |
| `sindre/general/diag.h` | 日志、诊断计时、条件检查和 Crashpad |

公共 API 使用 `sindre::general::Result<T>` 表达可预期失败，错误包含 `code`、
`message` 和 `context`。第三方异常不会穿透 General 的公共错误边界。

## 示例

```cpp
#include <sindre/general.h>

auto entries = sindre::general::list_directory("models");
auto files = sindre::general::glob("models/*/*.onnx");
auto text = sindre::general::string::parse_int("42");
```

文件操作通过 `std::filesystem` 处理 Unicode 路径，不暴露 `dirent`。`glob()` 支持
`*`、`?` 和独立路径组件 `**`。文件哈希按流式块读取；MD5 只用于缓存、去重和
非安全完整性检查，安全校验应使用 SHA-256。

日志、诊断和 Crashpad 统一从 `sindre/general/diag.h` 使用：

```cpp
#include <sindre/general/diag.h>

sindre::general::log::initialize();
sindre::general::log::info("service started");
auto check = sindre::general::diagnostics::check(true, "ready");
```

RE2 仍是可选实现依赖，但公共命名空间使用 `sindre::general::regex`，不暴露
第三方库名称。具体实现源文件位于 `modules/general/src/`，该目录只放对应的
`.cpp` 实现；模板和公共类型直接放在 `include/sindre/general/*.h` 中。

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

可选集成仍由 `SINDRE_WITH_LOG`、`SINDRE_WITH_HTTP`、`SINDRE_WITH_JSON`、
`SINDRE_WITH_CLI`、`SINDRE_WITH_RE2`、`SINDRE_WITH_CRASHPAD`、`SINDRE_WITH_ZLIB`
和 `SINDRE_WITH_EIGEN` 控制。关闭后，对应 API 返回明确的
`function_not_supported` 或不参与头文件导出，不产生静默成功。
Crashpad 在仓库固定 General 二进制包存在时默认启用；没有该包时可设置
`SINDRE_WITH_CRASHPAD=OFF`，或者通过 `SINDRE_CRASHPAD_TARGET` 提供兼容的
CMake target。

默认构建：

```powershell
cmake -S . -B build -G Ninja -DSINDRE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
