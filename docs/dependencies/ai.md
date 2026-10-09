# AI dependencies

本文面向需要启用 ONNX Runtime、CUDA 或 TensorRT 的使用者，说明 SDK 来源、版本
边界和为什么这些大型依赖不会由项目自动下载。

| Capability | Dependency | Source | Requirement | Default |
| --- | --- | --- | --- | --- |
| CPU inference | ONNX Runtime C/C++ SDK | host SDK, explicit `SINDRE_ONNXRUNTIME_ROOT`, or a pre-populated external cache | 1.22+ | AI on, ORT on |
| CUDA inference | ONNX Runtime GPU + CUDA + cuDNN | installed SDK/runtime | matching versions | off |
| TensorRT inference | TensorRT + CUDA Toolkit | installed SDK or `SINDRE_TENSORRT_ROOT` | TensorRT 10.11+, CUDA 12+ | off |

The validated Windows x64 and Linux x64 ONNX Runtime packages are supplied
outside the source repository. A pre-populated external cache under
`SINDRE_THIRD_PARTY_CACHE_DIR/ai/onnxruntime/1.22.0/` is accepted when present;
otherwise set `SINDRE_ONNXRUNTIME_ROOT` or provide an installed CMake package.
The extracted SDK is a local external cache, not a repository dependency and is
not silently downloaded by the project. When an explicit SDK root is used,
`cmake --install` also installs its headers, import library/shared objects,
runtime DLLs and license notices with the SindreCpp package, so an installed C++
consumer does not need to rediscover that SDK. On Linux and macOS the shared
objects are installed under `lib/sindre/onnxruntime/`; the exported target adds
the corresponding relative runtime search path.
Runtime DLLs found in the SDK root, `bin/`, or `lib/` are installed as a
flat set directly under the package `bin/`; the SDK's `include/` and `lib/`
directory trees are not copied into the runtime directory.

On Windows, place the installed `bin/onnxruntime.dll` beside the consumer
executable or prepend that directory to `PATH`. This is important on machines
that already have a different `onnxruntime.dll` in `System32`; the system DLL
can otherwise be loaded before the package runtime and report an older API.

The Linux CPU profile was validated with the official ONNX Runtime 1.22.0 x64
SDK (archive SHA256
`8344d55f93d5bc5021ce342db50f62079daf39aaafb5d311a451846228be49b3`, about
7.4 MiB compressed and about 20.1 MiB of shared objects). The source AI suite
passed 12/12 and an installed-package consumer loaded `identity.onnx` and passed
1/1. This is CPU-only evidence; it does not validate CUDA or TensorRT providers.

TensorRT is intentionally not fetched automatically: it is a large binary SDK
and must match the host compiler, GPU driver, CUDA and cuDNN.
The AI module supports `SINDRE_AI_TRT_RUNTIME=FULL` and `DISPATCH`.
`FULL` is required for ONNX parsing/building. `DISPATCH` is load-only and uses
`nvinfer_dispatch_10.dll` plus an external `nvinfer_lean_10.dll`; its ONNX
conversion API deliberately returns `function_not_supported`.

For a version-compatible plan, build with `VERSION_COMPATIBLE` and
`EXCLUDE_LEAN_RUNTIME` (the public API fields are
`BuildOptions::version_compatible` and `BuildOptions::exclude_lean_runtime`).
Dispatch loads it with the matching external Lean Runtime through
`LoadOptions::lean_runtime_path`; Full Runtime can load the same plan without
that argument. When Dispatch is used, the Lean DLL must be deployed and
version-pinned with the engine package. `allow_engine_host_code` remains off
by default and must only be enabled for a trusted engine that actually needs
embedded host code.

On the validated Windows identity engine, a clean Dispatch deployment containing
only `CsString.dll`, `nvinfer_dispatch_10.dll`, `nvinfer_lean_10.dll`,
`nvinfer_plugin_10.dll` and `cudart64_12.dll` was 96.39 MiB for runtime DLLs
(97.78 MiB including the test executable), and the engine was 15.86 KiB. The
executable ran successfully without `nvinfer_10.dll`, the builder resource,
`nvonnxparser_10.dll`, `cublas64_12.dll` or `cublasLt64_12.dll`. This is a
model-specific smoke-test result, not a universal TensorRT packaging rule;
retain optional libraries when the engine or plugin requires them. A Full
load-only package is much larger because the builder runtime is about 454.56
MiB and the monolithic Sindre translation unit also links the ONNX parser.

The minimum validated runtime list is therefore not a promise for arbitrary
engines. Before release, inspect the engine's actual plugin/tactic usage and
run the executable in an empty deployment directory. Do not remove a DLL just
because it is not a direct import in `dumpbin`; TensorRT may load optional
dependencies dynamically.

The current Windows validation builds a `VERSION_COMPATIBLE` and
`EXCLUDE_LEAN_RUNTIME` engine with TensorRT 10.11.0.33 and CUDA 12.9 using
`clang-cl`, copies the configured SDK runtime DLLs beside the test executable,
and runs identity ONNX inference on an RTX 3060 Laptop GPU. The FULL test
passed, including the expected invalid-profile rejection. The same engine was
then loaded and executed by DISPATCH with the external
`nvinfer_lean_10.dll`; that smoke test also passed. This validates the
repository's test layouts, but does not turn the large TensorRT builder DLL
into a recommended deployment dependency.

AI does not depend on `utils_py`, Python or NumPy. The SDK headers are consumed
only by `modules/ai/src/*.cpp`; consumers link `sindre::ai` and include only
`include/sindre/ai/*.h`. This is the supported C++-only integration boundary.
The installed package exports an internal `sindre::ai_onnxruntime` wrapper;
consumers do not need to link a raw SDK target.

### 无异常构建边界

当前 AI 的 `infer()`、`Model` 构造函数和 `Pipeline`/`Executor` 便捷接口仍是
异常语义；其 `try_*` 实现也依赖异常后端来捕获第三方 SDK 错误，因此目前不属于
`SINDRE_NO_EXCEPTIONS=ON` 的可用接口。CMake 会在配置阶段拒绝无异常 AI 构建；在迁移完成前，
不能声称 AI 支持严格 `-fno-exceptions`。General、Math、Utils2D 和 UtilsPy
仍须单独按配置验证，不能用 AI 的无异常限制掩盖其他模块的编译结果。
