#pragma once

#if !defined(SINDRE_AI_ONNXRUNTIME)
#error "Enable SINDRE_WITH_AI and SINDRE_AI_ONNXRUNTIME; link sindre::ai."
#endif

#include <sindre/ai/execution.h>
#include <sindre/ai/types.h>

#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <vector>

namespace sindre::ai::onnxruntime {

/// @brief ONNX Runtime 执行后端。
enum class Backend { cpu, cuda };

/// @brief ONNX Runtime 模型创建参数。
struct Options {
    Backend backend = Backend::cpu;
    int device_id = 0;
    int cpu_threads = 0;
    std::size_t queue_capacity = 16;
};

/// @brief 返回当前构建可用的执行后端名称。
std::vector<std::string> get_available_backends();

/// @brief 线程安全边界内的 ONNX Runtime 模型句柄。
///
/// SDK 类型和异常均隐藏在实现文件中；调用方只依赖 sindrecpp 的张量和错误类型。
class Model {
public:
    /// @brief 从 ONNX 文件创建模型；失败时抛出标准异常。
    explicit Model(const std::filesystem::path& path, const Options& options = {});
    ~Model();

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete;
    Model& operator=(Model&&) = delete;

    /// @brief 返回模型输入描述。
    const std::vector<TensorInfo>& get_inputs() const noexcept;
    /// @brief 返回模型输出描述。
    const std::vector<TensorInfo>& get_outputs() const noexcept;
    /// @brief 返回模型实际使用的后端。
    Backend get_backend() const noexcept;

    /// @brief 无异常创建模型，并将后端错误转换为 Result。
    static ::sindre::general::Result<std::shared_ptr<Model>> try_create(
        const std::filesystem::path& path, const Options& options = {}) noexcept;
    /// @brief 执行 float32 推理。
    ::sindre::general::Result<Tensors> try_infer(const Tensors& inputs) noexcept;
    /// @brief 执行指定数据类型的推理。
    ::sindre::general::Result<TypedTensors> try_infer_typed(
        const TypedTensors& inputs) noexcept;
    /// @brief 将推理结果写入调用方提供的 float32 容器。
    ::sindre::general::Result<void> try_infer_into(
        const Tensors& inputs, Tensors& outputs) noexcept;
    /// @brief 将推理结果写入调用方提供的类型化容器。
    ::sindre::general::Result<void> try_infer_typed_into(
        const TypedTensors& inputs, TypedTensors& outputs) noexcept;
    /// @brief 预热模型，减少首次推理的初始化抖动。
    ::sindre::general::Result<void> try_warm_up(
        const Tensors& inputs, int iterations = 1) noexcept;
    /// @brief 提交可取消的异步推理任务。
    ::sindre::general::Result<std::future<::sindre::general::Result<Tensors>>>
    try_infer_async(Tensors inputs, ::sindre::general::CancellationToken token = {}) noexcept;

    /// @brief 抛异常执行 float32 推理。
    Tensors infer(const Tensors& inputs);
    /// @brief 抛异常执行类型化推理。
    TypedTensors infer_typed(const TypedTensors& inputs);
    void infer_into(const Tensors& inputs, Tensors& outputs);
    void infer_typed_into(const TypedTensors& inputs, TypedTensors& outputs);
    std::future<Tensors> infer_async(Tensors inputs);
    void warm_up(const Tensors& inputs, int iterations = 1);
    void close() noexcept;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace sindre::ai::onnxruntime
