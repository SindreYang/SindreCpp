#pragma once

#if !defined(SINDRECPP_AI_TRT)
#error "Enable SINDRECPP_WITH_AI and SINDRECPP_AI_TRT; link SindreCpp::Ai."
#endif

#include <ai/types.hpp>
#include <ai/execution.hpp>
#include <NvInfer.h>
#include <NvInferPlugin.h>
#include <NvOnnxParser.h>
#include <cuda_runtime_api.h>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <memory>
#include <mutex>
#include <utility>

#if NV_TENSORRT_MAJOR != 10 || NV_TENSORRT_MINOR < 11
#error "This backend requires TensorRT 10.11.x or newer within the TensorRT 10 series."
#endif

namespace sindrecpp::ai::trt {

namespace native = nvinfer1;
enum class Compatibility { build_gpu, same_compute_capability, ampere_plus };

struct ShapeProfile {
    std::string name;
    std::vector<std::int64_t> min, opt, max;
};

struct BuildOptions {
    int device_id = 0;
    std::size_t workspace_bytes = std::size_t{1} << 30;
    int optimization_level = 3; // 0..5; more build time does not guarantee faster inference.
    int max_aux_streams = 0;
    bool fp16 = false;
    bool tf32 = true;
    bool version_compatible = false; // Load requires explicit trust in embedded runtime code.
    Compatibility compatibility = Compatibility::build_gpu;
    std::function<void(nvinfer1::INetworkDefinition&, nvinfer1::IBuilderConfig&)> configure;
    std::vector<ShapeProfile> profiles; // One profile, a range per dynamic input.
    static BuildOptions max_performance() {
        BuildOptions options;
        options.optimization_level = 5;
        return options; // No compatibility constraints; precision remains a user decision.
    }
    static BuildOptions cross_gpu() {
        BuildOptions options;
        options.compatibility = Compatibility::ampere_plus;
        return options;
    }
};

struct LoadOptions {
    int device_id = 0;
    int profile_index = 0;
    std::size_t queue_capacity = 16;
    bool allow_engine_host_code = false;
};

namespace detail {

inline void check(cudaError_t result, const char* operation) {
    if (result != cudaSuccess)
        throw std::runtime_error(std::string(operation) + ": " + cudaGetErrorString(result));
}
inline void require(bool success, const char* message) {
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
inline Logger& get_logger() { static Logger logger; return logger; }
inline void init_plugins() {
    static std::once_flag once;
    std::call_once(once, [] {
        require(initLibNvInferPlugins(&get_logger(), ""), "Cannot initialize TensorRT plugins");
    });
}
inline nvinfer1::Dims get_dims(const std::vector<std::int64_t>& shape) {
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
inline std::vector<std::int64_t> get_shape(const nvinfer1::Dims& dims) {
    if (dims.nbDims < 0) throw std::runtime_error("Invalid TensorRT dimensions");
    return {dims.d, dims.d + dims.nbDims};
}
inline std::vector<char> read_file(const std::filesystem::path& path) {
    std::ifstream file(path, std::ios::binary);
    if (!file) throw std::runtime_error("Cannot open model/engine: " + path.string());
    std::vector<char> bytes{std::istreambuf_iterator<char>(file), {}};
    if (bytes.empty() || file.bad()) throw std::runtime_error("Empty or unreadable model/engine");
    return bytes;
}
inline void save_file(const std::filesystem::path& path, const void* data, std::size_t size) {
    if (size > static_cast<std::size_t>(std::numeric_limits<std::streamsize>::max()))
        throw std::overflow_error("Engine file is too large");
    // Use a new destination; overwriting an existing engine must be an explicit caller action.
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
    std::unique_ptr<nvinfer1::ICudaEngine> engine;
    std::unique_ptr<nvinfer1::IExecutionContext> context;
    cudaStream_t stream = nullptr;
    cudaEvent_t done = nullptr;
    std::vector<TensorInfo> inputs, outputs;
    std::vector<std::string> names;
    std::vector<std::unique_ptr<Buffer>> buffers;
    std::vector<Tensor> output_shapes;
    std::mutex mutex;
    bool busy = false;
};
} // namespace detail

// Builds a serialized engine. No ONNX Runtime dependency. Only trusted ONNX models.
inline void convert_onnx(const std::filesystem::path& source, const std::filesystem::path& destination,
                         const BuildOptions& options = {}) {
    if (options.device_id < 0 || !options.workspace_bytes || options.optimization_level < 0 ||
        options.optimization_level > 5 || options.max_aux_streams < 0)
        throw std::invalid_argument("Invalid TensorRT build options");
    if (options.compatibility != Compatibility::build_gpu &&
        options.compatibility != Compatibility::same_compute_capability &&
        options.compatibility != Compatibility::ampere_plus)
        throw std::invalid_argument("Invalid TensorRT compatibility mode");
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
    // Native InstanceNorm avoids parser plugin dependence in version-compatible plans.
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
    if (options.version_compatible) config->setFlag(nvinfer1::BuilderFlag::kVERSION_COMPATIBLE);
    if (options.compatibility == Compatibility::ampere_plus)
        config->setHardwareCompatibilityLevel(nvinfer1::HardwareCompatibilityLevel::kAMPERE_PLUS);
    else if (options.compatibility == Compatibility::same_compute_capability)
        config->setHardwareCompatibilityLevel(nvinfer1::HardwareCompatibilityLevel::kSAME_COMPUTE_CAPABILITY);
    else config->setHardwareCompatibilityLevel(nvinfer1::HardwareCompatibilityLevel::kNONE);

    bool dynamic = false;
    for (int i = 0; i < network->getNbInputs(); ++i) {
        auto* input = network->getInput(i);
        if (input->isShapeTensor()) throw std::invalid_argument("Shape tensor inputs require a custom TensorRT builder");
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
        // Builder owns this pointer; never delete it.
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
    if (options.configure) options.configure(*network, *config);
    std::unique_ptr<nvinfer1::IHostMemory> plan(builder->buildSerializedNetwork(*network, *config));
    detail::require(bool(plan), "TensorRT engine build failed; inspect TensorRT diagnostics");
    detail::save_file(destination, plan->data(), plan->size());
}

// A CUDA completion ticket. Owns the engine state until completion; may be moved across threads.
// Destruction waits for GPU completion, so buffers cannot be freed while work is running.
class Pending {
public:
    explicit Pending(std::shared_ptr<detail::State> state, bool host_outputs = true)
        : state_(std::move(state)), host_outputs_(host_outputs) {}
    Pending(const Pending&) = delete;
    Pending& operator=(const Pending&) = delete;
    Pending(Pending&&) noexcept = default;
    Pending& operator=(Pending&& other) noexcept {
        if (this != &other) { release(); state_ = std::move(other.state_); host_outputs_ = other.host_outputs_; }
        return *this;
    }
    ~Pending() { release(); }
    bool ready() const {
        if (!state_) throw std::logic_error("Completion ticket has already been consumed");
        detail::DeviceScope device(state_->device_id);
        const auto result = cudaEventQuery(state_->done);
        if (result == cudaErrorNotReady) return false;
        detail::check(result, "Query inference completion");
        return true;
    }
    void wait() {
        if (!state_) throw std::logic_error("Completion ticket has already been consumed");
        detail::DeviceScope device(state_->device_id);
        detail::check(cudaEventSynchronize(state_->done), "Wait for inference completion");
        release();
    }
    Tensors get() {
        if (!host_outputs_) throw std::logic_error("Device inference returns caller-owned GPU outputs; use wait()");
        if (!state_) throw std::logic_error("Completion ticket has already been consumed");
        detail::DeviceScope device(state_->device_id);
        detail::check(cudaEventSynchronize(state_->done), "Wait for inference completion");
        auto outputs = state_->output_shapes;
        std::size_t index = 0;
        for (std::size_t i = 0; i < state_->names.size(); ++i) {
            if (state_->engine->getTensorIOMode(state_->names[i].c_str()) != nvinfer1::TensorIOMode::kOUTPUT)
                continue;
            const auto count = get_element_count(outputs[index].shape);
            auto* data = static_cast<const float*>(state_->buffers[i]->host);
            outputs[index++].data.assign(data, data + count);
        }
        release();
        return outputs;
    }
private:
    void release() noexcept {
        if (!state_) return;
        int previous = 0;
        (void)cudaGetDevice(&previous);
        (void)cudaSetDevice(state_->device_id);
        // Stream sync also covers failures occurring before event recording.
        (void)cudaStreamSynchronize(state_->stream);
        {
            std::lock_guard<std::mutex> lock(state_->mutex);
            state_->busy = false;
        }
        auto old = std::move(state_);
        (void)cudaSetDevice(previous);
    }
    std::shared_ptr<detail::State> state_;
    bool host_outputs_ = true;
};

struct DeviceTensorView {
    std::vector<std::int64_t> shape;
    float* data = nullptr;
    std::size_t elements = 0;
};

class Model {
public:
    explicit Model(const std::filesystem::path& path, const LoadOptions& options = {})
        : executor_(options.queue_capacity) {
        if (options.device_id < 0 || options.profile_index < 0)
            throw std::invalid_argument("Invalid engine load options");
        auto state = std::make_shared<detail::State>();
        state->device_id = options.device_id;
        detail::DeviceScope device(options.device_id);
        detail::init_plugins();
        state->runtime.reset(nvinfer1::createInferRuntime(detail::get_logger()));
        detail::require(bool(state->runtime), "Cannot create TensorRT runtime");
        state->runtime->setEngineHostCodeAllowed(options.allow_engine_host_code);
        auto bytes = detail::read_file(path);
        state->engine.reset(state->runtime->deserializeCudaEngine(bytes.data(), bytes.size()));
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
            if (state->engine->getTensorDataType(name.c_str()) != nvinfer1::DataType::kFLOAT ||
                state->engine->getTensorLocation(name.c_str()) != nvinfer1::TensorLocation::kDEVICE ||
                state->engine->getTensorFormat(name.c_str()) != nvinfer1::TensorFormat::kLINEAR ||
                state->engine->isShapeInferenceIO(name.c_str()))
                throw std::invalid_argument("Convenience engine API requires device, linear float32 I/O and no shape inputs");
            TensorInfo info{name, detail::get_shape(state->engine->getTensorShape(name.c_str())), DataType::float32};
            if (state->engine->getTensorIOMode(name.c_str()) == nvinfer1::TensorIOMode::kINPUT)
                state->inputs.push_back(info);
            else state->outputs.push_back(info);
            state->names.push_back(std::move(name));
            state->buffers.push_back(std::make_unique<detail::Buffer>(options.device_id));
        }
        if (state->inputs.empty() || state->outputs.empty()) throw std::invalid_argument("Engine has no tensor inputs/outputs");
        state_ = std::move(state);
    }
    Model(const Model&) = delete;
    Model& operator=(const Model&) = delete;
    ~Model() { executor_.close(); }

    const std::vector<TensorInfo>& get_inputs() const noexcept { return state_->inputs; }
    const std::vector<TensorInfo>& get_outputs() const noexcept { return state_->outputs; }

    Tensors infer(const Tensors& inputs) {
        std::lock_guard<std::mutex> lock(infer_mutex_);
        return enqueue(inputs).get();
    }
    std::future<Tensors> infer_async(Tensors inputs) {
        return executor_.submit([this, inputs = std::move(inputs)] { return infer(inputs); });
    }
    void warm_up(const Tensors& inputs, int iterations = 1) {
        if (iterations < 1) throw std::invalid_argument("Warm-up iterations must be positive");
        for (int i = 0; i < iterations; ++i) (void)infer(inputs);
    }
    void close() noexcept { executor_.close(); }

    // Immediate CUDA enqueue; at most one live ticket per Model. Busy models reject new enqueues.
    // Input values are copied to owned pinned memory before this call returns.
    Pending enqueue(const Tensors& inputs) {
        auto state = state_;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->busy) throw std::runtime_error("Engine already has an outstanding CUDA ticket");
            state->busy = true;
        }
        Pending ticket(state); // On every failure, drains queued CUDA work and clears busy.
        detail::DeviceScope device(state->device_id);
        if (inputs.size() != state->inputs.size()) throw std::invalid_argument("Engine input count mismatch");
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            inputs[i].validate();
            detail::require(state->context->setInputShape(state->inputs[i].name.c_str(), detail::get_dims(inputs[i].shape)),
                            "Input shape is outside engine profile or inconsistent with model");
        }
        state->output_shapes.clear();
        std::size_t input_index = 0;
        for (std::size_t i = 0; i < state->names.size(); ++i) {
            const auto* name = state->names[i].c_str();
            auto shape = detail::get_shape(state->context->getTensorShape(name));
            const auto count = get_element_count(shape); // Reject unresolved/data-dependent output shapes.
            if (count > std::numeric_limits<std::size_t>::max() / sizeof(float))
                throw std::overflow_error("Tensor byte count overflow");
            const auto bytes = count * sizeof(float);
            state->buffers[i]->reserve(bytes);
            detail::require(state->context->setTensorAddress(name, state->buffers[i]->device), "Cannot bind engine tensor");
            if (state->engine->getTensorIOMode(name) == nvinfer1::TensorIOMode::kINPUT) {
                if (shape != inputs[input_index].shape) throw std::invalid_argument("Engine input shape mismatch");
                std::memcpy(state->buffers[i]->host, inputs[input_index++].data.data(), bytes);
                detail::check(cudaMemcpyAsync(state->buffers[i]->device, state->buffers[i]->host, bytes,
                    cudaMemcpyHostToDevice, state->stream), "Upload input");
            } else state->output_shapes.push_back({std::move(shape), {}});
        }
        detail::require(state->context->inferShapes(0, nullptr) == 0, "Engine shapes are not fully specified");
        detail::require(state->context->enqueueV3(state->stream), "TensorRT enqueueV3 failed");
        for (std::size_t i = 0; i < state->names.size(); ++i) {
            const auto* name = state->names[i].c_str();
            if (state->engine->getTensorIOMode(name) != nvinfer1::TensorIOMode::kOUTPUT) continue;
            const auto count = get_element_count(detail::get_shape(state->context->getTensorShape(name)));
            detail::check(cudaMemcpyAsync(state->buffers[i]->host, state->buffers[i]->device,
                count * sizeof(float), cudaMemcpyDeviceToHost, state->stream), "Download output");
        }
        detail::check(cudaEventRecord(state->done, state->stream), "Record inference completion");
        return ticket;
    }
    // Zero host copies: caller owns GPU buffers on this device until ticket completion.
    // ready_event optionally orders this stream after another producer stream.
    Pending enqueue_device(const std::vector<DeviceTensorView>& inputs,
                           const std::vector<DeviceTensorView>& outputs,
                           cudaEvent_t ready_event = nullptr) {
        auto state = state_;
        {
            std::lock_guard<std::mutex> lock(state->mutex);
            if (state->busy) throw std::runtime_error("Engine already has an outstanding CUDA ticket");
            state->busy = true;
        }
        Pending ticket(state, false);
        detail::DeviceScope device(state->device_id);
        if (inputs.size() != state->inputs.size() || outputs.size() != state->outputs.size())
            throw std::invalid_argument("Device tensor count mismatch");
        auto validate = [](const DeviceTensorView& tensor) {
            if (!tensor.data || get_element_count(tensor.shape) != tensor.elements)
                throw std::invalid_argument("Device tensor pointer/shape/size mismatch");
        };
        if (ready_event) detail::check(cudaStreamWaitEvent(state->stream, ready_event, 0), "Wait for producer stream");
        for (std::size_t i = 0; i < inputs.size(); ++i) {
            validate(inputs[i]);
            const auto* name = state->inputs[i].name.c_str();
            detail::require(state->context->setInputShape(name, detail::get_dims(inputs[i].shape)),
                            "Device input shape outside engine profile");
            detail::require(state->context->setTensorAddress(name, inputs[i].data), "Cannot bind device input");
        }
        detail::require(state->context->inferShapes(0, nullptr) == 0, "Device shapes are not fully specified");
        for (std::size_t i = 0; i < outputs.size(); ++i) {
            validate(outputs[i]);
            const auto* name = state->outputs[i].name.c_str();
            if (outputs[i].shape != detail::get_shape(state->context->getTensorShape(name)))
                throw std::invalid_argument("Device output shape mismatch; data-dependent outputs are unsupported");
            detail::require(state->context->setTensorAddress(name, outputs[i].data), "Cannot bind device output");
        }
        detail::require(state->context->enqueueV3(state->stream), "Device enqueueV3 failed");
        detail::check(cudaEventRecord(state->done, state->stream), "Record device inference completion");
        return ticket;
    }

private:
    std::shared_ptr<detail::State> state_;
    std::mutex infer_mutex_;
    sindrecpp::ai::detail::Executor executor_; // Destroyed/drained before engine state.
};

} // namespace sindrecpp::ai::trt
