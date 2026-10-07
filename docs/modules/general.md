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
| `sindre/general/string.h` | CsString 编码字符串、转换和正则表达式 |
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

RE2 是 General 的固定必需依赖，但公共命名空间使用 `sindre::general::regex`，不暴露
第三方库名称。具体实现源文件位于 `modules/general/src/`，该目录只放对应的
`.cpp` 实现；模板和公共类型直接放在 `include/sindre/general/*.h` 中。

### 字符串

`sindre::general::string::BasicString<Encoding>` 是 CsString 的高级封装，编码策略
由模板参数决定，不由 General 固定为 UTF-8：

```cpp
using String = sindre::general::string::String;     // CsString::utf8
using String16 = sindre::general::string::String16; // CsString::utf16
```

字符串的 `size()`、索引、查找和截取按 Unicode code point 工作；底层存储单元、
编码转换以及非法输入的替换策略由 CsString 决定。需要与旧接口交互时显式调用
`try_to_utf8()`、`try_to_utf16()` 或 `to_utf8()`，不提供隐式 `std::string` 转换。
修改、转换和可能访问越界的操作使用 `try_*`，失败统一返回带有 `code`、`message`
和 `context` 的 `Result`。

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
cmake -S . -B build -G Ninja -DSINDRE_BUILD_TESTS=ON
cmake --build build --parallel
ctest --test-dir build --output-on-failure
```
