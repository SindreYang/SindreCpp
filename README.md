# SindreCpp

C++17 capability toolkit following Sindre naming. All third-party dependencies are opt-in.

## Modules

| Option | Target | Header / namespace | Scope |
| --- | --- | --- | --- |
| SINDRECPP_WITH_GENERAL | SindreCpp::General | general/index.hpp / general | Result, Error, text, data, filesystem, network, runtime and host helpers |
| SINDRECPP_WITH_GUI | SindreCpp::Gui | gui/index.hpp / gui | ImGui context, optional GLFW/OpenGL3, fonts and images |
| SINDRECPP_WITH_UTILS2D | SindreCpp::Utils2d | utils2d/index.hpp / utils2d | OpenCV images and preprocessing |
| SINDRECPP_WITH_UTILS3D | SindreCpp::Utils3d | utils3d/index.hpp / utils3d | VTK core, SindreMesh, Eigen and geometry backends |
| SINDRECPP_WITH_AI | SindreCpp::Ai | ai/index.hpp / ai | Tensor types, execution and selected backends |

Runnable, purpose-oriented examples live under [examples/](examples/README.md).
Each example has its own `CMakeLists.txt` and `main.cpp`, and can be configured
either from the root project or directly from its own directory.

Only dependency-free general defaults ON and can be disabled.
All other domains/integrations default OFF. AI defaults to the ONNX Runtime backend with
CPU compilation and execution; CUDA support and native TensorRT are opt-in. The backend
switches only apply when AI is enabled.
Third-party sources, fixed versions and SDK/package requirements are recorded under
[`thirds/`](thirds/README.md) by module; downloaded source trees stay in the build directory.
TensorRT does not link ONNX Runtime. Set `SINDRECPP_AI_ONNXRUNTIME=OFF` for a TRT-only build.
General integrations use SINDRECPP_WITH_POINTER, STRING, LOG, HTTP, JSON, CLI, RE2, ZLIB,
CRASHPAD and UTILS_PY (all OFF). Set SINDRECPP_NO_EXCEPTIONS=ON to build the core without compiler exception
support; the Result-based APIs remain the supported public surface.
Clang/clang-cl is the primary compiler profile. Common targets enable `-Wall -Wextra -Wpedantic`
on Clang or `/W4 /permissive- /Zc:__cplusplus /utf-8` on clang-cl/MSVC. Use
`SINDRECPP_WARNINGS_AS_ERRORS=ON` for CI and `SINDRECPP_MSVC_STATIC_RUNTIME=ON` only when the
application uses the same CRT policy.
Core/Gui/Python old targets, namespaces and headers have been removed without aliases.
Base strings and pointers work without CsString/CsPointer.
`general::string::split` returns owning `std::string` tokens; use `split_view` only when
the source string lifetime is guaranteed. UTF-8 text is kept as bytes, while filesystem
overloads in the file/image/mesh APIs perform platform-appropriate path conversion.
The string/JSON/HTTP convenience APIs accept UTF-8 encoded `char` strings; in C++20,
`u8"..."` is `char8_t` and must be converted explicitly (or use an ordinary UTF-8
source literal).

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
#include <ai/index.hpp>

sindrecpp::ai::onnxruntime::Model model("model.onnx"); // portable CPU default
sindrecpp::ai::Tensors input{ /* dense float32 tensors in model input order */ };
auto output = model.infer(input);
auto future = model.infer_async(input);
auto async_output = future.get(); // Errors are rethrown here.
```

For the common non-throwing boundary, use ONNX Runtime's `Model::try_create`,
`try_infer`, `try_infer_typed`, `try_infer_async`, and related `try_*` methods;
they return General `Result` values with an error context.

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
set(SINDRECPP_TENSORRT_ROOT "/path/to/TensorRT-10.x" CACHE PATH "")
# After FetchContent_MakeAvailable:
target_link_libraries(my_app PRIVATE SindreCpp::Ai)
```

```cpp
#include <ai/trt.hpp>

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

Result/Error/version now live under sindrecpp::general. `Error` carries code, message and context;
`Result<T>` is the common non-throwing return carrier. `trim` returns a view into the source;
use `trim_copy` when an owning result is needed. String helpers also provide UTF-8 validation,
ASCII fast casing, `parse_int`, `parse_float`, `join` and `concat`; parsing failures return `Result`.
`split` returns owning `std::string` tokens, while `split_view` is the zero-copy view variant.
JSON parsing uses `json::parse()`/`json::try_parse()` and returns `Result<Document>`; parse failures
do not cross the SindreCpp API as exceptions.
With HTTP enabled, `http::get(host, port, path, RequestOptions)` adds unified timeout/retry
handling and returns `Result<ResponseData>`; use `json::try_parse(response.body)` for JSON bodies.
With JSON enabled, `config::Config` loads JSON files/strings into dotted keys, supports typed reads,
defaults and environment overrides. `codec.hpp` provides UUID v4, FNV-1a and Base64 helpers.
`path.hpp` centralizes UTF-8/filesystem conversion, `file_watch.hpp` provides stoppable file watchers,
and `codec::simple_compress/simple_decompress` provide a small dependency-free RLE format. With
SINDRECPP_WITH_ZLIB, zlib compression/decompression is also available.
The general layer also includes `scope_guard`, `ranges`, `dynamic_library::Library`, `temp::File`,
`system`, `process`, `url`, `versioning`, `diagnostics`, `startup`, and `desktop`; use the
purpose-oriented aggregates documented in `modules/general/docs/README.md`. Optional RE2
and Crashpad boundaries are guarded by `SINDRECPP_WITH_RE2` and `SINDRECPP_WITH_CRASHPAD`; a
disabled platform backend returns `function_not_supported`. `transfer::download/upload` uses the
HTTP wrapper, cancellation-aware retries and progress callbacks.
Optional log exposes warning() and rotating_file(name, path), 10 MiB/file and five retained files.
`gui` 默认提供 GLFW + OpenGL3 后端：`GuiApplication::create()` 负责窗口、显示器选择、内容缩放、ImGui 初始化和默认暗色主题。
字体会优先扫描系统字体目录，并支持通过 `FontConfig::path` 指定字体；启用 CJK 加载时会使用完整中文 glyph range。
`ImageAsset`/`ImageCache` 统一处理 UTF-8/中文路径的图片读取，`TextureUploader` 将 CPU 解码结果交给宿主的 GPU 纹理实现。
`ScopedId`、`ScopedDisabled`、`tooltip`、`help_marker`、`icon_button` 和 `input_text` 提供常用 ImGui 封装。

需要自行管理窗口或渲染后端时，将 `SINDRECPP_GUI_GLFW_OPENGL3=OFF`，此时模块仅提供 ImGui 核心上下文和通用封装。
GUI 窗口测试默认关闭；可用 `SINDRECPP_BUILD_GUI_RUNTIME_TESTS=ON` 启用真实窗口 smoke test。
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

Use the platform presets so build directories do not multiply:

```bash
cmake --preset windows-clang   # build_win/bin
cmake --build build_win
ctest --test-dir build_win --output-on-failure

cmake --preset linux-clang     # build_linux/bin
cmake --build build_linux
ctest --test-dir build_linux --output-on-failure
```

All SindreCpp executables, libraries, and Windows runtime DLLs are placed in
the selected build directory's `bin/`. Windows dependency DLLs discovered by
the enabled modules are copied beside the generated executable so the loader
does not fall back to an unrelated system installation.

Manual configuration should still use the platform directory, for example
`cmake -S . -B build_win -DCMAKE_BUILD_TYPE=Release`.

For inference fixtures: pip install onnx==1.17.0; python tests/create_test_model.py.
CPU builds require ORT 1.22+ matching headers/runtime; GPU ORT also needs matching CUDA/cuDNN.
Native TRT targets TensorRT 10.11+ and CUDA Toolkit 12+; TensorRT 11+ is not yet supported.
The ONNX Runtime convenience API defaults to CPU. Set `SINDRECPP_AI_CUDA=ON` and select
`Options.backend=Backend::cuda` only when using a matching GPU ORT package and CUDA runtime.

CI runs Windows/Linux integrations, general-only/all-disabled builds, OpenCV/ORT CPU execution,
async/queue/pipeline tests and native TRT API compilation. GPU conversion/inference tests are opt-in
with SINDRECPP_BUILD_GPU_TESTS=ON and require real GPU hardware.

[Inference guide](docs/inference.md) · [Development](docs/development.md) · [Notes](docs/notes.md).
MIT; dependencies keep their licenses.
