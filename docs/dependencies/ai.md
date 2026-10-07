# AI dependencies

本文面向需要启用 ONNX Runtime、CUDA 或 TensorRT 的使用者，说明 SDK 来源、版本
边界和为什么这些大型依赖不会由项目自动下载。

| Capability | Dependency | Source | Requirement | Default |
| --- | --- | --- | --- | --- |
| CPU inference | ONNX Runtime C/C++ SDK | bundled `thirds/ai/onnxruntime/1.22.0/` or `SINDRE_ONNXRUNTIME_ROOT` | 1.22+ | AI on, ORT on |
| CUDA inference | ONNX Runtime GPU + CUDA + cuDNN | installed SDK/runtime | matching versions | off |
| TensorRT inference | TensorRT + CUDA Toolkit | installed SDK or `SINDRE_TENSORRT_ROOT` | TensorRT 10.11+, CUDA 12+ | off |

The repository keeps the validated Windows x64 ONNX Runtime 1.22.0 package under
`thirds/ai/onnxruntime/1.22.0/`. CMake uses it automatically when the extracted
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
The AI module copies discovered Windows runtime DLLs beside its executables.

AI does not depend on `utils_py`, Python or NumPy. The SDK headers are consumed
only by `modules/ai/src/*.cpp`; consumers link `sindre::ai` and include only
`include/sindre/ai/*.h`. This is the supported C++-only integration boundary.
The installed package exports an internal `sindre::ai_onnxruntime` wrapper;
consumers do not need to link a raw SDK target.
