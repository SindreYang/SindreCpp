# AI module

## CMake switches

- `SINDRECPP_WITH_AI=ON` enables the module and `SindreCpp::Ai`.
- `SINDRECPP_AI_ONNXRUNTIME=ON` enables ONNX Runtime discovery.
- `SINDRECPP_AI_TRT=ON` enables TensorRT and CUDA discovery.
- `SINDRECPP_AI_CUDA=ON` enables ONNX Runtime CUDA provider compilation.
- `SINDRECPP_BUILD_GPU_TESTS=ON` enables real TensorRT execution tests.

Use `SINDRECPP_ONNXRUNTIME_ROOT`, `SINDRECPP_TENSORRT_ROOT`, and
`SINDRECPP_CUDNN_ROOT` when SDKs are not discoverable.

The ONNX Runtime model also exposes `try_create`, `try_infer`,
`try_infer_typed`, `try_infer_into`, `try_infer_typed_into`, `try_warm_up`, and
`try_infer_async`. These methods use General `Result` values and stable
`ai.onnxruntime.*` error contexts for applications that do not use exceptions
as ordinary control flow.

## Test order

Run `sindrecpp.ai.execution` first, then ONNX Runtime float/int32/float16 and
typed int64/bool tests. On Windows, the CMake test properties prepend the
backend runtime directories to PATH.
