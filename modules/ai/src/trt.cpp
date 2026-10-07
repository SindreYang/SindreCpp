#include <sindre/ai/trt.h>

#include <NvInfer.h>
#include <NvInferPlugin.h>
#if !defined(SINDRE_AI_TRT_DISPATCH)
#include <NvOnnxParser.h>
#endif
#include <cuda_runtime_api.h>

#if NV_TENSORRT_MAJOR != 10 || NV_TENSORRT_MINOR < 11
#error "This backend requires TensorRT 10.11.x or newer within the TensorRT 10 series."
#endif

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <fstream>
#include <iterator>
#include <limits>
#include <mutex>
#include <stdexcept>
#include <system_error>

namespace sindre::ai::trt {
namespace detail {

void check(cudaError_t result, const char* operation) {
    if (result != cudaSuccess)
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(result));
}

void require(bool success, const char* message) {
    if (!success) throw std::runtime_error(message);
}

class DeviceScope {
public:
    explicit DeviceScope(int device) {
        check(cudaGetDevice(&previous_), "Get CUDA device");
        check(cudaSetDevice(device), "Set CUDA device");
    }
    ~DeviceScope() { (void)cudaSetDevice(previous_); }
private:
    int previous_;
};

class Logger : public nvinfer1::ILogger {
public:
    void log(Severity severity, const char* message) noexcept override {
        if (severity <= Severity::kWARNING) std::fprintf(stderr, "TensorRT: %s\n", message);
    }
};

Logger& get_logger() {
    static Logger logger;
    return logger;
}

void init_plugins() {
    static std::once_flag once;
    std::call_once(once, [] {
        require(initLibNvInferPlugins(&get_logger(), ""), "Cannot initialize TensorRT plugins");
    });
}

nvinfer1::Dims get_dims(const std::vector<std::int64_t>& shape) {
    (void)get_element_count(shape);
    if (shape.size() > nvinfer1::Dims::MAX_DIMS)
        throw std::invalid_argument("Tensor rank exceeds TensorRT dimension limit");
    nvinfer1::Dims dims{};
    dims.nbDims = static_cast<int>(shape.size());
    for (std::size_t d = 0; d < shape.size(); ++d) {
        if (shape[d] > std::numeric_limits<int>::max())
            throw std::overflow_error("Tensor dimension exceeds TensorRT limit");
        dims.d[d] = static_cast<int>(shape[d]);
    }
    return dims;
}

std::vector<std::int64_t> get_shape(const nvinfer1::Dims& dims) {
    if (dims.nbDims < 0) throw std::runtime_error("Invalid TensorRT dimensions");
    return {dims.d, dims.d + dims.nbDims};
}

DataType get_type(nvinfer1::DataType type) {
    switch (type) {
        case nvinfer1::DataType::kFLOAT: return DataType::float32;
        case nvinfer1::DataType::kHALF: return DataType::float16;
        case nvinfer1::DataType::kINT8: return DataType::int8;
        case nvinfer1::DataType::kINT32: return DataType::int32;
        case nvinfer1::DataType::kBOOL: return DataType::bool8;
        default: return DataType::other;
    }
}

std::vector<char> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open model/engine: " + path.string());
    std::vector<char> bytes{std::istreambuf_iterator<char>(file), {}};
    if (bytes.empty() || file.bad()) throw std::runtime_error("Empty or unreadable model/engine");
    return bytes;
}

void save_file(const std::filesystem::path& path, const void* data, std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::overflow_error("Engine file is too large");
    if (std::filesystem::exists(path)) throw std::runtime_error("Engine destination already exists");
    std::ofstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot create engine: " + path.string());
    file.write(static_cast<const char*>(data), static_cast<std::streamsize>(size));
    file.flush();
    if (!file) throw std::runtime_error("Cannot write engine: " + path.string());
}

struct Buffer {
    explicit Buffer(int device) : device_id(device) {}
    Buffer(const Buffer&) = delete;
    Buffer& operator=(const Buffer&) = delete;
    ~Buffer() {
        int previous = 0;
        (void)cudaGetDevice(&previous);
        (void)cudaSetDevice(device_id);
        if (device) (void)cudaFree(device);
        if (host) (void)cudaFreeHost(host);
        (void)cudaSetDevice(previous);
    }
    void reserve(std::size_t bytes) {
        if (capacity >= bytes) return;
        void* new_device = nullptr;
        void* new_host = nullptr;
        check(cudaMalloc(&new_device, bytes), "Allocate device buffer");
        auto status = cudaMallocHost(&new_host, bytes);
        if (status != cudaSuccess) {
            (void)cudaFree(new_device);
            check(status, "Allocate pinned host buffer");
        }
        if (device) (void)cudaFree(device);
        if (host) (void)cudaFreeHost(host);
        device = new_device;
        host = new_host;
        capacity = bytes;
    }
    int device_id;
    void* device = nullptr;
    void* host = nullptr;
    std::size_t capacity = 0;
};

struct State {
    ~State() {
        int previous = 0;
        (void)cudaGetDevice(&previous);
        (void)cudaSetDevice(device_id);
        if (stream) (void)cudaStreamSynchronize(stream);
        buffers.clear();
        context.reset();
        engine.reset();
        runtime.reset();
        if (done) (void)cudaEventDestroy(done);
        if (stream) (void)cudaStreamDestroy(stream);
        (void)cudaSetDevice(previous);
    }
    int device_id = 0;
    std::unique_ptr<nvinfer1::IRuntime> runtime;
    std::unique_ptr<nvinfer1::IRuntime> lean_runtime;
    std::unique_ptr<nvinfer1::ICudaEngine> engine;
    std::unique_ptr<nvinfer1::IExecutionContext> context;
    cudaStream_t stream = nullptr;
    cudaEvent_t done = nullptr;
    std::vector<TensorInfo> inputs, outputs;
    std::vector<std::string> names;
    std::vector<std::unique_ptr<Buffer>> buffers;
    std::vector<TypedTensor> output_tensors;
    std::mutex mutex;
    bool busy = false;
};

} // namespace detail

BuildOptions BuildOptions::max_performance() {
    BuildOptions options;
    options.optimization_level = 5;
    return options;
}

BuildOptions BuildOptions::cross_gpu() {
    BuildOptions options;
    options.compatibility = Compatibility::ampere_plus;
    return options;
}

void convert_onnx(const std::filesystem::path& source,
                  const std::filesystem::path& destination,
                  const BuildOptions& options) {
#if defined(SINDRE_AI_TRT_DISPATCH)
    (void)source;
    (void)destination;
    (void)options;
    throw std::system_error(std::make_error_code(std::errc::function_not_supported),
                            "ONNX conversion requires the full TensorRT runtime");
#else
    if (options.device_id < 0 || !options.workspace_bytes || options.optimization_level < 0 ||
        options.optimization_level > 5 || options.max_aux_streams < 0)
        throw std::invalid_argument("Invalid TensorRT build options");
    if (options.compatibility != Compatibility::build_gpu &&
        options.compatibility != Compatibility::same_compute_capability &&
        options.compatibility != Compatibility::ampere_plus)
        throw std::invalid_argument("Invalid TensorRT compatibility mode");
    if (options.exclude_lean_runtime && !options.version_compatible)
        throw std::invalid_argument("exclude_lean_runtime requires version_compatible");
    if (std::filesystem::exists(destination))
        throw std::runtime_error("Engine destination already exists");
    detail::DeviceScope device(options.device_id);
    detail::init_plugins();
    auto& logger = detail::get_logger();
    std::unique_ptr<nvinfer1::IBuilder> builder(nvinfer1::createInferBuilder(logger));
    detail::require(bool(builder), "Cannot create TensorRT builder");
    std::unique_ptr<nvinfer1::INetworkDefinition> network(builder->createNetworkV2(0));
    std::unique_ptr<nvinfer1::IBuilderConfig> config(builder->createBuilderConfig());
    detail::require(bool(network) && bool(config), "Cannot create TensorRT network/configuration");
    std::unique_ptr<nvonnxparser::IParser> parser(nvonnxparser::createParser(*network, logger));
    detail::require(bool(parser), "Cannot create ONNX parser");
    if (options.version_compatible) parser->setFlag(nvonnxparser::OnnxParserFlag::kNATIVE_INSTANCENORM);
    const auto onnx = detail::read_file(source);
    if (!parser->parse(onnx.data(), onnx.size(), source.string().c_str())) {
        std::string error = "Cannot parse ONNX";
        for (int i = 0; i < parser->getNbErrors(); ++i)
            error += std::string("\n") + parser->getError(i)->desc();
        throw std::runtime_error(error);
    }
    config->setMemoryPoolLimit(nvinfer1::MemoryPoolType::kWORKSPACE, options.workspace_bytes);
    config->setBuilderOptimizationLevel(options.optimization_level);
    config->setMaxAuxStreams(options.max_aux_streams);
    if (options.fp16) config->setFlag(nvinfer1::BuilderFlag::kFP16);
    if (options.tf32) config->setFlag(nvinfer1::BuilderFlag::kTF32);
    else config->clearFlag(nvinfer1::BuilderFlag::kTF32);
    if (options.version_compatible) {
        config->setFlag(nvinfer1::BuilderFlag::kVERSION_COMPATIBLE);
        if (options.exclude_lean_runtime)
            config->setFlag(nvinfer1::BuilderFlag::kEXCLUDE_LEAN_RUNTIME);
    }
    if (options.compatibility == Compatibility::ampere_plus)
        config->setHardwareCompatibilityLevel(nvinfer1::HardwareCompatibilityLevel::kAMPERE_PLUS);
    else if (options.compatibility == Compatibility::same_compute_capability)
        config->setHardwareCompatibilityLevel(nvinfer1::HardwareCompatibilityLevel::kSAME_COMPUTE_CAPABILITY);
    else config->setHardwareCompatibilityLevel(nvinfer1::HardwareCompatibilityLevel::kNONE);

    bool dynamic = false;
    for (int i = 0; i < network->getNbInputs(); ++i) {
        auto* input = network->getInput(i);
        if (input->isShapeTensor())
            throw std::invalid_argument("Shape tensor inputs require a custom TensorRT builder");
        const auto shape = detail::get_shape(input->getDimensions());
        dynamic = dynamic || std::find(shape.begin(), shape.end(), -1) != shape.end();
    }
    for (std::size_t i = 0; i < options.profiles.size(); ++i) {
        bool found = false;
        for (int n = 0; n < network->getNbInputs(); ++n)
            found = found || options.profiles[i].name == network->getInput(n)->getName();
        if (!found) throw std::invalid_argument("Profile refers to an unknown input");
        for (std::size_t j = 0; j < i; ++j)
            if (options.profiles[j].name == options.profiles[i].name)
                throw std::invalid_argument("Duplicate input profile");
    }
    if (dynamic || !options.profiles.empty()) {
        auto* profile = builder->createOptimizationProfile();
        detail::require(profile != nullptr, "Cannot create optimization profile");
        for (int i = 0; i < network->getNbInputs(); ++i) {
            auto* input = network->getInput(i);
            const auto shape = detail::get_shape(input->getDimensions());
            const auto provided = std::find_if(options.profiles.begin(), options.profiles.end(),
                [&](const ShapeProfile& range) { return range.name == input->getName(); });
            ShapeProfile range{input->getName(), shape, shape, shape};
            if (provided != options.profiles.end()) range = *provided;
            else if (std::find(shape.begin(), shape.end(), -1) != shape.end())
                throw std::invalid_argument(std::string("Missing dynamic input profile: ") + input->getName());
            const auto min = detail::get_dims(range.min), opt = detail::get_dims(range.opt),
                       max = detail::get_dims(range.max);
            if (range.min.size() != shape.size() || range.opt.size() != shape.size() ||
                range.max.size() != shape.size()) throw std::invalid_argument("Profile rank mismatch");
            for (std::size_t d = 0; d < shape.size(); ++d)
                if (range.min[d] > range.opt[d] || range.opt[d] > range.max[d] ||
                    (shape[d] >= 0 && (range.min[d] != shape[d] || range.max[d] != shape[d])))
                    throw std::invalid_argument("Invalid profile range or fixed dimension mismatch");
            detail::require(profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kMIN, min) &&
                profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kOPT, opt) &&
                profile->setDimensions(input->getName(), nvinfer1::OptProfileSelector::kMAX, max),
                "Cannot set optimization profile dimensions");
        }
        detail::require(profile->isValid() && config->addOptimizationProfile(profile) >= 0,
                        "Invalid TensorRT optimization profile");
    }
    std::unique_ptr<nvinfer1::IHostMemory> plan(builder->buildSerializedNetwork(*network, *config));
    detail::require(bool(plan), "TensorRT engine build failed; inspect TensorRT diagnostics");
    detail::save_file(destination, plan->data(), plan->size());
#endif
}

::sindre::general::Result<void> try_convert_onnx(
    const std::filesystem::path& source,
    const std::filesystem::path& destination,
    const BuildOptions& options) noexcept {
#if defined(SINDRE_AI_TRT_DISPATCH)
    (void)source;
    (void)destination;
    (void)options;
    return ::sindre::general::Result<void>::failure(
        std::make_error_code(std::errc::function_not_supported),
        "ONNX conversion requires the full TensorRT runtime",
        "ai.tensorrt.dispatch.convert");
#else
    return ::sindre::ai::detail::guarded(
        [&] { convert_onnx(source, destination, options); },
        "ai.tensorrt.convert");
#endif
}

Pending::Pending(std::shared_ptr<void> state, bool host_outputs)
    : state_(std::move(state)), host_outputs_(host_outputs) {}

Pending::Pending(Pending&& other) noexcept
    : state_(std::move(other.state_)), host_outputs_(other.host_outputs_) {}

Pending& Pending::operator=(Pending&& other) noexcept {
    if (this != &other) {
        release();
        state_ = std::move(other.state_);
        host_outputs_ = other.host_outputs_;
    }
    return *this;
}

Pending::~Pending() { release(); }

bool Pending::ready() const {
    if (!state_) throw std::logic_error("Completion ticket has already been consumed");
    auto state = std::static_pointer_cast<detail::State>(state_);
    detail::DeviceScope device(state->device_id);
    const auto result = cudaEventQuery(state->done);
    if (result == cudaErrorNotReady) return false;
    detail::check(result, "Query inference completion");
    return true;
}

::sindre::general::Result<bool> Pending::try_ready() const noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return ready(); }, "ai.tensorrt.pending.ready");
}

void Pending::wait() {
    if (!state_) throw std::logic_error("Completion ticket has already been consumed");
    auto state = std::static_pointer_cast<detail::State>(state_);
    detail::DeviceScope device(state->device_id);
    detail::check(cudaEventSynchronize(state->done), "Wait for inference completion");
    release();
}

::sindre::general::Result<void> Pending::try_wait() noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { wait(); }, "ai.tensorrt.pending.wait");
}

Tensors Pending::get() {
    auto typed = get_typed();
    Tensors outputs;
    outputs.reserve(typed.size());
    for (auto &output : typed) {
        if (output.type != DataType::float32)
            throw std::invalid_argument("Use get_typed() for non-float32 TensorRT outputs");
        Tensor value{std::move(output.shape), {}};
        const auto count = output.data.size() / sizeof(float);
        value.data.resize(count);
        if (!output.data.empty()) std::memcpy(value.data.data(), output.data.data(), output.data.size());
        outputs.push_back(std::move(value));
    }
    return outputs;
}

::sindre::general::Result<Tensors> Pending::try_get() noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return get(); }, "ai.tensorrt.pending.get");
}

TypedTensors Pending::get_typed() {
    if (!host_outputs_) throw std::logic_error("Device inference returns caller-owned GPU outputs; use wait()");
    if (!state_) throw std::logic_error("Completion ticket has already been consumed");
    auto state = std::static_pointer_cast<detail::State>(state_);
    detail::DeviceScope device(state->device_id);
    detail::check(cudaEventSynchronize(state->done), "Wait for inference completion");
    auto outputs = state->output_tensors;
    std::size_t index = 0;
    for (std::size_t i = 0; i < state->names.size(); ++i) {
        if (state->engine->getTensorIOMode(state->names[i].c_str()) != nvinfer1::TensorIOMode::kOUTPUT)
            continue;
        const auto bytes = get_element_count(outputs[index].shape) *
                           data_type_size(outputs[index].type);
        outputs[index].data.resize(bytes);
        if (bytes) std::memcpy(outputs[index].data.data(), state->buffers[i]->host, bytes);
        ++index;
    }
    release();
    return outputs;
}

::sindre::general::Result<TypedTensors> Pending::try_get_typed() noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return get_typed(); }, "ai.tensorrt.pending.get_typed");
}

void Pending::release() noexcept {
    if (!state_) return;
    auto state = std::static_pointer_cast<detail::State>(state_);
    int previous = 0;
    (void)cudaGetDevice(&previous);
    (void)cudaSetDevice(state->device_id);
    (void)cudaStreamSynchronize(state->stream);
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        state->busy = false;
    }
    state_.reset();
    (void)cudaSetDevice(previous);
}

class Model::Impl {
public:
    Impl(const std::filesystem::path& path, const LoadOptions& options)
        : executor(options.queue_capacity) {
        if (options.device_id < 0 || options.profile_index < 0)
            throw std::invalid_argument("Invalid engine load options");
        auto state = std::make_shared<detail::State>();
        state->device_id = options.device_id;
        detail::DeviceScope device(options.device_id);
        detail::init_plugins();
#if defined(SINDRE_AI_TRT_DISPATCH)
        if (options.lean_runtime_path.empty())
            throw std::invalid_argument(
                "Dispatch TensorRT runtime requires an external lean runtime path");
#endif
        state->runtime.reset(nvinfer1::createInferRuntime(detail::get_logger()));
        detail::require(bool(state->runtime), "Cannot create TensorRT runtime");
        state->runtime->setEngineHostCodeAllowed(options.allow_engine_host_code);
        if (!options.lean_runtime_path.empty()) {
            state->lean_runtime.reset(
                state->runtime->loadRuntime(options.lean_runtime_path.string().c_str()));
            detail::require(bool(state->lean_runtime), "Cannot load TensorRT lean runtime");
            state->lean_runtime->setEngineHostCodeAllowed(options.allow_engine_host_code);
        }
        auto bytes = detail::read_file(path);
        auto* runtime = state->lean_runtime ? state->lean_runtime.get() : state->runtime.get();
        state->engine.reset(runtime->deserializeCudaEngine(bytes.data(), bytes.size()));
        detail::require(bool(state->engine), "Cannot deserialize engine; check GPU/runtime compatibility and trust options");
        if (options.profile_index >= state->engine->getNbOptimizationProfiles())
            throw std::invalid_argument("Optimization profile index out of range");
        state->context.reset(state->engine->createExecutionContext());
        detail::require(bool(state->context), "Cannot create execution context");
        detail::check(cudaStreamCreateWithFlags(&state->stream, cudaStreamNonBlocking), "Create CUDA stream");
        detail::check(cudaEventCreateWithFlags(&state->done, cudaEventDisableTiming), "Create completion event");
        detail::require(state->context->setOptimizationProfileAsync(options.profile_index, state->stream),
                        "Cannot select optimization profile");
        for (int i = 0; i < state->engine->getNbIOTensors(); ++i) {
            std::string name = state->engine->getIOTensorName(i);
            const auto type = detail::get_type(state->engine->getTensorDataType(name.c_str()));
            if (type == DataType::other ||
                state->engine->getTensorLocation(name.c_str()) != nvinfer1::TensorLocation::kDEVICE ||
                state->engine->getTensorFormat(name.c_str()) != nvinfer1::TensorFormat::kLINEAR ||
                state->engine->isShapeInferenceIO(name.c_str()))
                throw std::invalid_argument("Convenience engine API requires device, linear supported-type I/O and no shape inputs");
            TensorInfo info{name, detail::get_shape(state->engine->getTensorShape(name.c_str())), type};
            if (state->engine->getTensorIOMode(name.c_str()) == nvinfer1::TensorIOMode::kINPUT)
                state->inputs.push_back(info);
            else state->outputs.push_back(info);
            state->names.push_back(std::move(name));
            state->buffers.push_back(std::make_unique<detail::Buffer>(options.device_id));
        }
        if (state->inputs.empty() || state->outputs.empty())
            throw std::invalid_argument("Engine has no tensor inputs/outputs");
        state_ = std::move(state);
    }
    std::shared_ptr<detail::State> state_;
    ::sindre::ai::detail::Executor executor;
    std::mutex infer_mutex;
};

Model::Model(const std::filesystem::path& path, const LoadOptions& options)
    : impl_(std::make_unique<Impl>(path, options)) {}

Model::~Model() = default;

const std::vector<TensorInfo>& Model::get_inputs() const noexcept { return impl_->state_->inputs; }
const std::vector<TensorInfo>& Model::get_outputs() const noexcept { return impl_->state_->outputs; }

::sindre::general::Result<std::shared_ptr<Model>> Model::try_create(
    const std::filesystem::path& path, const LoadOptions& options) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return std::make_shared<Model>(path, options); },
        "ai.tensorrt.create");
}

Tensors Model::infer(const Tensors& inputs) {
    std::lock_guard<std::mutex> lock(impl_->infer_mutex);
    return enqueue(inputs).get();
}

::sindre::general::Result<Tensors> Model::try_infer(const Tensors& inputs) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return infer(inputs); }, "ai.tensorrt.infer");
}

std::future<Tensors> Model::infer_async(Tensors inputs) {
    return impl_->executor.submit([this, inputs = std::move(inputs)] { return infer(inputs); });
}

::sindre::general::Result<std::future<::sindre::general::Result<Tensors>>>
Model::try_infer_async(Tensors inputs,
                       ::sindre::general::CancellationToken token) noexcept {
    ::sindre::general::TaskOptions options;
    options.token = std::move(token);
    return try_infer_async(std::move(inputs), std::move(options));
}

::sindre::general::Result<std::future<::sindre::general::Result<Tensors>>>
Model::try_infer_async(Tensors inputs,
                       ::sindre::general::TaskOptions options) noexcept {
    return ::sindre::general::try_run_async(
        [this, inputs = std::move(inputs)](::sindre::general::CancellationToken task_token) mutable {
#if !defined(SINDRE_NO_EXCEPTIONS)
            if (task_token.is_cancelled()) throw std::runtime_error("AI inference cancelled");
#else
            (void)task_token;
#endif
            return infer(inputs);
        }, std::move(options));
}

void Model::warm_up(const Tensors& inputs, int iterations) {
    if (iterations < 1) throw std::invalid_argument("Warm-up iterations must be positive");
    for (int i = 0; i < iterations; ++i) (void)infer(inputs);
}

::sindre::general::Result<void> Model::try_warm_up(
    const Tensors& inputs, int iterations) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { warm_up(inputs, iterations); }, "ai.tensorrt.warm_up");
}

void Model::close() noexcept { impl_->executor.close(); }

Pending Model::enqueue(const Tensors& inputs) {
    TypedTensors typed;
    typed.reserve(inputs.size());
    for (const auto &input : inputs) {
        typed.push_back({input.shape, DataType::float32, {}});
        typed.back().data.resize(input.data.size() * sizeof(float));
        if (!input.data.empty()) std::memcpy(typed.back().data.data(), input.data.data(), typed.back().data.size());
    }
    return enqueue_typed(typed);
}

::sindre::general::Result<Pending> Model::try_enqueue(
    const Tensors& inputs) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return enqueue(inputs); }, "ai.tensorrt.enqueue");
}

Pending Model::enqueue_typed(const TypedTensors& inputs) {
    auto state = impl_->state_;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->busy) throw std::runtime_error("Engine already has an outstanding CUDA ticket");
        state->busy = true;
    }
    Pending ticket(std::static_pointer_cast<void>(state));
    detail::DeviceScope device(state->device_id);
    if (inputs.size() != state->inputs.size()) throw std::invalid_argument("Engine input count mismatch");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        inputs[i].validate();
        if (inputs[i].type != state->inputs[i].type)
            throw std::invalid_argument("Engine input data type mismatch");
        detail::require(state->context->setInputShape(state->inputs[i].name.c_str(), detail::get_dims(inputs[i].shape)),
                        "Input shape is outside engine profile or inconsistent with model");
    }
    state->output_tensors.clear();
    std::size_t input_index = 0;
    std::size_t output_index = 0;
    for (std::size_t i = 0; i < state->names.size(); ++i) {
        const auto* name = state->names[i].c_str();
        auto shape = detail::get_shape(state->context->getTensorShape(name));
        const auto count = get_element_count(shape);
        const auto type = state->engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT
            ? state->inputs[input_index].type
            : state->outputs[output_index].type;
        const auto bytes_per_element = data_type_size(type);
        if (count > std::numeric_limits<std::size_t>::max() / bytes_per_element)
            throw std::overflow_error("Tensor byte count overflow");
        const auto bytes = count * bytes_per_element;
        state->buffers[i]->reserve(bytes);
        detail::require(state->context->setTensorAddress(name, state->buffers[i]->device), "Cannot bind engine tensor");
        if (state->engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) {
            if (shape != inputs[input_index].shape) throw std::invalid_argument("Engine input shape mismatch");
            if (inputs[input_index].data.size() != bytes)
                throw std::invalid_argument("Engine input byte count mismatch");
            std::memcpy(state->buffers[i]->host, inputs[input_index++].data.data(), bytes);
            detail::check(cudaMemcpyAsync(state->buffers[i]->device, state->buffers[i]->host, bytes,
                cudaMemcpyHostToDevice, state->stream), "Upload input");
        } else {
            state->output_tensors.push_back({std::move(shape), type, {}});
            ++output_index;
        }
    }
    detail::require(state->context->inferShapes(0, nullptr) == 0, "Engine shapes are not fully specified");
    detail::require(state->context->enqueueV3(state->stream), "TensorRT enqueueV3 failed");
    std::size_t downloaded_output_index = 0;
    for (std::size_t i = 0; i < state->names.size(); ++i) {
        const auto* name = state->names[i].c_str();
        if (state->engine->getTensorIOMode(name) != nvinfer1::TensorIOMode::kOUTPUT) continue;
        const auto count = get_element_count(detail::get_shape(state->context->getTensorShape(name)));
        const auto bytes = count * data_type_size(state->outputs[downloaded_output_index++].type);
        detail::check(cudaMemcpyAsync(state->buffers[i]->host, state->buffers[i]->device,
            bytes,
            cudaMemcpyDeviceToHost, state->stream), "Download output");
    }
    detail::check(cudaEventRecord(state->done, state->stream), "Record inference completion");
    return ticket;
}

::sindre::general::Result<Pending> Model::try_enqueue_typed(
    const TypedTensors& inputs) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return enqueue_typed(inputs); }, "ai.tensorrt.enqueue_typed");
}

TypedTensors Model::infer_typed(const TypedTensors& inputs) {
    std::lock_guard<std::mutex> lock(impl_->infer_mutex);
    return enqueue_typed(inputs).get_typed();
}

::sindre::general::Result<TypedTensors> Model::try_infer_typed(
    const TypedTensors& inputs) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return infer_typed(inputs); }, "ai.tensorrt.infer_typed");
}

std::future<TypedTensors> Model::infer_typed_async(TypedTensors inputs) {
    return impl_->executor.submit([this, inputs = std::move(inputs)] {
        return infer_typed(inputs);
    });
}

::sindre::general::Result<std::future<::sindre::general::Result<TypedTensors>>>
Model::try_infer_typed_async(TypedTensors inputs,
                             ::sindre::general::CancellationToken token) noexcept {
    ::sindre::general::TaskOptions options;
    options.token = std::move(token);
    return try_infer_typed_async(std::move(inputs), std::move(options));
}

::sindre::general::Result<std::future<::sindre::general::Result<TypedTensors>>>
Model::try_infer_typed_async(TypedTensors inputs,
                             ::sindre::general::TaskOptions options) noexcept {
    return ::sindre::general::try_run_async(
        [this, inputs = std::move(inputs)](::sindre::general::CancellationToken task_token) mutable {
#if !defined(SINDRE_NO_EXCEPTIONS)
            if (task_token.is_cancelled()) throw std::runtime_error("AI inference cancelled");
#else
            (void)task_token;
#endif
            return infer_typed(inputs);
        }, std::move(options));
}

Pending Model::enqueue_device(const std::vector<DeviceTensorView>& inputs,
                              const std::vector<DeviceTensorView>& outputs,
                              void* ready_event) {
    auto state = impl_->state_;
    {
        std::lock_guard<std::mutex> lock(state->mutex);
        if (state->busy) throw std::runtime_error("Engine already has an outstanding CUDA ticket");
        state->busy = true;
    }
    Pending ticket(std::static_pointer_cast<void>(state), false);
    detail::DeviceScope device(state->device_id);
    if (inputs.size() != state->inputs.size() || outputs.size() != state->outputs.size())
        throw std::invalid_argument("Device tensor count mismatch");
    auto validate = [](const DeviceTensorView& tensor, DataType expected_type) {
        if (!tensor.data || tensor.type != expected_type ||
            get_element_count(tensor.shape) != tensor.elements)
            throw std::invalid_argument("Device tensor pointer/shape/size mismatch");
    };
    if (ready_event)
        detail::check(cudaStreamWaitEvent(state->stream, static_cast<cudaEvent_t>(ready_event), 0),
                      "Wait for producer stream");
    for (std::size_t i = 0; i < inputs.size(); ++i) {
        validate(inputs[i], state->inputs[i].type);
        const auto* name = state->inputs[i].name.c_str();
        detail::require(state->context->setInputShape(name, detail::get_dims(inputs[i].shape)),
                        "Device input shape outside engine profile");
        detail::require(state->context->setTensorAddress(name, inputs[i].data), "Cannot bind device input");
    }
    detail::require(state->context->inferShapes(0, nullptr) == 0, "Device shapes are not fully specified");
    for (std::size_t i = 0; i < outputs.size(); ++i) {
        validate(outputs[i], state->outputs[i].type);
        const auto* name = state->outputs[i].name.c_str();
        if (outputs[i].shape != detail::get_shape(state->context->getTensorShape(name)))
            throw std::invalid_argument("Device output shape mismatch; data-dependent outputs are unsupported");
        detail::require(state->context->setTensorAddress(name, outputs[i].data), "Cannot bind device output");
    }
    detail::require(state->context->enqueueV3(state->stream), "Device enqueueV3 failed");
    detail::check(cudaEventRecord(state->done, state->stream), "Record device inference completion");
    return ticket;
}

::sindre::general::Result<Pending> Model::try_enqueue_device(
    const std::vector<DeviceTensorView>& inputs,
    const std::vector<DeviceTensorView>& outputs,
    void* ready_event) noexcept {
    return ::sindre::ai::detail::guarded(
        [&] { return enqueue_device(inputs, outputs, ready_event); },
        "ai.tensorrt.enqueue_device");
}

} // namespace sindre::ai::trt
