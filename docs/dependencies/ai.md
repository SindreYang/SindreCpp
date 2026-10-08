# AI dependencies

本文面向需要启用 ONNX Runtime、CUDA 或 TensorRT 的使用者，说明 SDK 来源、版本
边界和为什么这些大型依赖不会由项目自动下载。

| Capability | Dependency | Source | Requirement | Default |
| --- | --- | --- | --- | --- |
| CPU inference | ONNX Runtime C/C++ SDK | external cache `SINDRE_THIRD_PARTY_CACHE_DIR/ai/onnxruntime/1.22.0/` or `SINDRE_ONNXRUNTIME_ROOT` | 1.22+ | AI on, ORT on |
| CUDA inference | ONNX Runtime GPU + CUDA + cuDNN | installed SDK/runtime | matching versions | off |
| TensorRT inference | TensorRT + CUDA Toolkit | installed SDK or `SINDRE_TENSORRT_ROOT` | TensorRT 10.11+, CUDA 12+ | off |

The repository keeps the validated Windows x64 ONNX Runtime 1.22.0 package under
The external cache under `SINDRE_THIRD_PARTY_CACHE_DIR/ai/onnxruntime/1.22.0/` is used automatically when the extracted
package is present; set `SINDRE_ONNXRUNTIME_ROOT` to override it. The downloaded
archive is a local cache and is not required by CMake after extraction. When the
fixed package is used, `cmake --install` also installs its headers, import
library, runtime DLL and license notices with the SindreCpp package, so an
installed C++ consumer does not need to rediscover the SDK.

On Windows, place the installed `bin/onnxruntime.dll` beside the consumer
executable or prepend that directory to `PATH`. This is important on machines
that already have a different `onnxruntime.dll` in `System32`; the system DLL
can otherwise be loaded before the package runtime and report an older API.

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

On the validated Windows FCN example, a Dispatch deployment containing
`nvinfer_dispatch_10.dll`, `nvinfer_lean_10.dll`, `nvinfer_plugin_10.dll` and
`cudart64_12.dll` was 96.30 MiB, plus a 70.19 MiB engine. The same example
ran from a clean directory without `cublas64_12.dll` or `cublasLt64_12.dll` and
produced the same mask. This is a model-specific smoke-test result, not a
universal TensorRT packaging rule; retain cuBLAS libraries when the engine or
plugin requires them. A Full load-only package is much larger because the
builder runtime is about 454.56 MiB and the monolithic Sindre translation unit
also links the ONNX parser.

The minimum validated runtime list is therefore not a promise for arbitrary
engines. Before release, inspect the engine's actual plugin/tactic usage and
run the executable in an empty deployment directory. Do not remove a DLL just
because it is not a direct import in `dumpbin`; TensorRT may load optional
dependencies dynamically.

AI does not depend on `utils_py`, Python or NumPy. The SDK headers are consumed
only by `modules/ai/src/*.cpp`; consumers link `sindre::ai` and include only
`include/sindre/ai/*.h`. This is the supported C++-only integration boundary.
The installed package exports an internal `sindre::ai_onnxruntime` wrapper;
consumers do not need to link a raw SDK target.
