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

The ONNX Runtime model also exposes `try_create`, `try_infer`,
`try_infer_typed`, `try_infer_into`, `try_infer_typed_into`, `try_warm_up`, and
`try_infer_async`. These methods use General `Result` values and stable
`ai.onnxruntime.*` error contexts for applications that do not use exceptions
as ordinary control flow.

The bundled official ONNX Runtime 1.22.0 Windows x64 package was compiled and
run with clang-cl. CPU float32, int32, float16, int64 and bool model tests all
passed, including the public typed and asynchronous execution paths.

## Test order

Run `sindre.ai.execution` first, then ONNX Runtime float/int32/float16 and
typed int64/bool tests. On Windows, the CMake test properties prepend the
backend runtime directories to PATH.
