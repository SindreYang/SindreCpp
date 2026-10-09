# sindrecpp repository guidance

本文件只保留仓库协作时必须遵守的规则；详细架构、模块边界、命名、构建和测试
说明统一放在 [`docs/development/`](docs/development/development.md)。使用者文档
从 [`docs/README.md`](docs/README.md) 开始。

## 当前结构

- 公共头文件唯一位置：`include/sindre/`。
- 模块实现和测试位置：`modules/<module>/{src,tests,CMakeLists.txt}`。
- 模块公共 target：`sindre::general`、`sindre::math`、`sindre::utils_py`、`sindre::ai`、
  `sindre::gui`、`sindre::utils_2d`、`sindre::utils_3d`。
- 项目文档唯一位置：`docs/`，分为 `guides/`、`modules/`、`dependencies/`、
  `development/`；代码目录不放 README 或模块文档。
- 第三方依赖登记在 `3rdparty/` 及其依赖子目录，不把第三方头文件复制进项目。
- 所有本项目产生的下载包、解压目录、依赖构建树、安装前缀、构建目录和测试产物
  必须位于仓库根目录 `F:\My_Github\SindreCpp` 内（例如 `build/`、`build_win/`、
  `build_linux/` 和被 `.gitignore` 排除的缓存目录）。禁止写入 `D:\software`、
  `F:\SindreCppCache`、用户目录、WSL `/home` 或其他仓库外路径；仓库外 SDK 只能
  只读使用，若必须下载或安装必须先得到用户明确授权。

- Windows 构建只能使用 `build_win/`，Linux/WSL 构建只能使用 `build_linux/`；不同
  配置应在这两个目录下使用有意义的子目录隔离，例如 `build_linux/core/` 或
  `build_win/ai-trt-dispatch/`，不得在仓库根目录散落 `build_*` 目录。
- WSL 即使为了规避 `/mnt/f` 的 9P 性能问题，也不得把本项目的构建树、安装前缀或
  测试产物长期放在 WSL `/home`、容器卷或临时目录。确需使用临时 ext4 构建时，
  完成验证后必须迁回仓库的 `build_linux/`；工具虚拟环境（例如 `cmake-venv`）
  与构建目录分开管理，不得混同。

不要恢复旧的 `include/<module>/index.h`、模块本地公共头、Json/Http/Log 独立
target 或 catch-all 聚合 target。项目自己的头文件统一使用 `.h`，第三方头文件
保留上游后缀。

## 不可破坏的公共约定

- C++17 是最低标准。
- 可失败的公共操作优先返回 `sindre::general::Result<T>`；错误保留 `code`、
  `message` 和 `context`。
- 新公共接口不得让第三方异常穿透错误边界；平台或后端未启用时返回明确错误。
- 可选依赖只通过所属模块的 CMake target 传递。
- AI、并发和异步接口不得丢失取消、超时/截止时间、重试和进度语义。
- 公共函数遵循“动词前缀 + 对象”，例如 `get_xxx`、`set_xxx`、`change_xxx`、
  `try_xxx`；完整前缀表和命名规则见开发指南。网络入口是简洁例外：统一放在
  `sindre::general::network`，采用全小写短名称（`parse`、`get`、`post`、
  `download`、`upload`），失败仍通过 `Result` 表达，不使用 `try_` 前缀。

## 工作流程

1. 先读对应的模块、依赖和指南文档，再决定实现位置。
2. 检查 Git 状态、现有代码、CMake target 和测试，保留用户已有修改。
3. 只修改负责该能力的模块，避免把可选依赖扩散到 General 或宿主项目。
4. 修改公共头或公共依赖后，至少构建 General 和受影响模块并运行测试。
5. 行为、API、开关或依赖变化时同步更新 `docs/` 中对应文档和示例。
6. 检查 Markdown 链接、`git diff --check`，并区分编译验证和真实运行验证。

涉及 GUI、网络、Python、AI 或后端 SDK 时，不得只凭编译或静态检查声称功能完成；
需要实际启动对应程序或测试，并报告环境限制。

## 构建入口

从仓库根目录构建。Windows 使用 `scripts\build.bat` 自动发现并初始化 Visual Studio
工具链，再用 Ninja + ClangCL；Linux/WSL 使用 `scripts/build.sh` 检查工具后再用 Ninja
和 Clang。默认配置为 `RelWithDebInfo`：

```powershell
scripts\build.bat
```

Windows 如需直接调用 CMake，必须先进入 Visual Studio x64 Developer Command Prompt；
普通 PowerShell 不保证 PATH 中存在 `cl`、`clang-cl` 和 `ninja`。

Linux/WSL：

```bash
./scripts/build.sh
```

构建目录固定为 `build_win/` 和 `build_linux/`；不要为普通测试或模块验证在
仓库根目录创建临时 `build_*` 目录。

WSL 构建也必须落在仓库内的 `build_linux/`。如果从 WSL ext4 临时迁回项目目录，
只能迁移到 `build_linux/<profile>/`，并在迁移完成后确认源目录已消失、目标目录存在。
由于 CMake 缓存可能保存旧的绝对路径，迁移后的构建目录在继续使用前必须重新运行
对应的配置命令或预设；不能仅凭目录移动结果声称缓存仍然可复用。

模块开关、依赖要求、编译器策略、测试边界和模块内部规则以 `docs/` 为准；当仓库
规则变化时更新本文件，并避免复制一整套长期说明造成双份规范。
