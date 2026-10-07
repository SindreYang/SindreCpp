cmake_minimum_required(VERSION 3.20)
project(sindreExampleAiTensorRtSegmentation LANGUAGES CXX)

# The repository's fixed Windows dependency profile is static Release CRT with
# iterator debugging disabled. Keep a standalone example ABI-compatible with
# the libraries brought in below instead of inheriting CMake's Debug default.
if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release CACHE STRING "Build type" FORCE)
endif()
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded" CACHE STRING
        "Use the fixed static MSVC runtime" FORCE)
    add_compile_definitions(_ITERATOR_DEBUG_LEVEL=0)
endif()

# This example is independently configurable:
# cmake -S examples/ai_tensorrt_segmentation -B build_ai_seg -G Ninja \
#   -DSINDRE_TENSORRT_ROOT=C:/TensorRT -DCUDAToolkit_ROOT=C:/CUDA
if(NOT TARGET sindre::ai)
    set(SINDRE_BUILD_TESTS OFF CACHE BOOL "" FORCE)
    set(SINDRE_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
    set(SINDRE_WITH_AI ON CACHE BOOL "" FORCE)
    set(SINDRE_AI_ONNXRUNTIME OFF CACHE BOOL "" FORCE)
    set(SINDRE_AI_TRT ON CACHE BOOL "" FORCE)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../.." "${CMAKE_BINARY_DIR}/sindre")
endif()

add_executable(sindre_example_ai_tensorrt_segmentation main.cpp)
target_link_libraries(sindre_example_ai_tensorrt_segmentation PRIVATE sindre::ai)
if(WIN32)
    # Copy only the runtime subset needed by this executable. Copying the
    # entire CUDA bin directory would add several gigabytes to a deployment.
    file(TO_CMAKE_PATH "${SINDRE_TENSORRT_ROOT}" _sindre_trt_root)
    file(TO_CMAKE_PATH "${CUDAToolkit_ROOT}" _sindre_cuda_root)
    if(SINDRE_AI_TRT_RUNTIME STREQUAL "DISPATCH")
        set(_sindre_trt_runtime_names
            nvinfer_dispatch_10.dll
            nvinfer_lean_10.dll
            nvinfer_plugin_10.dll)
    else()
        set(_sindre_trt_runtime_names
            nvinfer_10.dll
            nvinfer_lean_10.dll
            nvinfer_plugin_10.dll
            nvonnxparser_10.dll)
    endif()
    set(_sindre_trt_runtime_files)
    foreach(_sindre_runtime_name IN LISTS _sindre_trt_runtime_names)
        foreach(_sindre_runtime_dir IN ITEMS
                "${_sindre_trt_root}/lib"
                "${_sindre_trt_root}/bin")
            if(EXISTS "${_sindre_runtime_dir}/${_sindre_runtime_name}")
                list(APPEND _sindre_trt_runtime_files
                    "${_sindre_runtime_dir}/${_sindre_runtime_name}")
            endif()
        endforeach()
    endforeach()
    file(GLOB _sindre_cuda_runtime_files CONFIGURE_DEPENDS
        "${_sindre_cuda_root}/bin/cudart64_*.dll"
        "${_sindre_cuda_root}/bin/cublas64_*.dll"
        "${_sindre_cuda_root}/bin/cublasLt64_*.dll")
    sindre_copy_runtime_files(sindre_example_ai_tensorrt_segmentation
        ${_sindre_trt_runtime_files} ${_sindre_cuda_runtime_files})
endif()
