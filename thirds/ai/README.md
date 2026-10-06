# AI dependencies

| Capability | Dependency | Source | Requirement | Default |
| --- | --- | --- | --- | --- |
| CPU inference | ONNX Runtime C/C++ SDK | installed package or `SINDRECPP_ONNXRUNTIME_ROOT` | 1.22+ | AI on, ORT on |
| CUDA inference | ONNX Runtime GPU + CUDA + cuDNN | installed SDK/runtime | matching versions | off |
| TensorRT inference | TensorRT + CUDA Toolkit | installed SDK or `SINDRECPP_TENSORRT_ROOT` | TensorRT 10.11+, CUDA 12+ | off |

ONNX Runtime and TensorRT are intentionally not fetched automatically: they are
large binary SDKs and must match the host compiler, GPU driver, CUDA and cuDNN.
The AI module copies discovered Windows runtime DLLs beside its executables.
