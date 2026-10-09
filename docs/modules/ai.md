# AI module

本文面向需要在 C++ 中运行 ONNX 或 TensorRT 推理的使用者，介绍 target、开关、
后端边界和测试入口；依赖版本请配合 [AI 依赖说明](../dependencies/ai.md) 阅读。

`sindre::ai` is a static library target. The executor lifecycle and all
ONNX Runtime/TensorRT backend implementations are compiled into `sindre_ai`.
Public AI headers expose only SindreCpp data types; backend SDK headers and
native resources stay inside the AI translation units.

AI is completely implemented in C++ translation units. Python, NumPy and
pybind11 are not part of `sindre::ai` and are not required to build or run the
ONNX Runtime/TensorRT backends. This keeps the AI ABI usable from ordinary C++
applications and prevents backend SDK types from leaking through public
headers.

The ONNX Runtime and TensorRT native session/configuration objects are not
part of the public ABI. TensorRT device buffers use opaque `void*` handles in
`DeviceTensorView`; applications that need native SDK customization should
use a dedicated backend adapter rather than including SDK headers through
SindreCpp.

## Tensor、reshape 与精度

`sindre::ai::Tensor` 是连续的 `float32` 主机张量，支持任意 rank；4D 和 5D
输入都只需要把完整 shape 放入 `shape`：

```cpp
sindre::ai::Tensor image{{1, 3, 224, 224}, std::vector<float>(1 * 3 * 224 * 224)};
sindre::ai::Tensor volume{{1, 3, 16, 224, 224},
                          std::vector<float>(1 * 3 * 16 * 224 * 224)};
```

reshape 不移动数据，只改变 shape，并且必须保持元素数量不变；允许一个 `-1`
自动推导维度：

```cpp
auto status = image.try_reshape({1, 3, 50176});
if (!status) {
    // status.error().code/message/context
}
```

其他精度使用 `sindre::ai::TypedTensor`。当前通用类型包括 `float16`、
`bfloat16`、`float64`、有符号/无符号整数和 `bool8`。原始字节入口适合
半精度和模型自定义布局；`try_from<T>()` 适合标准 C++ 数值容器：

```cpp
auto input = sindre::ai::TypedTensor::try_from<std::int32_t>(
    {1, 3}, {1, 2, 3});
if (!input) {
    // shape、元素数量或字节数错误
}
auto output = model->try_infer_typed({input.value()});
```

`try_reshape()`、`try_validate()`、`try_from()` 和 `try_from_bytes()` 不通过
异常表达可预期输入错误。`infer_typed()` 可用于 ONNX Runtime；TensorRT
便利接口支持引擎实际声明的线性 `float32`、`float16`、`int8`、`int32` 和
`bool8` I/O。不同后端仍必须以 `get_inputs()` 返回的精度和 shape 为准。

The CPU/GPU backend implementations require the matching ONNX Runtime or
TensorRT SDK to be present. ONNX Runtime is a large external SDK supplied by
the host or by an explicitly configured package prefix; this repository only
pins its minimum supported version and does not copy the SDK into
`3rdparty/` or silently download it. The validated Windows package was
ONNX Runtime GPU 1.26.0. TensorRT remains an installed SDK because it must
match the host CUDA and GPU driver. Without those SDKs, only the
backend-independent AI execution target can be compiled and tested.

## CMake switches

- `SINDRE_WITH_AI=ON` enables the module and `sindre::ai`.
- `SINDRE_AI_ONNXRUNTIME=ON` enables ONNX Runtime discovery.
- `SINDRE_AI_TRT=ON` enables TensorRT and CUDA discovery.
- `SINDRE_AI_CUDA=ON` enables ONNX Runtime CUDA provider compilation.
- `SINDRE_BUILD_GPU_TESTS=ON` enables real TensorRT execution tests.

Use `SINDRE_ONNXRUNTIME_ROOT`, `SINDRE_TENSORRT_ROOT`, and
`SINDRE_CUDNN_ROOT` when SDKs are not discoverable.

根目录提供 `CMakePresets.json` 快速入口：

```powershell
$env:SINDRE_THIRD_GENERAL_PACKAGE_CACHE_ROOT = "F:\My_Github\SindreCpp\.sindre_cache\SindreCpp\general\packages"
$env:SINDRE_TENSORRT_ROOT = "C:\Program Files\NVIDIA\TensorRT-10.11.0.33"
$env:CUDAToolkit_ROOT = "C:\Program Files\NVIDIA GPU Computing Toolkit\CUDA\v12.9"

cmake --preset ai-trt-full
cmake --build --preset ai-trt-full

cmake --preset ai-trt-dispatch
cmake --build --preset ai-trt-dispatch
```

两个预设分别使用 `build_win/ai-trt-full` 和 `build_win/ai-trt-dispatch`，不能共用同一个构建
目录。Full 预设用于构建 ONNX engine，Dispatch 预设用于生成小体积推理程序。

The ONNX Runtime backend also exposes `try_get_available_backends`; the model
exposes `try_create`, `try_infer`,
`try_infer_typed`, `try_infer_into`, `try_infer_typed_into`, `try_warm_up`,
`try_infer_async`, and `try_infer_typed_async`. These methods use General
`Result` values and stable
`ai.onnxruntime.*` error contexts for applications that do not use exceptions
as ordinary control flow. TensorRT exposes the corresponding `try_create`,
`try_infer`, `try_infer_typed`, `try_warm_up`, `try_enqueue*`, pending-ticket
`try_*` methods, and cancellable `try_infer_async` methods. The throwing
convenience methods remain available for exception-oriented applications, but
production hosts should use the `try_*` boundary and inspect `Error.code`,
`Error.message`, and `Error.context`. The async overload accepting
`sindre::general::TaskOptions` carries cancellation, a steady-clock deadline,
and the shared progress callback contract; the backend reports completion as a
single inference task.

The repository contains the CMake wiring for an external ONNX Runtime SDK
whose version must satisfy the pinned 1.22+ minimum. Backend execution is only considered
validated when the matching SDK, runtime DLLs, compiler, and test command are
available on the target machine; a configure or compile check alone does not
prove provider loading or inference correctness.

## TensorRT 生产部署示例

`examples/ai_tensorrt_segmentation` 是独立可配置的完整样例，使用 ONNX Model
Zoo 的 FCN ResNet-50 语义分割模型、PPM 输入和 PGM/PPM 输出，不依赖 OpenCV。它
展示了生产代码应使用的 `try_convert_onnx()`、`Model::try_create()` 和
`try_infer()` 错误边界，以及固定尺寸 optimization profile。示例 README：

[`TensorRT 语义分割示例`](../guides/ai_tensorrt_segmentation.md)

TensorRT 构建策略应按部署目标选择：

- 默认 `same_compute_capability`，在同一计算能力族上通常保留更多 tactics，适合
  性能优先部署；
- `--portable` 使用 `ampere_plus`，适合在 Ampere 及更新架构之间迁移，但可能牺牲
  性能；
- `version_compatible` 用于跨 TensorRT 版本，`exclude_lean_runtime` 可把 lean
  runtime 从 plan 中排除，让多个 engine 共享一份外部运行时；加载时通过
  `LoadOptions::lean_runtime_path` 指定它；
- `allow_engine_host_code` 默认关闭，只有 engine 来源受信任且确实需要 host code
  时才打开。

这些选项并不能替代目标 GPU、CUDA、TensorRT 版本的实际 smoke test。Windows 发布
包必须把匹配的 CUDA/TensorRT/lean runtime DLL 放在可搜索目录，并固定版本和哈希。

### 兼容模式的实测差异

当前在 RTX 3060 Laptop（计算能力 8.6）、TensorRT 10.11.0.33、CUDA 12.9、
FCN ResNet-50、FP16、输入 `1x3x224x224` 上，用 `trtexec --loadEngine`、预热
1 秒、运行 5 秒、`--noDataTransfers` 测得：

| Engine 配置 | GPU 延迟均值 | 吞吐 | 相对 `build_gpu` 延迟 | 相对吞吐 |
| --- | ---: | ---: | ---: | ---: |
| `build_gpu` | 2.37257 ms | 421.071 qps | 基准 | 基准 |
| `same_compute_capability` | 2.38387 ms | 419.043 qps | +0.48% | -0.48% |
| `ampere_plus`（`--portable`） | 3.47930 ms | 287.225 qps | +46.65% | -31.79% |

三个配置在 `sample`、`checker`、`noise` 三个确定性输入上的输出 mask 均为字节级
一致，当前验证没有观察到输出差异。但这不是带标注数据集上的 mIoU 或数值误差
评估；正式项目仍需用业务验证集测 mIoU、类别召回和端到端延迟。

`VERSION_COMPATIBLE`、Full/Dispatch 和最小 DLL 集合使用的是同一份外置 Lean
engine；当前 smoke test 的 mask 也字节级一致。运行时 DLL 裁剪本身不会改变已经
编译进 engine 的 tactics，性能差异主要来自 engine 构建配置，而不是删除未使用的
DLL。若重新构建时禁用 cuBLAS/cuBLASLt tactic source，必须单独重新做速度和精度
对比，不能沿用上表结论。

### Full / Dispatch / Lean 部署

TensorRT 后端支持两种链接模式，由 `SINDRE_AI_TRT_RUNTIME` 选择：

- `FULL`（默认）：链接完整 `nvinfer`、`nvinfer_plugin` 和 `nvonnxparser`，可在程序内
  执行 `try_convert_onnx()`；适合构建机或需要现场转换 ONNX 的工具。
- `DISPATCH`：只链接 `nvinfer_dispatch` 和插件库，不带 builder/parser；它是加载模式，
  `try_convert_onnx()` 会返回 `function_not_supported`。创建 `Model` 时必须在
  `LoadOptions::lean_runtime_path` 提供外部 `nvinfer_lean_10.dll`。

例如分别构建 Full 和 Dispatch：

```powershell
cmake --preset ai-trt-full
cmake --build --preset ai-trt-full

cmake --preset ai-trt-dispatch
cmake --build --preset ai-trt-dispatch
```

推荐在 Full 构建机上生成版本兼容且不内嵌 Lean 的 plan：

```text
--version-compatible --exclude-lean-runtime \
--lean-runtime C:/Program Files/NVIDIA/TensorRT-10.11.0.33/lib/nvinfer_lean_10.dll
```

其中 `--lean-runtime` 在构建阶段用于验证外置 Lean；使用 Dispatch 部署时仍需把同一
版本的 Lean DLL 与 Dispatch 程序一起提供。Full Runtime 可以直接加载不含内嵌 Lean
的 plan，不强制提供外部 Lean。`EXCLUDE_LEAN_RUNTIME` 不能单独使用，必须和
`VERSION_COMPATIBLE` 同时设置。Dispatch 不等于跨 GPU：plan 仍受其构建时的 GPU、
硬件兼容级别、CUDA/TensorRT 主版本和插件约束，必须在目标机做真实加载与推理验证。

本机 RTX 3060 Laptop、TensorRT 10.11.0.33、CUDA 12.9 的 FCN ResNet-50 示例已验证：
外置 Lean 的 version-compatible plan 可以由 Full 和 Dispatch 两种程序加载，输出
mask SHA-256 完全相同；Dispatch 版本在没有 cuBLAS/cuBLASLt DLL 的干净目录仍能完成
推理。这个结果只代表该 engine 和插件集合，不能据此保证任意模型都能删除 cuBLAS。

## Test order

Run `sindre.ai.execution` first, then ONNX Runtime float/int32/float16 and
typed int64/bool tests. On Windows, the CMake test properties prepend the
backend runtime directories to PATH.
