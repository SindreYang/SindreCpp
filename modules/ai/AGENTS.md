# AI module guidance

AI owns tensor types, cancellable execution, ONNX Runtime, and TensorRT. The
public module entry point is `include/ai/index.hpp`; backend headers
are opt-in implementation surfaces.

The public boundary must not leak backend exceptions. Use General Result/Error
and preserve cancellation, deadlines, retry/timeout decisions, and progress
callbacks. Keep backend-specific conversion code in the backend header.

ONNX Runtime tests use fixtures in `tests/models`. Typed fixtures are separate
for int32, float16, int64, and bool. A typed test failure can indicate a tensor
element-type mapping error even when float32 execution succeeds.
