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
TensorRT SDK to be present. The validated Windows ONNX Runtime package is
fixed under `thirds/ai` and is automatically used by default. TensorRT remains
an installed SDK because it must match the host CUDA and GPU driver. Without
those SDKs, only the backend-independent AI execution target can be compiled
and tested.

## CMake switches

- `SINDRE_WITH_AI=ON` enables the module and `sindre::ai`.
- `SINDRE_AI_ONNXRUNTIME=ON` enables ONNX Runtime discovery.
- `SINDRE_AI_TRT=ON` enables TensorRT and CUDA discovery.
- `SINDRE_AI_CUDA=ON` enables ONNX Runtime CUDA provider compilation.
- `SINDRE_BUILD_GPU_TESTS=ON` enables real TensorRT execution tests.

Use `SINDRE_ONNXRUNTIME_ROOT`, `SINDRE_TENSORRT_ROOT`, and
`SINDRE_CUDNN_ROOT` when SDKs are not discoverable.

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

The repository contains the CMake wiring and fixtures for the bundled ONNX
Runtime 1.22.0 Windows x64 package. Backend execution is only considered
validated when the matching SDK, runtime DLLs, compiler, and test command are
available on the target machine; a configure or compile check alone does not
prove provider loading or inference correctness.

## TensorRT 生产部署示例

`examples/ai_tensorrt_segmentation` 是独立可配置的完整样例，使用 ONNX Model
Zoo 的 FCN ResNet-50 语义分割模型、PPM 输入和 PGM/PPM 输出，不依赖 OpenCV。它
展示了生产代码应使用的 `try_convert_onnx()`、`Model::try_create()` 和
`try_infer()` 错误边界，以及固定尺寸 optimization profile。示例 README：

[`examples/ai_tensorrt_segmentation/README.md`](../../examples/ai_tensorrt_segmentation/README.md)

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

## Test order

Run `sindre.ai.execution` first, then ONNX Runtime float/int32/float16 and
typed int64/bool tests. On Windows, the CMake test properties prepend the
backend runtime directories to PATH.
