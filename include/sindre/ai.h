#pragma once

/// @file
/// @brief AI 模块统一入口；后端实现完全位于 C++ 编译单元中。
#include <sindre/ai/types.h>
#include <sindre/ai/execution.h>
#if defined(SINDRE_AI_ONNXRUNTIME)
#include <sindre/ai/onnxruntime.h>
#endif
#if defined(SINDRE_AI_TRT)
#include <sindre/ai/trt.h>
#endif
