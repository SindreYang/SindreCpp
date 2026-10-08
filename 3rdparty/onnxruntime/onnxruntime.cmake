# AI 后端的最低版本约束集中声明在这里；具体 SDK 路径由 Options 或宿主环境提供。
set(SINDRE_THIRD_AI_ONNXRUNTIME_MIN_VERSION
    "1.22" CACHE STRING "Minimum ONNX Runtime SDK version")
set(SINDRE_THIRD_AI_TENSORRT_MIN_VERSION
    "10.11" CACHE STRING "Minimum TensorRT SDK version")
set(SINDRE_THIRD_AI_CUDA_MIN_VERSION
    "12" CACHE STRING "Minimum CUDA Toolkit major version")
