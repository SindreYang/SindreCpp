# 开发指南

本文面向贡献者和维护者，说明目录边界、公共 API、依赖接入、测试和提交前检查。
如果只是使用库，请先回到 [文档入口](../README.md)，不需要阅读本页。
当前 Windows 开发机的固定工具链路径见 [本机开发环境](本机.md)，Agent 修改构建前应先读取。

## 项目定位

sindrecpp 是 Sindre 生态的 C++17 基础库，提供稳定、直接、面向使用者的通用能力。
所有公共 target 和内置第三方适配均以 C++17 为最低且固定的项目语言标准；不以
C++20 作为公共接口或可选后端的隐含要求。

项目按“功能域”组织，不按第三方依赖组织。第三方库只是实现方式，不应成为对外 API 的核心概念。

## 目录结构

```text
sindrecpp/
├── include/sindre/      # 根聚合头
├── modules/              # general/math/utils_py/ai/gui/utils_2d/utils_3d
│   └── <module>/{src,tests,CMakeLists.txt}
├── examples/            # 最小可运行示例
├── thirds/              # 按模块登记的第三方依赖来源、版本和接入方式
├── tests/               # 单元测试
├── docs/                # 全部项目、模块和依赖文档
├── scripts/             # Windows/Linux 快捷构建脚本
├── cmake/               # 编译器默认值、选项和公共 CMake 函数
├── CMakePresets.json    # Ninja 构建的可选入口
├── CMakeLists.txt       # CMake 配置
├── docs/README.md       # 项目入口文档
└── LICENSE
```

新增功能时，优先按用户看到的功能放置：

- 字符串处理放在 `sindre/general/string.h` 的 `sindre::general::string` 能力中
- `sindre/utils_3d/` 对外只提供后端无关的 `Mesh`、`PointCloud`、`SindreMesh` 和算法接口；VTK、CGAL、PCL 只能放在模块私有实现中
- `sindre/utils_3d/sindremesh.h` 是用户快速使用入口；算法按领域放在 `sindre/utils_3d/algorithms/`
- 网格和点云算法实现位于 `modules/utils_3d/`；数学交换使用 `sindre::math`，第三方实现只允许出现在模块私有目录
- 不得在 `utils_3d` 公共头、Options、Result 或报告中暴露 VTK、CGAL、PCL 类型、后端枚举和原生转换函数
- 日志、诊断和 Crashpad 通过 General 的 `sindre/general/diag.h` 暴露，具体实现位于 `modules/general/src/`；该目录只放 `.cpp`，不放私有头
- 媒体能力未来放在预留的 `utils_av` 模块
- 图像能力应放在 `modules/utils_2d`，入口为 `sindre/utils_2d.h`
- 底层库别名放入对应模块的 `native` 命名空间

不要为每一个函数单独创建头文件。

## 功能域

sindrecpp 的顶层组织跟随 Sindre：

- `general`：通用工具、字符串、日志、HTTP、JSON、CLI；
- `math`：固定 Eigen/OpenBLAS 基础、行主序 Matrix/Array 别名和 Eigen 互操作；
- `utils_py`：Python/NumPy 数组与网格互操作；

通用错误约定：新接口优先返回 `sindre::general::Result<T>`；`sindre::general::Error` 同时保存 code、message
和 context，调用方可用 `describe()` 生成日志文本。字符串数值解析、`json::parse()`
和 `json::stringify()` 遵循这一约定，不要求调用方通过异常控制普通失败路径。
`Result<T>` 在确认成功后支持 `result->member()`、`(*result).member()` 和 `result.data()`；
其中 `data()` 在失败状态返回 `nullptr`，前两种未检查失败状态时与 `value()` 一样确定性终止。
`data()`、`value_ptr()` 和 `error_ptr()` 必须使用左值 `Result`，禁止从临时结果保存内部指针；
临时结果的 `error()` 和 `Result<T>::value()` 返回独立值。`and_then()` 只能连接返回
`Result<U>` 的回调，并且必须标记 `[[nodiscard]]` 的结果；异常构建会把回调异常转换为
`result.and_then` 错误，无异常构建不得引入异常捕获代码。`Result<void>` 的 const、
右值链式调用必须与 `Result<T>` 保持一致，独立测试入口为 `sindre.general.result`。
进程、动态库、临时文件、环境、URL、版本、自启动、桌面能力、scope guard、ranges 和诊断
封装也必须沿用该约定；平台或第三方后端未启用时返回 `function_not_supported`，不得静默成功。
- `utils_2d`：OpenCV 图像、传统视觉算法和推理预处理；
- `utils_3d`：后端无关的 Mesh、PointCloud、SindreMesh 和几何算法；VTK/CGAL/PCL 仅作为私有实现按需开启，详见 [网格指南](../guides/mesh.md)；
- `utilsav`：音视频能力，预留；
- `ai`：ONNX Runtime CPU/CUDA 和独立 TensorRT 推理；
- `general` 对外按用户用途提供 `core.h`、`system.h`、`runtime.h`、`cli.h`、`string.h`、`network.h` 和 `diag.h`；实现集中在 `modules/general/src/`，内部头不作为稳定公共 API；

### General 网络实现约定

- `network.h` 是唯一网络公共入口；`url.cpp`、`http.cpp`、`transfer.cpp` 和
  `network_common.cpp` 分别负责 URL、HTTP 客户端、上传下载和错误类别。
- 公共 API 只能返回项目自己的 `Result<T>`、`Error` 和 `NetworkErrc`，不得暴露
  `httplib::Client`、`httplib::Result`、`httplib::Request` 或 `httplib::Response`。
- URL 必须先通过 `network::parse()`；编码使用 RFC3986 组件规则，`+` 保持字面值，
  fragment、userinfo、控制字符和非法端口必须拒绝。
- 网络公共函数统一使用 `network::parse()`、`network::get()`、`network::post()`、
  `network::download()` 和 `network::upload()` 等短小全小写名称；网络接口不使用
  `try_` 前缀，失败通过 `Result` 返回。
- 默认请求不重试、不关闭 TLS 校验，并限制响应内存；新增重试、取消、截止时间、流式
  回调或传输逻辑时，必须保留这些默认安全语义。
- cpp-httplib 只能作为 General runtime 的私有实现依赖；安装消费者只验证
  `#include <sindre/general/network.h>`，不能要求 `httplib.h` 出现在安装 include 目录。
- HTTP 集成测试可以在测试 target 私有包含固定版本的 `httplib.h` 启动本地 server，
  但该头不得从 General 的公共 target 传播给用户。
- `utils_py` 独立承载 Python/NumPy 互操作，避免 Python 开发依赖进入 General；
- `apps`：应用级能力，预留。

第三方库只能作为域内实现，不直接决定 sindrecpp 的顶层模块名称。平台差异放在
General 的实现细节和 CMake 中，不以 `platform` 或 `deploy` 作为公共 API 分类。

## 编译器策略

Clang/clang-cl 是首选编译器。公共 target 默认启用：

- Clang：`-Wall -Wextra -Wpedantic`
- clang-cl/MSVC：`/W4 /permissive- /Zc:__cplusplus /utf-8 /bigobj`

可以通过 `SINDRE_WARNINGS_AS_ERRORS=ON` 开启 CI 的警告即错误策略；
General 固定使用静态依赖和静态 MSVC CRT；Windows 宿主必须与 General 使用相同的
静态 CRT 配置，不能把 General 静态库链接到动态 CRT 的宿主中。

## 构建目录和运行时

统一使用 `build/<profile>` 作为构建目录，并使用 Ninja 生成器；生成文件放在对应的
`build/<profile>/bin/` 中。Windows 下 General、AI、GUI、utils_2d 和 utils_3d 的测试/示例
目标会在构建后复制已发现的 DLL 到目标文件同目录，避免加载到系统中不匹配的版本。

## 快捷构建

根目录提供 `scripts/build.bat` 和 `scripts/build.sh`，默认执行 Linux/WSL 或 Windows
Release 配置、编译和模块测试。两个脚本都要求 Ninja；OpenBLAS 为默认 Math 后端，
可以显式指定固定根目录：

```powershell
$env:SINDRE_MATH_OPENBLAS_ROOT = 'C:\sdk\OpenBLAS'
scripts\build.bat
```

```bash
SINDRE_MATH_OPENBLAS_ROOT=/opt/OpenBLAS ./scripts/build.sh
```

也可以把额外的 CMake 选项直接传给脚本，例如
`scripts\build.bat -DSINDRE_WITH_UTILS_2D=ON` 或
`./scripts/build.sh -DSINDRE_MATH_NATIVE_ARCH=OFF`。

固定的 Windows 静态依赖使用 Release ABI。Ninja 等单配置生成器如果显式传入
非 `Release` 的 `CMAKE_BUILD_TYPE` 会在配置阶段直接失败，避免把 Debug CRT 或
iterator-debug ABI 与固定 Release 包混链；多配置生成器应构建 `Release` 配置。

## 安装包消费者验证

安装后包的 CMake 导出也要单独验证，不能只验证源码树内的 target：

```powershell
cmake -S tests/installed_consumer -B build-installed-consumer -G Ninja `
  -DCMAKE_BUILD_TYPE=Release `
  -DCMAKE_PREFIX_PATH="<install>;<fixed-third-party-prefixes>"
cmake --build build-installed-consumer --parallel
ctest --test-dir build-installed-consumer --output-on-failure
```

AI 的安装消费者测试会真实加载并运行固定版本的 ONNX Runtime。Windows
应用应将安装包 `bin/` 放入 PATH，或把所需运行时 DLL 复制到应用目录；否则
系统目录中同名的旧 DLL 可能被优先加载。

## 命名规则

### CMake

```text
项目名：sindrecpp
目标名：sindre::general、sindre::math 或 sindre::utils_3d
选项名：SINDRE_WITH_UTILS_3D；CsString 是 General 的固定依赖，不提供独立开关
```

### C++

```cpp
namespace sindre::general::string {}
namespace sindre::utils_3d {}

namespace sindre::general {
class Result {};
struct Error {};
}

std::string replace_all(...);
bool starts_with(...);
```

规则：

- 命名空间、函数、变量使用小写下划线；
- 类型使用 PascalCase；
- 常量使用 `UPPER_SNAKE_CASE`；
- 公开函数遵循下方“动词前缀 + 对象”规则，不使用没有明确对象的裸动词；
- 状态流程使用 `start`、`done`，不使用 `complete`；
- 不使用 `do_`、`handle_`、`process_` 这类无法表达具体动作的名称。

## 模块设计

每个模块应同时提供：

1. 简单的 sindre API；
2. 必要时提供 `native` 命名空间访问底层库；
3. 独立的 CMake 目标；
4. 最小测试或示例；
5. 文档中的使用方式。

示例：

```cpp
namespace sindre::general::log {
using Logger = spdlog::logger;
namespace native = spdlog;
}
```

### 动词前缀 + 对象

公开函数统一使用“动词前缀 + 对象”的顺序，函数名采用小写下划线。对象部分必须
表达具体的数据、属性或行为，禁止只使用没有语义的 `get()`、`do()`、`handle()`。

| 前缀 | 语义 | 示例 |
| --- | --- | --- |
| `get_` | 获取已有对象或属性，不改变可观察状态 | `get_runtime()`、`get_timeout()` |
| `set_` | 设置一个明确的属性或配置 | `set_timeout()`、`set_output_path()` |
| `change_` | 改变对象的类型、状态或表示形式 | `change_color()`、`change_backend()` |
| `create_` | 创建并返回新对象或资源 | `create_runtime()`、`create_temp_file()` |
| `open_` / `close_` | 打开或关闭句柄、连接、文件等资源 | `open_library()`、`close_watch()` |
| `start_` / `stop_` | 启动或停止可持续运行的任务 | `start_watcher()`、`stop_watcher()` |
| `load_` / `save_` | 在外部介质与内存对象之间读写 | `load_json()`、`save_image()` |
| `parse_` / `format_` | 解析或格式化文本、版本、URL 等值 | `parse_version()`、`format_url()` |
| `convert_` | 在两个明确的数据表示之间转换 | `convert_to_tensor()`、`convert_to_utf8()` |
| `validate_` | 校验输入或对象，不负责隐式修复 | `validate_path()`、`validate_config()` |
| `is_` / `has_` / `can_` | 查询布尔状态、能力或成员存在性 | `is_ready()`、`has_value()`、`can_cancel()` |
| `try_` | 尝试执行可能失败的操作，并返回 `Result` | `try_parse()`、`try_create()` |

命名要求：

- 前缀后必须紧跟具体对象，例如 `get_name()`、`set_level()`，不要使用裸动词；
- `get_` 只用于读取，`set_` 只用于赋值；有转换或状态迁移语义时使用 `change_`；
- `utils_2d::SindreImage` 的可失败成员操作直接使用 `load()`、`resize()`、`save()` 等
  对象动作名，因为返回类型已经是 `Result`；不再叠加 `try_` 前缀；
- 会创建资源的函数使用 `create_`，不会用 `get_` 伪装创建行为；
- 可能失败但不应抛异常穿透公共边界的函数优先使用 `try_`，返回统一 `Result`；
- 同一对象的一组 API 使用一致对象名，例如 `get_timeout()`、`set_timeout()`、
  `change_timeout()`，不要混用 `timeout()`、`update_timeout()` 等近义名称；
- 不使用 `do_`、`handle_`、`process_`、`manage_` 等无法说明具体动作的前缀；
- 类型、枚举和模板参数使用 PascalCase，常量使用 `UPPER_SNAKE_CASE`，命名空间、
  函数和变量使用 `lower_snake_case`。

### 头文件

项目自有的 C/C++ 头文件统一使用 `.h` 后缀，包括模块聚合头、公共头、
内部实现头和测试头。只有第三方库的头文件保留其原始后缀，例如
`opencv2/core.hpp` 和 `argparse/argparse.hpp`。

### General CLI

`sindre::general::cli` 的公共头只依赖 General 自有类型。argparse 仅在
`modules/general/src/cli.cpp` 的编译器异常支持构建中作为私有实现后端使用，不能
出现在公共头、公共 target 的 include 路径或用户代码中。`SINDRE_NO_EXCEPTIONS=ON`
是严格的编译器级无异常模式：CMake 会为 GCC/Clang 添加 `-fno-exceptions`，为
MSVC/clang-cl 添加 `/EHs-c-`，此时由同一源文件中的内部无异常解析器提供与
argparse 等价的行为。

CLI 使用 `Specification::add_option()`、`add_flag()` 和 `add_positional()` 描述选项和
位置参数，声明方式保持接近 Python argparse，避免用户直接构造多层聚合初始化结构。
入口优先使用 `parse_current(spec)`，因此调用者不处理 `argc/argv`。所有解析失败都返回
`Result<Arguments>`；`--help` 和 `--version` 返回 `Arguments::action_text`，不输出、
不退出。`Arguments::has()` 表示存在有效值（默认值也算），`is_set()` 只表示用户
显式提供。新增选项必须覆盖长名称、别名、默认值、必选项、重复项、负数值、`--`
终止符及严格编译器无异常构建的测试。

调用者默认使用 `sindre::general::log`，只有需要底层高级能力时才使用 `sindre::general::log::native`。

System API 的命名也遵循同一规则：文件信息使用 `get_file_size()`、
`get_file_info()`，计算操作使用 `calculate_file_sha256()`，环境变量使用
`get_environment_variable()`/`set_environment_variable()`/`unset_environment_variable()`，
程序定位使用 `get_executable_path()`，自启动使用
`get_startup_location()`、`enable_startup()` 和 `disable_startup()`。目录遍历保留
约定俗成的 `list_directory()` 和 `glob()`；路径编码保留 `to_utf8()`/
`from_utf8()`，因为它们明确表达转换方向；需要错误返回时使用
`try_from_utf8()`。文件系统修改使用 `create_directories()`、`copy_file()`、
`move_path()`、`remove_file()` 和 `remove_directory()`。公共 API 不保留同一行为的
旧兼容别名。

System 的跨平台边界固定为 Windows 和 Linux/WSL；macOS 及其他平台在 CMake 配置阶段
直接拒绝。不得把 Linux 专用行为放进所有非 Windows 分支：平台专属功能必须使用明确的
`_WIN32` 和 `__linux__` 分支；未实现的平台返回
`std::errc::function_not_supported`。文件路径统一按 UTF-8 公共输入处理，平台原生
编码只能在 `system.cpp` 内部转换。

## 错误处理

可预期的失败使用 `Result<T>` 和 `Error`：

```cpp
auto result = load_config(path);
if (!result) {
    // result.error()
}
```

约定：

- 成功使用 `Result::success(...)`；
- 失败使用 `Result::failure(...)`；
- 不混用 `ok`、`fail`、`complete` 等替代命名；
- 错误信息应能说明操作、对象和原因；
- 不要用静默返回空值掩盖失败。

## 依赖管理

- 第三方依赖登记在 `thirds/<module>/`，包括来源、固定版本、许可证/SDK 说明和 CMake cache 覆盖点；
- General 的全部依赖和版本必须固定在 `thirds/general/`，配置时只允许使用固定源码和固定二进制包；其余模块才可以按需接入固定 Git/URL 依赖；
- 优先复用父项目中已经存在的 CMake target；
- 没有现成 target 时才使用 FetchContent；
- 第三方库的测试、示例和文档默认关闭；
- 每个依赖必须固定版本；
- General 不接受宿主环境中同名 target 或系统包覆盖固定版本；缺少固定依赖必须修复依赖配置，不能通过外部包绕过；
- 不把第三方头文件复制进 sindrecpp；
- 不通过全局宏污染宿主项目。

## 测试

本地构建：

```bash
cmake -S . -B build/linux -G Ninja -DCMAKE_BUILD_TYPE=Release -DSINDRE_BUILD_TESTS=ON
cmake --build build/linux
ctest --test-dir build/linux --output-on-failure
```

也可以使用根目录 `CMakePresets.json` 的快捷入口。预设统一使用 Ninja，AI
预设把构建目录分开，避免 Full 和 Dispatch 的缓存互相污染：

```powershell
cmake --list-presets

cmake --preset windows-clang
cmake --build --preset windows-clang

$env:SINDRE_TENSORRT_ROOT = "C:\Program Files\NVIDIA\TensorRT-10.11.0.33"
$env:CUDAToolkit_ROOT = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9"
cmake --preset ai-trt-full
cmake --build --preset ai-trt-full

cmake --preset ai-trt-dispatch
cmake --build --preset ai-trt-dispatch
```

`ai-trt-full` 用于 ONNX 转 engine，`ai-trt-dispatch` 用于只加载已有 engine 的
部署程序。TensorRT 和 CUDA 路径只从环境变量读取，不写入仓库；Linux 使用
`linux-clang`，AI 预设的 Windows clang-cl 配置可按同样方式复制到本机 toolchain。

The General layer is non-throwing at its public boundary in every build. Use
`-DSINDRE_NO_EXCEPTIONS=ON` for the strict compiler-no-exceptions validation build.
This option also adds `-fno-exceptions` for GCC/Clang or `/EHs-c-` for MSVC/clang-cl,
and enables the no-exception settings required by the bundled third-party adapters.
Because argparse relies on C++ exceptions, normal builds use the private argparse
backend while this strict build uses General's equivalent internal CLI parser.

新增模块至少应覆盖：

- 正常输入；
- 空输入；
- 边界输入；
- 失败路径；
- 关闭模块选项后的构建行为。

## 提交前检查

提交前确认：

- 公开名称符合本规范；
- 示例可以独立理解；
- 关闭无关模块后仍能构建；
- 没有引入不必要的兼容别名；
- 没有把内部实现名称暴露给用户；
- README 或对应文档已经更新。


## 按需模块

General 及其固定依赖集成默认开启；AI、GUI、Python、2D、3D 等其他功能域默认关闭。
Result/Error/version 属于 general，无独立 Core 目标。
GUI/Python 域分别为 `gui`/`utils_py`，目录、头文件和命名空间保持一致。
AI 默认选择 ONNX Runtime CPU；CUDA 需要显式开启，独立 TensorRT 显式开启，并可关闭 ORT。
提供 infer、infer_async 和两阶段 Pipeline，错误经 future 传播，关闭排空任务。
未启用 AI 不查找 ORT/GPU SDK。
详见 [推理指南](../guides/inference.md)。
