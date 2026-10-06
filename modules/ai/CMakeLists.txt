set(_sindrecpp_ai_include "${CMAKE_CURRENT_SOURCE_DIR}/include")
sindrecpp_add_interface_target(Ai "${_sindrecpp_ai_include}")
include("${SINDRECPP_THIRDS_DIR}/ai/Dependencies.cmake")
target_compile_definitions(SindreCpp_Ai INTERFACE SINDRECPP_WITH_AI=1)
target_link_libraries(SindreCpp_Ai INTERFACE SindreCpp::General)

set(sindrecpp_ai_runtime_dirs "")

if(SINDRECPP_AI_ONNXRUNTIME)
    if(NOT TARGET onnxruntime::onnxruntime AND NOT TARGET onnxruntime)
        find_package(onnxruntime CONFIG QUIET)
    endif()
    if(TARGET onnxruntime::onnxruntime)
        set(sindrecpp_ort_target onnxruntime::onnxruntime)
    elseif(TARGET onnxruntime)
        set(sindrecpp_ort_target onnxruntime)
    else()
        find_path(sindrecpp_ort_include onnxruntime_cxx_api.h
            HINTS "${SINDRECPP_ONNXRUNTIME_ROOT}" ENV ONNXRUNTIME_ROOT
            PATH_SUFFIXES include include/onnxruntime/core/session)
        find_library(sindrecpp_ort_library NAMES onnxruntime
            HINTS "${SINDRECPP_ONNXRUNTIME_ROOT}" ENV ONNXRUNTIME_ROOT
            PATH_SUFFIXES lib lib64)
        if(NOT sindrecpp_ort_include OR NOT sindrecpp_ort_library)
            message(FATAL_ERROR "Install ONNX Runtime C/C++ SDK (${SINDRECPP_THIRD_AI_ONNXRUNTIME_MIN_VERSION}+) or set SINDRECPP_ONNXRUNTIME_ROOT")
        endif()
        add_library(sindrecpp_onnxruntime UNKNOWN IMPORTED GLOBAL)
        set_target_properties(sindrecpp_onnxruntime PROPERTIES
            IMPORTED_LOCATION "${sindrecpp_ort_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${sindrecpp_ort_include}")
        set(sindrecpp_ort_target sindrecpp_onnxruntime)
    endif()
    target_link_libraries(SindreCpp_Ai INTERFACE ${sindrecpp_ort_target})
    target_compile_definitions(SindreCpp_Ai INTERFACE SINDRECPP_AI_ONNXRUNTIME=1)
    if(SINDRECPP_AI_CUDA)
        target_compile_definitions(SindreCpp_Ai INTERFACE SINDRECPP_AI_CUDA=1)
        if(WIN32)
            find_file(sindrecpp_cudnn_runtime NAMES cudnn64_9.dll
                HINTS "${SINDRECPP_CUDNN_ROOT}" "C:/Program Files/NVIDIA/CUDNN"
                PATH_SUFFIXES bin bin/12.9)
            if(sindrecpp_cudnn_runtime)
                get_filename_component(sindrecpp_cudnn_runtime_dir
                    "${sindrecpp_cudnn_runtime}" DIRECTORY)
                list(APPEND sindrecpp_ai_runtime_dirs "${sindrecpp_cudnn_runtime_dir}")
            endif()
        endif()
    endif()
    if(SINDRECPP_ONNXRUNTIME_ROOT)
        list(APPEND sindrecpp_ai_runtime_dirs
            "${SINDRECPP_ONNXRUNTIME_ROOT}" "${SINDRECPP_ONNXRUNTIME_ROOT}/lib")
    endif()
    if(WIN32)
        find_file(sindrecpp_ort_runtime NAMES onnxruntime.dll
            HINTS "${SINDRECPP_ONNXRUNTIME_ROOT}" "${SINDRECPP_ONNXRUNTIME_ROOT}/lib"
            PATH_SUFFIXES bin lib)
        if(sindrecpp_ort_runtime)
            get_filename_component(sindrecpp_ort_runtime_dir
                "${sindrecpp_ort_runtime}" DIRECTORY)
            list(APPEND sindrecpp_ai_runtime_dirs "${sindrecpp_ort_runtime_dir}")
        endif()
    endif()
endif()

if(SINDRECPP_AI_TRT)
    find_package(CUDAToolkit ${SINDRECPP_THIRD_AI_CUDA_MIN_VERSION} REQUIRED)
    if(TARGET TensorRT::nvinfer AND TARGET TensorRT::nvinfer_plugin AND TARGET TensorRT::nvonnxparser)
        set(sindrecpp_trt_targets TensorRT::nvinfer TensorRT::nvinfer_plugin TensorRT::nvonnxparser)
    else()
        find_path(sindrecpp_trt_include NvInfer.h
            HINTS "${SINDRECPP_TENSORRT_ROOT}" ENV TENSORRT_ROOT PATH_SUFFIXES include)
        if(NOT sindrecpp_trt_include)
            message(FATAL_ERROR "Set SINDRECPP_TENSORRT_ROOT to TensorRT ${SINDRECPP_THIRD_AI_TENSORRT_MIN_VERSION} or newer")
        endif()
        foreach(library IN ITEMS nvinfer nvinfer_plugin nvonnxparser)
            find_library(sindrecpp_${library}_library NAMES ${library} ${library}_10
                HINTS "${SINDRECPP_TENSORRT_ROOT}" ENV TENSORRT_ROOT PATH_SUFFIXES lib lib64)
            if(NOT sindrecpp_${library}_library)
                message(FATAL_ERROR "TensorRT SDK missing ${library}")
            endif()
            add_library(sindrecpp_${library} UNKNOWN IMPORTED GLOBAL)
            set_target_properties(sindrecpp_${library} PROPERTIES
                IMPORTED_LOCATION "${sindrecpp_${library}_library}"
                INTERFACE_INCLUDE_DIRECTORIES "${sindrecpp_trt_include}")
            list(APPEND sindrecpp_trt_targets sindrecpp_${library})
        endforeach()
    endif()
    target_link_libraries(SindreCpp_Ai INTERFACE ${sindrecpp_trt_targets} CUDA::cudart)
    target_compile_definitions(SindreCpp_Ai INTERFACE SINDRECPP_AI_TRT=1)
    list(APPEND sindrecpp_ai_runtime_dirs
        "${SINDRECPP_TENSORRT_ROOT}/lib" "${CUDAToolkit_BIN_DIR}")
endif()

if(WIN32 AND sindrecpp_ai_runtime_dirs)
    list(REMOVE_DUPLICATES sindrecpp_ai_runtime_dirs)
    set(sindrecpp_ai_path_modifications)
    foreach(runtime_dir IN LISTS sindrecpp_ai_runtime_dirs)
        list(APPEND sindrecpp_ai_path_modifications
            "PATH=path_list_prepend:${runtime_dir}")
    endforeach()
    set_property(TARGET SindreCpp_Ai PROPERTY SINDRECPP_RUNTIME_DIRS
        "${sindrecpp_ai_runtime_dirs}")
endif()

if(SINDRECPP_BUILD_TESTS)
    add_executable(sindrecpp_ai_execution_tests tests/execution_tests.cpp)
    target_link_libraries(sindrecpp_ai_execution_tests PRIVATE SindreCpp::Ai)
    sindrecpp_copy_target_runtime_dlls(sindrecpp_ai_execution_tests)
    add_test(NAME sindrecpp.ai.execution COMMAND sindrecpp_ai_execution_tests)

    if(SINDRECPP_AI_ONNXRUNTIME)
        set(sindrecpp_ai_fixture_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/models")
        foreach(model IN ITEMS identity.onnx identity_int32.onnx identity_float16.onnx
                              identity_int64.onnx identity_bool.onnx)
            if(NOT EXISTS "${sindrecpp_ai_fixture_dir}/${model}")
                message(FATAL_ERROR "Missing AI test fixture: ${sindrecpp_ai_fixture_dir}/${model}")
            endif()
        endforeach()
        add_executable(sindrecpp_ai_tests tests/ai_tests.cpp)
        target_link_libraries(sindrecpp_ai_tests PRIVATE SindreCpp::Ai)
        sindrecpp_copy_target_runtime_dlls(sindrecpp_ai_tests)
        if(WIN32 AND sindrecpp_ai_runtime_dirs)
            sindrecpp_copy_runtime_dirs(sindrecpp_ai_tests ${sindrecpp_ai_runtime_dirs})
        endif()
        add_test(NAME sindrecpp.ai.onnxruntime COMMAND sindrecpp_ai_tests
            "${sindrecpp_ai_fixture_dir}/identity.onnx")
        add_executable(sindrecpp_ai_typed_tests tests/typed_ai_tests.cpp)
        target_link_libraries(sindrecpp_ai_typed_tests PRIVATE SindreCpp::Ai)
        sindrecpp_copy_target_runtime_dlls(sindrecpp_ai_typed_tests)
        if(WIN32 AND sindrecpp_ai_runtime_dirs)
            sindrecpp_copy_runtime_dirs(sindrecpp_ai_typed_tests ${sindrecpp_ai_runtime_dirs})
        endif()
        if(WIN32 AND sindrecpp_ort_runtime)
            foreach(test_target IN ITEMS sindrecpp_ai_tests sindrecpp_ai_typed_tests)
                add_custom_command(TARGET ${test_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${sindrecpp_ort_runtime}" "$<TARGET_FILE_DIR:${test_target}>")
            endforeach()
        endif()
        foreach(kind IN ITEMS int32 float16 int64 bool)
            add_test(NAME sindrecpp.ai.typed.${kind} COMMAND sindrecpp_ai_typed_tests
                "${sindrecpp_ai_fixture_dir}/identity_${kind}.onnx" ${kind})
        endforeach()
        if(WIN32 AND sindrecpp_ai_path_modifications)
            set_tests_properties(sindrecpp.ai.onnxruntime
                sindrecpp.ai.typed.int32 sindrecpp.ai.typed.float16
                sindrecpp.ai.typed.int64 sindrecpp.ai.typed.bool PROPERTIES
                ENVIRONMENT_MODIFICATION
                "${sindrecpp_ai_path_modifications}")
        endif()
    endif()

    if(SINDRECPP_AI_TRT AND SINDRECPP_BUILD_GPU_TESTS)
        add_executable(sindrecpp_ai_trt_tests tests/trt_tests.cpp)
        target_link_libraries(sindrecpp_ai_trt_tests PRIVATE SindreCpp::Ai)
        sindrecpp_copy_target_runtime_dlls(sindrecpp_ai_trt_tests)
        add_test(NAME sindrecpp.ai.tensorrt COMMAND sindrecpp_ai_trt_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/models/identity.onnx")
        set_tests_properties(sindrecpp.ai.tensorrt PROPERTIES TIMEOUT 120)
    endif()
endif()

install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
