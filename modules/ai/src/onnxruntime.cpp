#include <sindre/ai/onnxruntime.h>

#include <onnxruntime_cxx_api.h>
static_assert(ORT_API_VERSION >= 22, "ONNX Runtime 1.22+ headers are required");

#include <algorithm>
#include <cstring>
#include <limits>
#include <mutex>
#include <stdexcept>

namespace sindre::ai::onnxruntime {
namespace {

DataType get_type(ONNXTensorElementDataType type) {
    switch (type) {
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT: return DataType::float32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16: return DataType::float16;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32: return DataType::int32;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64: return DataType::int64;
        case ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL: return DataType::bool8;
        default: return DataType::other;
    }
}

ONNXTensorElementDataType to_ort_type(DataType type) {
    switch (type) {
        case DataType::float32: return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT;
        case DataType::float16: return ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT16;
        case DataType::int32: return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT32;
        case DataType::int64: return ONNX_TENSOR_ELEMENT_DATA_TYPE_INT64;
        case DataType::bool8: return ONNX_TENSOR_ELEMENT_DATA_TYPE_BOOL;
        default: throw std::invalid_argument("Unsupported typed tensor data type");
    }
}

Ort::Env& get_environment() {
    static Ort::Env environment(ORT_LOGGING_LEVEL_WARNING, "sindre");
    return environment;
}

#if defined(SINDRE_AI_CUDA)
void require_provider(const char* name) {
    const auto providers = Ort::GetAvailableProviders();
    if (std::find(providers.begin(), providers.end(), name) == providers.end())
        throw std::runtime_error(std::string("Required execution provider is unavailable: ") + name);
}
#endif

std::vector<TensorInfo> get_info(const Ort::Session& session, bool input) {
    Ort::AllocatorWithDefaultOptions allocator;
    const auto count = input ? session.GetInputCount() : session.GetOutputCount();
    std::vector<TensorInfo> info;
    for (std::size_t i = 0; i < count; ++i) {
        auto name = input ? session.GetInputNameAllocated(i, allocator)
                          : session.GetOutputNameAllocated(i, allocator);
        auto type = input ? session.GetInputTypeInfo(i) : session.GetOutputTypeInfo(i);
        auto tensor = type.GetTensorTypeAndShapeInfo();
        info.push_back({name.get(), tensor.GetShape(), get_type(tensor.GetElementType())});
    }
    return info;
}

std::vector<Ort::Value> get_values(const std::vector<Tensor>& tensors,
                                   const std::vector<TensorInfo>& metadata) {
    if (tensors.size() != metadata.size())
        throw std::invalid_argument("Tensor count does not match model");
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
        auto* data = const_cast<float*>(tensor.data.data());
        static float empty_tensor_storage = 0.0f;
        if (!data) data = &empty_tensor_storage;
        values.push_back(Ort::Value::CreateTensor<float>(memory, data, tensor.data.size(),
            tensor.shape.data(), tensor.shape.size()));
    }
    return values;
}

std::vector<Ort::Value> get_typed_values(const TypedTensors& tensors,
                                         const std::vector<TensorInfo>& metadata) {
    if (tensors.size() != metadata.size())
        throw std::invalid_argument("Tensor count does not match model");
    auto memory = Ort::MemoryInfo::CreateCpu(OrtArenaAllocator, OrtMemTypeDefault);
    std::vector<Ort::Value> values;
    values.reserve(tensors.size());
    static std::uint8_t empty_tensor_storage = 0;
    for (std::size_t i = 0; i < tensors.size(); ++i) {
        const auto& tensor = tensors[i];
        tensor.validate();
        if (tensor.type != metadata[i].type)
            throw std::invalid_argument("Tensor data type does not match model");
        if (tensor.shape.size() != metadata[i].shape.size())
            throw std::invalid_argument("Tensor rank does not match model");
        for (std::size_t d = 0; d < tensor.shape.size(); ++d)
            if (metadata[i].shape[d] >= 0 && tensor.shape[d] != metadata[i].shape[d])
                throw std::invalid_argument("Tensor dimension does not match model");
        auto* data = tensor.data.empty() ? &empty_tensor_storage : tensor.data.data();
        values.push_back(Ort::Value::CreateTensor(memory, const_cast<std::uint8_t*>(data),
            tensor.data.size(), tensor.shape.data(), tensor.shape.size(), to_ort_type(tensor.type)));
    }
    return values;
}

} // namespace

class Model::Impl {
public:
    Impl(const std::filesystem::path& path, const Options& options)
        : backend(options.backend), executor(options.queue_capacity) {
        if (options.device_id < 0 || options.cpu_threads < 0 ||
            (options.backend != Backend::cpu && options.backend != Backend::cuda))
            throw std::invalid_argument("Invalid inference options");
        Ort::SessionOptions session_options;
        session_options.SetGraphOptimizationLevel(GraphOptimizationLevel::ORT_ENABLE_ALL);
        if (options.cpu_threads > 0) session_options.SetIntraOpNumThreads(options.cpu_threads);
        if (options.backend != Backend::cpu) {
#if defined(SINDRE_AI_CUDA)
            require_provider("CUDAExecutionProvider");
            OrtCUDAProviderOptionsV2* raw = nullptr;
            Ort::ThrowOnError(Ort::GetApi().CreateCUDAProviderOptions(&raw));
            std::unique_ptr<OrtCUDAProviderOptionsV2,
                decltype(Ort::GetApi().ReleaseCUDAProviderOptions)>
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
        session = std::make_unique<Ort::Session>(get_environment(), path.c_str(), session_options);
        inputs = get_info(*session, true);
        outputs = get_info(*session, false);
        for (const auto& input : inputs) input_names.push_back(input.name.c_str());
        for (const auto& output : outputs) output_names.push_back(output.name.c_str());
    }

    Backend backend;
    std::unique_ptr<Ort::Session> session;
    std::vector<TensorInfo> inputs;
    std::vector<TensorInfo> outputs;
    std::vector<const char*> input_names;
    std::vector<const char*> output_names;
    std::mutex mutex;
    detail::Executor executor;
};

std::vector<std::string> get_available_backends() {
    return Ort::GetAvailableProviders();
}

Model::Model(const std::filesystem::path& path, const Options& options)
    : impl_(std::make_unique<Impl>(path, options)) {}

Model::~Model() = default;

const std::vector<TensorInfo>& Model::get_inputs() const noexcept { return impl_->inputs; }
const std::vector<TensorInfo>& Model::get_outputs() const noexcept { return impl_->outputs; }
Backend Model::get_backend() const noexcept { return impl_->backend; }

::sindre::general::Result<std::shared_ptr<Model>> Model::try_create(
    const std::filesystem::path& path, const Options& options) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return std::make_shared<Model>(path, options); }, "ai.onnxruntime.create");
}

::sindre::general::Result<Tensors> Model::try_infer(const Tensors& inputs) noexcept {
    return ::sindre::ai::detail::guarded([&] { return infer(inputs); }, "ai.onnxruntime.infer");
}

::sindre::general::Result<TypedTensors> Model::try_infer_typed(
    const TypedTensors& inputs) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return infer_typed(inputs); }, "ai.onnxruntime.infer_typed");
}

::sindre::general::Result<void> Model::try_infer_into(
    const Tensors& inputs, Tensors& outputs) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { infer_into(inputs, outputs); }, "ai.onnxruntime.infer_into");
}

::sindre::general::Result<void> Model::try_infer_typed_into(
    const TypedTensors& inputs, TypedTensors& outputs) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { infer_typed_into(inputs, outputs); }, "ai.onnxruntime.infer_typed_into");
}

::sindre::general::Result<void> Model::try_warm_up(
    const Tensors& inputs, int iterations) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { warm_up(inputs, iterations); }, "ai.onnxruntime.warm_up");
}

::sindre::general::Result<std::future<::sindre::general::Result<Tensors>>>
Model::try_infer_async(Tensors inputs, ::sindre::general::CancellationToken token) noexcept {
    return ::sindre::general::try_run_async(
        [this, inputs = std::move(inputs)](::sindre::general::CancellationToken task_token) mutable {
#if !defined(SINDRE_NO_EXCEPTIONS)
            if (task_token.cancelled()) throw std::runtime_error("AI inference cancelled");
#else
            (void)task_token;
#endif
            return infer(inputs);
        }, token);
}

Tensors Model::infer(const Tensors& inputs) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto values = get_values(inputs, impl_->inputs);
    auto result = impl_->session->Run(Ort::RunOptions{nullptr}, impl_->input_names.data(),
        values.data(), values.size(), impl_->output_names.data(), impl_->output_names.size());
    std::vector<Tensor> outputs;
    outputs.reserve(result.size());
    for (auto& value : result) {
        if (!value.IsTensor()) throw std::runtime_error("Only dense tensor outputs are supported");
        auto info = value.GetTensorTypeAndShapeInfo();
        if (info.GetElementType() != ONNX_TENSOR_ELEMENT_DATA_TYPE_FLOAT)
            throw std::runtime_error("infer supports float32 outputs only; use infer_typed for other types");
        const auto count = info.GetElementCount();
        const auto* data = value.GetTensorData<float>();
        Tensor output{info.GetShape(), {}};
        if (count) output.data.assign(data, data + count);
        outputs.push_back(std::move(output));
    }
    return outputs;
}

TypedTensors Model::infer_typed(const TypedTensors& inputs) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto values = get_typed_values(inputs, impl_->inputs);
    auto result = impl_->session->Run(Ort::RunOptions{nullptr}, impl_->input_names.data(),
        values.data(), values.size(), impl_->output_names.data(), impl_->output_names.size());
    TypedTensors outputs;
    outputs.reserve(result.size());
    for (auto& value : result) {
        if (!value.IsTensor()) throw std::runtime_error("Only dense tensor outputs are supported");
        auto info = value.GetTensorTypeAndShapeInfo();
        const auto type = get_type(info.GetElementType());
        const auto bytes_per_element = data_type_size(type);
        const auto count = info.GetElementCount();
        if (count > std::numeric_limits<std::size_t>::max() / bytes_per_element)
            throw std::overflow_error("Tensor output is too large");
        TypedTensor output{info.GetShape(), type,
                           std::vector<std::uint8_t>(count * bytes_per_element)};
        if (!output.data.empty())
            std::memcpy(output.data.data(), value.GetTensorRawData(), output.data.size());
        outputs.push_back(std::move(output));
    }
    return outputs;
}

void Model::infer_typed_into(const TypedTensors& inputs, TypedTensors& outputs) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto input_values = get_typed_values(inputs, impl_->inputs);
    auto output_values = get_typed_values(outputs, impl_->outputs);
    impl_->session->Run(Ort::RunOptions{nullptr}, impl_->input_names.data(), input_values.data(),
        input_values.size(), impl_->output_names.data(), output_values.data(), output_values.size());
}

void Model::infer_into(const Tensors& inputs, Tensors& outputs) {
    std::lock_guard<std::mutex> lock(impl_->mutex);
    auto input_values = get_values(inputs, impl_->inputs);
    auto output_values = get_values(outputs, impl_->outputs);
    impl_->session->Run(Ort::RunOptions{nullptr}, impl_->input_names.data(), input_values.data(),
        input_values.size(), impl_->output_names.data(), output_values.data(), output_values.size());
}

std::future<Tensors> Model::infer_async(Tensors inputs) {
    return impl_->executor.submit([this, inputs = std::move(inputs)] { return infer(inputs); });
}

void Model::warm_up(const Tensors& inputs, int iterations) {
    if (iterations < 1) throw std::invalid_argument("Warm-up iterations must be positive");
    for (int i = 0; i < iterations; ++i) (void)infer(inputs);
}

void Model::close() noexcept { impl_->executor.close(); }

} // namespace sindre::ai::onnxruntime
