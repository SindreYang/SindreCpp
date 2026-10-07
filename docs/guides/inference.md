# 图像与推理指南

本文面向已经启用 AI 模块、需要选择 ONNX Runtime 或 TensorRT 后端的应用开发者，
重点说明同步/异步推理、流水线、取消和资源所有权；基础开关见 [AI 模块说明](../modules/ai.md)。

## 两个独立后端

- ai::onnxruntime：ONNX Runtime CPU/CUDA，读取 ONNX。
- ai::trt：原生 TensorRT，ONNX 转 engine，读取 engine 并直接 enqueueV3。
- 两者共用 ai::Tensor、ai::Tensors 和 Pipeline，不共享后端运行库依赖。
- 不再通过 ORT TensorRT 执行提供器实现 TRT。

仅 general 默认开启。AI 默认关闭；启用 AI 后默认 ONNX Runtime，默认编译和运行 CPU。
TRT 独立开启：SINDRE_WITH_AI=ON、SINDRE_AI_TRT=ON。
只用 TRT 时关闭 SINDRE_AI_ONNXRUNTIME；此时不查找 ORT。
只用基础执行/流水线时两个后端都可关闭。

## 三种执行模式

两个 Model 均提供：
- infer(inputs)：同步，返回输出。
- infer_async(inputs)：返回 std::future，数据归任务所有，失败在 get 时重抛。
- warm_up(inputs, iterations)：显式预热。

需要统一非异常边界时，ONNX Runtime `Model` 提供 `try_create`、`try_infer`、
`try_infer_typed`、`try_infer_async` 等 `try_*` 接口，返回 General `Result`，错误包含
`ai.onnxruntime.*` context。

ORT 另外提供 infer_into，重用已知形状的主机输出缓冲。
同一 Model 的同步/异步基础接口串行，有界 FIFO 默认等待队列容量 16。
满队列立即报错，不丢任务、不隐式覆盖旧输入。队列容量可配置。
close 停止异步提交并排空已接收任务；同步 infer 仍可调用。
析构等待已接收工作完成，所以退出不会释放仍在使用的资源。

流水线有独立的预处理、推理线程，真正重叠两个阶段，不是简单逐项调用：

```cpp
#include <sindre/ai.h>
#include <sindre/utils_2d.h>

using namespace sindre;
auto model = std::make_shared<ai::onnxruntime::Model>("model.onnx");
ai::Pipeline<utils_2d::Image, ai::Tensors, ai::Tensors> pipeline(
    [](utils_2d::Image image) {
        auto boxed = utils_2d::letterbox(image, {640, 640});
        auto tensor = utils_2d::to_tensor(boxed.image);
        return ai::Tensors{ai::Tensor{std::move(tensor.shape), std::move(tensor.data)}};
    },
    [model](ai::Tensors input) { return model->infer(input); },
    8); // 最多8项已接收且未完成的工作
auto first = pipeline.infer_async(utils_2d::load("first.jpg"));
auto second = pipeline.infer_async(utils_2d::load("second.jpg"));
auto output = first.get();
pipeline.close(); // 排空已接收任务，second.get() 仍有效
```

Pipeline::infer 是 submit 后等待的同步形式。
每个 future 对应自己的输入；单流水线推理阶段 FIFO。
调用者必须让外部引用活到任务完成。cv::Mat 是共享数据，不是深拷贝：
提交后不要修改对应图像；要独立快照请显式 clone。
回调捕获 shared_ptr<Model>，避免析构后使用。
不要从自身工作回调析构/close 执行器或流水线，也不要递归等待同一单线程队列任务，
这些会死锁或触发程序终止。退出在提交线程调用 close。
多个 Model/流水线可并发，但注意显存/线程数，不自动创建无限线程。

## TRT转换：参数由用户决定

```cpp
namespace trt = sindre::ai::trt;
auto build = trt::BuildOptions::max_performance();
build.fp16 = true;
build.tf32 = true;
build.workspace_bytes = std::size_t{4} << 30;
build.optimization_level = 5;
build.max_aux_streams = 0;
build.compatibility = trt::Compatibility::build_gpu;
build.profiles = {{"images", {1,3,640,640}, {4,3,640,640}, {8,3,640,640}}};
trt::convert_onnx("model.onnx", "model.engine", build);
```

| 参数 | 意义 |
| --- | --- |
| fp16 / tf32 | 内部低精度策略；公共便利接口仍是 float32 I/O |
| workspace_bytes | builder workspace 上限，不是总显存限制 |
| optimization_level | 0..5；较高等级增加构建成本，不保证每个模型更快 |
| max_aux_streams | TensorRT 辅助 stream 上限 |
| profiles | 单个优化 profile，各动态输入的 min/opt/max 范围 |
| compatibility | build_gpu / same_compute_capability / ampere_plus |
| version_compatible | TensorRT 版本兼容策略，与跨 GPU 不同 |
| configure | 高级 builder 回调，可配置 tactic/timing cache 等；相关资源生命周期由用户负责 |

max_performance 使用等级5、不加硬件兼容约束，默认保留用户精度决策。
cross_gpu 使用 ampere_plus，覆盖 NVIDIA Ampere 及更新架构的适用范围，
不承诺任意显卡、任意操作系统或任意 CUDA/TRT 版本通用。
same_compute_capability 仅同计算能力范围。兼容约束可能排除快 tactic，降低性能。
FP16/TF32 必须检查业务误差，INT8 校准/量化不在首版便利参数中。
缺少动态输入 profile 会报错，不猜测业务尺寸。
只写新 engine 路径，已有文件会拒绝；覆盖前由调用者明确处理旧文件。
外部权重文件必须与 ONNX 相对位置正确，转换模型及插件只来自可信来源。
configure 回调能够覆盖前述选项；不保证自定义配置仍满足便利接口限制。

## 原生 TRT 异步与设备接口

```cpp
trt::Model model("model.engine");
auto future = model.infer_async(inputs); // 有界后台队列
auto result = future.get();

auto pending = model.enqueue(inputs); // 直接提交CUDA工作，不等待完成
bool done = pending.ready();
auto outputs = pending.get(); // 等待event并取得主机输出
```

enqueue 立即进行主机输入拷贝到自有 pinned memory，然后提交异步 H2D/infer/D2H。
Pinned/device 缓冲按容量缓存，不在固定尺寸每次调用时重新分配。
一个 Model 同时最多一个未消费 Pending，busy 时拒绝新的直接 enqueue；
Model::infer/infer_async 基础接口通过锁串行，不能与未完成的直接 ticket 混用。

设备输入/输出不经主机：
```cpp
std::vector<trt::DeviceTensorView> device_inputs{{input_shape, gpu_input, input_elements}};
std::vector<trt::DeviceTensorView> device_outputs{{output_shape, gpu_output, output_elements}};
auto completion = model.enqueue_device(device_inputs, device_outputs, producer_event);
completion.wait();
```

gpu_input/gpu_output 在 Model 所选 GPU 上，形状/长度准确；缓冲至少活到完成。
可用 producer_event 等待另一条 stream 的输入生产，调用者管理 event 生命周期。
设备 ticket 使用 ready/wait，get 会拒绝，不复制到 CPU。
Pending 可移动到其他线程，持有共享引擎资源；析构会等待 stream 结束，
不是随手销毁就取消任务。引擎不可在工作未完成时被释放。

首版 native TRT 便利接口限制：
- 线性、device、dense float32 I/O；
- 不接受 shape tensor 输入；
- 输出尺寸必须能从输入形状推导，不支持数据依赖输出分配；
- 标量支持，零元素不支持；INT8/FP16 I/O 等需自定义原生实现；
- 一个构建 profile，加载可选 engine 中指定 profile。
- TRT 10.11+ API；TensorRT 11+ 尚未支持，不假设已删除 API 仍可用。

## 安装与部署

ORT：1.22+ C/C++ SDK，同版头文件/动态库，GPU用官方GPU包。
CPU 可直接使用默认 CMake 选项和 Options。启用 `SINDRE_AI_CUDA=ON` 后再选择
`Backend::cuda`；CUDA/Provider 缺失会明确失败，
不会静默降级为 CPU。

TRT：安装 TensorRT 10.11+ 的 nvinfer/nvinfer_plugin/nvonnxparser 和 CUDA Toolkit 12+。
设置 SINDRE_TENSORRT_ROOT、CUDAToolkit_ROOT，或父项目提供 TensorRT::nvinfer、
TensorRT::nvinfer_plugin、TensorRT::nvonnxparser targets。
引擎与 GPU/TRT/平台兼容性必须验证。version_compatible 可能包含运行时代码，
只有可信 engine 才显式 LoadOptions.allow_engine_host_code=true。
默认拒绝 engine 嵌入的 host code；不要对外部不可信 plan 放开此开关。

Windows配置SDK的DLL目录到PATH；Linux配置动态库路径/RPATH。
库不会自动安装显卡驱动、大型SDK或复制DLL。
官方文档：
- https://onnxruntime.ai/docs/execution-providers/CUDA-ExecutionProvider.html
- https://docs.nvidia.com/deeplearning/tensorrt/10.x.x/inference-library/version-compatibility.html
- https://docs.nvidia.com/deeplearning/tensorrt/10.x.x/inference-library/work-dynamic-shapes.html

## 图像预处理

load默认BGR；save(value,destination)。resize改变宽高，letterbox保留比例并补边。
crop严格检查并返回独立副本；normalize=pixel*scale+offset。
to_tensor=(pixel*scale-mean)/std，默认RGB/NCHW/float32/1/255，参数按输出通道顺序。
支持非连续ROI，不修改原图，不自动resize。
预测坐标逆变换先减left/top再除scale，整数缩放有亚像素舍入误差。
不自动附加某个模型的NMS，前后处理由业务确认。

## 验证

CPU/队列/流水线、Windows/Linux、OpenCV测试在CI执行。
TRT和CUDA在无GPU runner仅检查官方API编译，不能证明GPU执行或跨卡兼容。
真实GPU测试：
```bash
uv run --with onnx==1.17.0 python tests/create_test_model.py
# Generates the float32, int32, and float16 fixtures used by the AI tests.
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DSINDRE_WITH_AI=ON -DSINDRE_AI_ONNXRUNTIME=OFF -DSINDRE_AI_TRT=ON \
  -DSINDRE_BUILD_GPU_TESTS=ON -DSINDRE_TENSORRT_ROOT=/path/to/TensorRT
cmake --build build
ctest --test-dir build --output-on-failure
```

测试保留唯一命名的临时identity engine供诊断。
发布前在实际GPU比较同步/异步输出、动态profile、busy失败恢复、跨卡加载、
FP16误差、连续推理和设备缓冲生命周期。
性能测试固定模型/尺寸/批量/精度/设备，预热并同步GPU，分别报告端到端和纯推理时间。
