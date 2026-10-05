#pragma once

#if !defined(SINDRECPP_AI_ONNXRUNTIME)
#error "Enable SINDRECPP_WITH_AI and link SindreCpp::OnnxRuntime."
#endif

#include <sindrecpp/ai/types.hpp>
#include <sindrecpp/ai/execution.hpp>
#include <onnxruntime_cxx_api.h>
static_assert(ORT_API_VERSION >= 22, "ONNX Runtime 1.22+ headers are required");
#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <vector>

namespace sindrecpp::ai::onnxruntime {

namespace native = Ort;
enum class Backend { cpu, cuda };

struct Options {
    Backend backend = Backend::cuda;
    int device_id = 0;
    int cpu_threads = 0; // 0: ONNX Runtime chooses.
    std::size_t queue_capacity = 16;
};

inline DataType get_type(ONNXTensorElementDataType type) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return DataType::float32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return DataType::float16;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return DataType::int32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return DataType::int64;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return DataType::bool8;
        default: return DataType::other;
    }
}

inline std::vector<std::string> get_available_backends() {
    return Ort::GetAvailableProviders();
}

class Model {
public:
    explicit Model(const std::filesystem::path& path, const Options& options = {})
        : backend_(options.backend), executor_(options.queue_capacity) {
        if (options.device_id < 0 || options.cpu_threads < 0 ||
            (options.backend != Backend::cpu && options.backend != Backend::cuda))
            throw std::invalid_argument("Invalid inference options");
        Ort::SessionOptions session_options;
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (options.cpu_threads > 0) session_options.SetIntraOpNumThreads(options.cpu_threads);
        if (options.backend != Backend::cpu) {
#if defined(SINDRECPP_AI_CUDA)
            require_provider("CUDAExecutionProvider");
            OrtCUDAProviderOptionsV2* raw = nullptr;
            Ort::ThrowOnError(Ort::GetApi().CreateCUDAProviderOptions(&raw));
            std::unique_ptr<OrtCUDAProviderOptionsV2, decltype(Ort::GetApi().ReleaseCUDAProviderOptions)>
                provider(raw, Ort::GetApi().ReleaseCUDAProviderOptions);
            const auto device = std::to_string(options.device_id);
            const char* keys[] = {"device_id", "cudnn_conv_use_max_workspace"};
            const char* values[] = {device.c_str(), "1"};
            Ort::ThrowOnError(Ort::GetApi().UpdateCUDAProviderOptions(provider.get(), keys, values, 2));
            session_options.AppendExecutionProvider_CUDA_V2(*provider);
#else
            throw std::runtime_error("CUDA backend was not enabled; choose Backend::cpu explicitly");
#endif
        }
        session_ = std::make_unique<Ort::Session>(get_environment(), path.c_str(), session_options);
        inputs_ = get_info(true);
        outputs_ = get_info(false);
        for (const auto& input : inputs_) input_names_.push_back(input.name.c_str());
        for (const auto& output : outputs_) output_names_.push_back(output.name.c_str());
    }

    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    Model(Model&&) = delete; // Cached name pointers and native resources stay stable.
    Model& operator=(Model&&) = delete;

    const std::vector<TensorInfo>& get_inputs() const noexcept { return inputs_; }
    const std::vector<TensorInfo>& get_outputs() const noexcept { return outputs_; }
    Backend get_backend() const noexcept { return backend_; }

    // Inputs are ordered as get_inputs(); outputs are ordered as get_outputs().
    // Calls on one Model are serialized. Use one Model per worker for parallel GPU streams.
    std::vector<Tensor> infer(const std::vector<Tensor>& inputs) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto values = get_values(inputs, inputs_);
        auto result = session_->Run(Ort::RunOptions{nullptr}, input_names_.data(), values.data(),
                                    values.size(), output_names_.data(), output_names_.size());
        std::vector<Tensor> outputs;
        outputs.reserve(result.size());
        for (auto& value : result) {
            if (!value.IsTensor()) throw std::runtime_error("Only dense tensor outputs are supported");
            auto info = value.GetTensorTypeAndShapeInfo();
            if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
                throw std::runtime_error("infer supports float32 outputs only; use native session for other types");
            const auto count = info.GetElementCount();
            const auto* data = value.GetTensorData<float>();
            Tensor output{info.GetShape(), {}};
            if (count) output.data.assign(data, data + count);
            outputs.push_back(std::move(output));
        }
        return outputs;
    }

    // Reuse caller-owned output buffers; shapes must already be resolved.
    void infer_into(const std::vector<Tensor>& inputs, std::vector<Tensor>& outputs) {
        std::lock_guard<std::mutex> lock(mutex_);
        auto input_values = get_values(inputs, inputs_);
        auto output_values = get_values(outputs, outputs_);
        session_->Run(Ort::RunOptions{nullptr}, input_names_.data(), input_values.data(),
                      input_values.size(), output_names_.data(), output_values.data(), output_values.size());
    }

    std::future<Tensors> infer_async(Tensors inputs) {
        return executor_.submit([this, inputs = std::move(inputs)] { return infer(inputs); });
    }
    void close() noexcept { executor_.close(); }

    void warm_up(const std::vector<Tensor>& inputs, int iterations = 1) {
        if (iterations < 1) throw std::invalid_argument("Warm-up iterations must be positive");
        for (int i = 0; i < iterations; ++i) (void)infer(inputs);
    }

    // Advanced: Ort::IoBinding for GPU-resident buffers. Caller handles synchronization,
    // tensor lifetimes and mutual exclusion with infer/infer_into.
    Ort::Session& get_native_session() noexcept { return *session_; }

private:
    static Ort::Env& get_environment() {
        static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "SindreCpp");
        return environment; // Initialized on first Model construction.
    }
    static void require_provider(const char* name) {
        const auto providers = get_available_backends();
        if (std::find(providers.begin(), providers.end(), name) == providers.end())
            throw std::runtime_error(std::string("Required execution provider is unavailable: ") + name);
    }
    std::vector<TensorInfo> get_info(bool input) const {
        Ort::AllocatorWithDefaultOptions allocator;
        const auto count = input ? session_->GetInputCount() : session_->GetOutputCount();
        std::vector<TensorInfo> info;
        for (std::size_t i = 0; i < count; ++i) {
            auto name = input ? session_->GetInputNameAllocated(i, allocator)
                              : session_->GetOutputNameAllocated(i, allocator);
            auto type = input ? session_->GetInputTypeInfo(i) : session_->GetOutputTypeInfo(i);
            auto tensor = type.GetTensorTypeAndShapeInfo();
            info.push_back({name.get(), tensor.GetShape(), get_type(tensor.GetElementType())});
        }
        return info;
    }
    static std::vector<Ort::Value> get_values(const std::vector<Tensor>& tensors,
                                             const std::vector<TensorInfo>& metadata) {
        if (tensors.size() != metadata.size()) throw std::invalid_argument("Tensor count does not match model");
        auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
        std::vector<Ort::Value> values;
        values.reserve(tensors.size());
        for (std::size_t i = 0; i < tensors.size(); ++i) {
            const auto& tensor = tensors[i];
            tensor.validate();
            if (metadata[i].type != DataType::float32)
                throw std::invalid_argument("Convenience API supports float32 tensors only");
            if (tensor.shape.size() != metadata[i].shape.size())
                throw std::invalid_argument("Tensor rank does not match model");
            for (std::size_t d = 0; d < tensor.shape.size(); ++d)
                if (metadata[i].shape[d] >= 0 && tensor.shape[d] != metadata[i].shape[d])
                    throw std::invalid_argument("Tensor dimension does not match model");
            values.push_back(Ort::Value::CreateTensor<float>(memory,
                const_cast<float*>(tensor.data.data()), tensor.data.size(), tensor.shape.data(), tensor.shape.size()));
        }
        return values;
    }

    Backend backend_;
    std::unique_ptr<Ort::Session> session_;
    std::vector<TensorInfo> inputs_, outputs_;
    std::vector<const char*> input_names_, output_names_;
    std::mutex mutex_;
    detail::Executor executor_;
};

} // namespace sindrecpp::ai::onnxruntime
