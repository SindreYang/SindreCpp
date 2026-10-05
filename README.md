# SindreCpp

C++17 capability toolkit following Sindre naming. All third-party dependencies are opt-in.

## Modules

| Option | Target | Header / namespace | Scope |
| --- | --- | --- | --- |
| SINDRECPP_WITH_GENERAL | SindreCpp::General | general.hpp / general | Result, Error, version, strings, pointers, optional integrations |
| SINDRECPP_WITH_UTILS_GUI | SindreCpp::Utils_gui | utils_gui.hpp / utils_gui | ImGui context lifetime |
| SINDRECPP_WITH_UTILS_PY | SindreCpp::Utils_py | utils_py.hpp / utils_py | Python embedding and NumPy conversion |
| SINDRECPP_WITH_UTILS2D | SindreCpp::Utils2d | utils2d.hpp / utils2d | OpenCV images and preprocessing |
| SINDRECPP_WITH_UTILS3D | SindreCpp::Utils3d | utils3d.hpp / utils3d | VTK core, fast SindreMesh entry point, Eigen, optional geometry backends |
| SINDRECPP_WITH_AI | SindreCpp::Ai | ai.hpp / ai | Tensor types, async execution, pipeline and selected backends |
| SINDRECPP_AI_ONNXRUNTIME | SindreCpp::OnnxRuntime | ai/onnxruntime.hpp / ai::onnxruntime | Independent ONNX Runtime CPU/CUDA inference |
| SINDRECPP_AI_TRT | SindreCpp::Trt | ai/trt.hpp / ai::trt | Independent TensorRT conversion and engine inference |

Only dependency-free general defaults ON and can be disabled.
All other domains/integrations default OFF. AI defaults to the ONNX Runtime backend and CUDA support
when enabled; native TensorRT is opt-in. The backend switches only apply when AI is enabled.
TensorRT does not link ONNX Runtime. Set AI_ONNXRUNTIME=OFF for a TRT-only build.
General integrations use SINDRECPP_WITH_POINTER, STRING, LOG, HTTP, JSON and CLI (all OFF).
Core/Gui/Python old targets, namespaces and headers have been removed without aliases.
Base strings and pointers work without CsString/CsPointer.

## Add to a project

```cmake
include(FetchContent)
set(SINDRECPP_WITH_AI ON CACHE BOOL "")
set(SINDRECPP_WITH_UTILS2D ON CACHE BOOL "")
set(SINDRECPP_ONNXRUNTIME_ROOT "/path/to/onnxruntime" CACHE PATH "")
FetchContent_Declare(SindreCpp
    GIT_REPOSITORY https://github.com/SindreYang/SindreCpp.git
    GIT_TAG main) # Pin a tested commit for production.
FetchContent_MakeAvailable(SindreCpp)
add_executable(my_app main.cpp)
target_link_libraries(my_app PRIVATE SindreCpp::Ai SindreCpp::Utils2d)
```

SindreCpp::SindreCpp aggregates enabled domains without enabling additional dependencies.
OpenCV uses OpenCV_DIR (core/imgproc/imgcodecs); ORT uses its SDK root or parent targets.
TensorRT uses SINDRECPP_TENSORRT_ROOT and a CUDA Toolkit. SDKs are not built/downloaded by SindreCpp.

## Synchronous, asynchronous and pipeline inference

```cpp
#include <sindrecpp/ai.hpp>

sindrecpp::ai::onnxruntime::Model model("model.onnx"); // CUDA default
sindrecpp::ai::Tensors input{ /* dense float32 tensors in model input order */ };
auto output = model.infer(input);
auto future = model.infer_async(input);
auto async_output = future.get(); // Errors are rethrown here.
```

Both backend Models provide infer, infer_async and warm_up.
Pipeline<Input, Prepared, Output> has separate preprocessing/inference workers, so CPU work for
the next item overlaps inference for the current one. Bounded queues reject excess submissions;
close drains accepted work. Capture shared model ownership in callbacks.
See [docs/inference.md](docs/inference.md) for complete pipeline usage and ownership rules.

## Standalone TensorRT

```cmake
set(SINDRECPP_WITH_AI ON CACHE BOOL "")
set(SINDRECPP_AI_ONNXRUNTIME OFF CACHE BOOL "")
set(SINDRECPP_AI_TRT ON CACHE BOOL "")
set(SINDRECPP_TENSORRT_ROOT "/path/to/TensorRT-10.13.x" CACHE PATH "")
# After FetchContent_MakeAvailable:
target_link_libraries(my_app PRIVATE SindreCpp::Trt)
```

```cpp
#include <sindrecpp/ai/trt.hpp>

namespace trt = sindrecpp::ai::trt;
auto options = trt::BuildOptions::max_performance();
options.fp16 = true; // User must validate precision.
options.workspace_bytes = std::size_t{4} << 30;
options.profiles = {{"images", {1, 3, 640, 640}, {4, 3, 640, 640}, {8, 3, 640, 640}}};
trt::convert_onnx("model.onnx", "model.engine", options);
trt::Model model("model.engine");
auto future = model.infer_async(input);
auto output = future.get();
```

BuildOptions::cross_gpu selects Ampere-and-newer compatibility, not arbitrary NVIDIA GPUs.
same_compute_capability is a narrower compatibility choice. Hardware compatibility, precision,
workspace, optimization level, auxiliary streams, profiles and version compatibility are user-controlled.
Presets are starting points, not proof of maximum performance.

TRT enqueue returns a CUDA completion ticket (ready/get/wait).
enqueue_device binds caller-owned GPU input/output buffers without host copies.
Pinned host staging/device allocations are reused by the host-input interface.
At most one outstanding CUDA ticket per Model; use multiple Models/streams for concurrency.
The engine loader accepts trusted plans only; embedded runtime code needs explicit permission.

## Images and other utilities

utils2d: load/save, resize/crop, color conversion, normalize, letterbox and to_tensor.
Default to_tensor: BGR -> RGB, NCHW, float32, pixel/255; preprocessing must match the model.
Crop returns an independent copy; letterbox returns scale/padding.

Result/Error/version now live under sindrecpp::general. trim/split return views; keep source text alive.
Optional log exposes warning() and rotating_file(name, path), 10 MiB/file and five retained files.
utils_gui manages context only; host owns window/render backends.
utils_py needs Python development files; NumPy helpers need NumPy at runtime.
Header inclusion alone does not create an interpreter/context/model.

utils3d supports SINDRECPP_UTILS3D_BLAS_BACKEND=AUTO/EIGEN/BLAS.
It requires VTK 9 when enabled. MeshLib/CGAL/Open3D/libigl/VCG are independently opt-in via
SINDRECPP_UTILS3D_MESHLIB/CGAL/OPEN3D/IGL/VCG. MeshLib requires C++20; the base API remains C++17.
See [Mesh guide](docs/mesh.md) for algorithms, backend priority, attributes and NumPy/Eigen interchange.
Opt-in `SINDRECPP_UTILS3D_SHOW` adds the standalone `show_mesh` viewer called by `mesh.show()`.
Opt-in `SINDRECPP_UTILS3D_VTK_DATA` adds datasets, images and scientific filters; with SHOW it also
supports volumes, image slices and charts. See [VTK guide and coverage checklist](docs/vtk.md).
Use SINDRECPP_UTILS3D_NATIVE_ARCH=OFF for portable/cross-compiled binaries.
SINDRECPP_BUILD_UTILS3D_BENCHMARKS enables the optional GEMM benchmark.

## Build and test

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

For inference fixtures: pip install onnx==1.17.0; python tests/create_test_model.py.
CPU builds require ORT 1.22+ matching headers/runtime; GPU ORT also needs matching CUDA/cuDNN.
Native TRT targets 10.13.x and CUDA Toolkit 12+; TensorRT 11 is deliberately rejected.
For CPU-only ORT, set SINDRECPP_AI_CUDA=OFF and explicitly select Options.backend=Backend::cpu.

CI runs Windows/Linux integrations, general-only/all-disabled builds, OpenCV/ORT CPU execution,
async/queue/pipeline tests and native TRT API compilation. GPU conversion/inference tests are opt-in
with SINDRECPP_BUILD_GPU_TESTS=ON and require real GPU hardware.

[Inference guide](docs/inference.md) · [Development](docs/development.md) · [Notes](docs/notes.md).
MIT; dependencies keep their licenses.
