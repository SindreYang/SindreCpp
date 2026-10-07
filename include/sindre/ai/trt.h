#pragma once

/// @file
/// @brief TensorRT engine 构建、加载和推理接口。

#if !defined(SINDRE_AI_TRT)
#error "Enable SINDRE_WITH_AI and SINDRE_AI_TRT; link sindre::ai."
#endif

#include <sindre/ai/execution.h>
#include <sindre/ai/types.h>

#include <cstddef>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace sindre::ai::trt {

/// @brief TensorRT engine 的兼容性策略。
enum class Compatibility { build_gpu, same_compute_capability, ampere_plus };

/// @brief 动态输入的最小、优化和最大形状。
struct ShapeProfile {
    std::string name;
    std::vector<std::int64_t> min, opt, max;
};

/// @brief TensorRT engine 构建参数。
struct BuildOptions {
    int device_id = 0;
    std::size_t workspace_bytes = std::size_t{1} << 30;
    int optimization_level = 3;
    int max_aux_streams = 0;
    bool fp16 = false;
    bool tf32 = true;
    bool version_compatible = false;
    /// @brief 不把 TensorRT lean runtime 嵌入 plan；需要加载外部 runtime。
    ///
    /// 仅在 version_compatible=true 时有效，适合多个 engine 共享一份运行时。
    bool exclude_lean_runtime = false;
    Compatibility compatibility = Compatibility::build_gpu;
    std::vector<ShapeProfile> profiles;
    /// @brief 针对当前 GPU 的最高性能默认配置。
    static BuildOptions max_performance();
    /// @brief 允许在 Ampere 及更新架构间迁移的兼容配置。
    static BuildOptions cross_gpu();
};

/// @brief TensorRT engine 加载参数。
struct LoadOptions {
    int device_id = 0;
    int profile_index = 0;
    std::size_t queue_capacity = 16;
    bool allow_engine_host_code = false;
    /// @brief 为排除内置 lean runtime 的 version-compatible plan 指定外部 runtime。
    std::filesystem::path lean_runtime_path;
};

/// @brief 将 ONNX 模型构建为 TensorRT engine 文件。
void convert_onnx(const std::filesystem::path& source,
                 const std::filesystem::path& destination,
                 const BuildOptions& options = {});

/// @brief 将 ONNX 模型构建为 TensorRT engine，并把后端失败转换为 Result。
::sindre::general::Result<void> try_convert_onnx(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const BuildOptions& options = {}) noexcept;

/// @brief 表示设备异步推理尚未完成的结果。
class Pending {
public:
    Pending(const Pending&) = delete;
    Pending& operator=(const Pending&) = delete;
    Pending(Pending&&) noexcept;
    Pending& operator=(Pending&&) noexcept;
    ~Pending();

    /// @brief 查询 CUDA 工作是否完成。
    bool ready() const;
    /// @brief 查询 CUDA 工作是否完成，并把 CUDA/句柄错误转换为 Result。
    ::sindre::general::Result<bool> try_ready() const noexcept;
    /// @brief 等待设备工作完成。
    void wait();
    /// @brief 等待设备工作完成，并把 CUDA/句柄错误转换为 Result。
    ::sindre::general::Result<void> try_wait() noexcept;
    /// @brief 读取异步推理生成的主机张量。
    Tensors get();
    /// @brief 读取异步推理生成的主机张量，并把错误转换为 Result。
    ::sindre::general::Result<Tensors> try_get() noexcept;
    /// @brief 读取异步推理生成的类型化主机张量。
    TypedTensors get_typed();
    /// @brief 读取异步推理生成的类型化主机张量，并把错误转换为 Result。
    ::sindre::general::Result<TypedTensors> try_get_typed() noexcept;

private:
    friend class Model;
    explicit Pending(std::shared_ptr<void> state, bool host_outputs = true);
    void release() noexcept;
    std::shared_ptr<void> state_;
    bool host_outputs_ = true;
};

/// @brief 调用方持有的设备张量视图；data 不由该类型释放。
struct DeviceTensorView {
    std::vector<std::int64_t> shape;
    void* data = nullptr;
    std::size_t elements = 0;
    DataType type = DataType::float32;
};

/// @brief TensorRT engine 的稳定公共句柄。
///
/// TensorRT、CUDA stream 和 event 类型都留在 cpp 实现中，公共 API 使用不透明指针。
class Model {
public:
    explicit Model(const std::filesystem::path& path, const LoadOptions& options = {});
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;

    /// @brief 返回 engine 输入描述。
    const std::vector<TensorInfo>& get_inputs() const noexcept;
    /// @brief 返回 engine 输出描述。
    const std::vector<TensorInfo>& get_outputs() const noexcept;
    /// @brief 创建 TensorRT 模型，并把加载错误转换为 Result。
    static ::sindre::general::Result<std::shared_ptr<Model>> try_create(
        const std::filesystem::path& path, const LoadOptions& options = {}) noexcept;
    /// @brief 执行同步主机张量推理。
    Tensors infer(const Tensors& inputs);
    /// @brief 执行同步主机张量推理，并把错误转换为 Result。
    ::sindre::general::Result<Tensors> try_infer(const Tensors& inputs) noexcept;
    /// @brief 执行支持多种元素精度的同步主机张量推理。
    TypedTensors infer_typed(const TypedTensors& inputs);
    /// @brief 执行类型化主机张量推理，并把错误转换为 Result。
    ::sindre::general::Result<TypedTensors> try_infer_typed(
        const TypedTensors& inputs) noexcept;
    /// @brief 提交异步主机张量推理。
    std::future<Tensors> infer_async(Tensors inputs);
    /// @brief 提交可取消的异步主机张量推理。
    ::sindre::general::Result<std::future<::sindre::general::Result<Tensors>>>
    try_infer_async(Tensors inputs,
                    ::sindre::general::CancellationToken token = {}) noexcept;
    /// @brief 提交带取消、截止时间和进度选项的异步推理任务。
    ::sindre::general::Result<std::future<::sindre::general::Result<Tensors>>>
    try_infer_async(Tensors inputs,
                    ::sindre::general::TaskOptions options) noexcept;
    /// @brief 提交支持多种元素精度的异步主机张量推理。
    std::future<TypedTensors> infer_typed_async(TypedTensors inputs);
    /// @brief 提交可取消的异步类型化主机张量推理。
    ::sindre::general::Result<std::future<::sindre::general::Result<TypedTensors>>>
    try_infer_typed_async(TypedTensors inputs,
                          ::sindre::general::CancellationToken token = {}) noexcept;
    /// @brief 提交带取消、截止时间和进度选项的异步类型化推理任务。
    ::sindre::general::Result<std::future<::sindre::general::Result<TypedTensors>>>
    try_infer_typed_async(TypedTensors inputs,
                          ::sindre::general::TaskOptions options) noexcept;
    /// @brief 预热 engine。
    void warm_up(const Tensors& inputs, int iterations = 1);
    /// @brief 预热 engine，并把错误转换为 Result。
    ::sindre::general::Result<void> try_warm_up(
        const Tensors& inputs, int iterations = 1) noexcept;
    void close() noexcept;
    /// @brief 提交主机张量到设备执行。
    Pending enqueue(const Tensors& inputs);
    /// @brief 提交主机张量到设备执行，并把错误转换为 Result。
    ::sindre::general::Result<Pending> try_enqueue(const Tensors& inputs) noexcept;
    /// @brief 提交类型化主机张量到设备执行，并把错误转换为 Result。
    ::sindre::general::Result<Pending> try_enqueue_typed(
        const TypedTensors& inputs) noexcept;
    /// @brief 提交已有设备缓冲区，ready_event 为可选 CUDA event 不透明指针。
    Pending enqueue_device(const std::vector<DeviceTensorView>& inputs,
                           const std::vector<DeviceTensorView>& outputs,
                           void* ready_event = nullptr);
    /// @brief 提交设备缓冲区，并把错误转换为 Result。
    ::sindre::general::Result<Pending> try_enqueue_device(
        const std::vector<DeviceTensorView>& inputs,
        const std::vector<DeviceTensorView>& outputs,
        void* ready_event = nullptr) noexcept;

private:
    class Impl;
    Pending enqueue_typed(const TypedTensors &inputs);
    std::unique_ptr<Impl> impl_;
};

} // namespace sindre::ai::trt
