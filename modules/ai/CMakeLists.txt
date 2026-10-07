sindre_add_static_target(ai "${SINDRE_SOURCE_DIR}/include"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/ai.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/src/execution.cpp")
if(SINDRE_AI_ONNXRUNTIME)
    target_sources(sindre_ai PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/onnxruntime.cpp")
endif()
if(SINDRE_AI_TRT)
    target_sources(sindre_ai PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/src/trt.cpp")
endif()
include("${SINDRE_THIRDS_DIR}/ai/Dependencies.cmake")
target_compile_definitions(sindre_ai PUBLIC SINDRE_WITH_AI=1)
target_link_libraries(sindre_ai PUBLIC sindre::general)

set(sindre_ai_runtime_dirs "")
set(SINDRE_AI_ONNXRUNTIME_BUNDLED OFF)
set(SINDRE_AI_TRT_ENABLED OFF)

if(SINDRE_AI_ONNXRUNTIME)
    if(NOT TARGET onnxruntime::onnxruntime AND NOT TARGET onnxruntime)
        find_package(onnxruntime CONFIG QUIET)
    endif()
    if(TARGET onnxruntime::onnxruntime)
        set(sindre_ort_target onnxruntime::onnxruntime)
    elseif(TARGET onnxruntime)
        set(sindre_ort_target onnxruntime)
    else()
        find_path(sindre_ort_include onnxruntime_cxx_api.h
            HINTS "${SINDRE_ONNXRUNTIME_ROOT}" ENV ONNXRUNTIME_ROOT
            PATH_SUFFIXES include include/onnxruntime/core/session)
        find_library(sindre_ort_library NAMES onnxruntime
            HINTS "${SINDRE_ONNXRUNTIME_ROOT}" ENV ONNXRUNTIME_ROOT
            PATH_SUFFIXES lib lib64)
        if(NOT sindre_ort_include OR NOT sindre_ort_library)
            message(FATAL_ERROR "Install ONNX Runtime C/C++ SDK (${SINDRE_THIRD_AI_ONNXRUNTIME_MIN_VERSION}+) or set SINDRE_ONNXRUNTIME_ROOT")
        endif()
        add_library(sindre_onnxruntime UNKNOWN IMPORTED GLOBAL)
        set_target_properties(sindre_onnxruntime PROPERTIES
            IMPORTED_LOCATION "${sindre_ort_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${sindre_ort_include}")
        set(sindre_ort_target sindre_onnxruntime)
    endif()
    # Keep SDK targets behind exported Sindre wrappers. Raw imported targets
    # created from a developer machine must not leak into installed exports.
    add_library(sindre_ai_onnxruntime INTERFACE)
    set_target_properties(sindre_ai_onnxruntime PROPERTIES EXPORT_NAME ai_onnxruntime)
    target_link_libraries(sindre_ai_onnxruntime INTERFACE
        $<BUILD_INTERFACE:${sindre_ort_target}>
        $<INSTALL_INTERFACE:sindre::ai_onnxruntime_native>)
    target_link_libraries(sindre_ai PUBLIC sindre_ai_onnxruntime)
    install(TARGETS sindre_ai_onnxruntime EXPORT sindrecppTargets)
    target_compile_definitions(sindre_ai PUBLIC SINDRE_AI_ONNXRUNTIME=1)
    if(SINDRE_AI_CUDA)
        target_compile_definitions(sindre_ai PUBLIC SINDRE_AI_CUDA=1)
        if(WIN32)
            find_file(sindre_cudnn_runtime NAMES cudnn64_9.dll
                HINTS "${SINDRE_CUDNN_ROOT}" "C:/Program Files/NVIDIA/CUDNN"
                PATH_SUFFIXES bin bin/12.9)
            if(sindre_cudnn_runtime)
                get_filename_component(sindre_cudnn_runtime_dir
                    "${sindre_cudnn_runtime}" DIRECTORY)
                list(APPEND sindre_ai_runtime_dirs "${sindre_cudnn_runtime_dir}")
            endif()
        endif()
    endif()
    if(SINDRE_ONNXRUNTIME_ROOT)
        list(APPEND sindre_ai_runtime_dirs
            "${SINDRE_ONNXRUNTIME_ROOT}" "${SINDRE_ONNXRUNTIME_ROOT}/lib")
    endif()
    if(WIN32)
        find_file(sindre_ort_runtime NAMES onnxruntime.dll
            HINTS "${SINDRE_ONNXRUNTIME_ROOT}" "${SINDRE_ONNXRUNTIME_ROOT}/lib"
            PATH_SUFFIXES bin lib)
        if(sindre_ort_runtime)
            get_filename_component(sindre_ort_runtime_dir
                "${sindre_ort_runtime}" DIRECTORY)
            list(APPEND sindre_ai_runtime_dirs "${sindre_ort_runtime_dir}")
        endif()
    endif()
    if(TARGET sindre_onnxruntime)
        get_filename_component(_sindre_bundled_ort_root
            "${SINDRE_THIRDS_DIR}/ai/onnxruntime/1.22.0/onnxruntime-win-x64-1.22.0"
            REALPATH)
        get_filename_component(_sindre_selected_ort_root
            "${SINDRE_ONNXRUNTIME_ROOT}" REALPATH)
        if(_sindre_selected_ort_root STREQUAL _sindre_bundled_ort_root)
            set(SINDRE_AI_ONNXRUNTIME_BUNDLED ON)
            if(NOT sindre_ort_runtime)
                message(FATAL_ERROR "Bundled ONNX Runtime is missing its runtime DLL")
            endif()
            install(DIRECTORY "${sindre_ort_include}/"
                DESTINATION "${CMAKE_INSTALL_INCLUDEDIR}/sindre/third_party/onnxruntime")
            install(FILES "${sindre_ort_library}"
                DESTINATION "${CMAKE_INSTALL_LIBDIR}/sindre/onnxruntime")
            install(FILES "${sindre_ort_runtime}"
                DESTINATION "${CMAKE_INSTALL_BINDIR}")
            install(FILES
                "${SINDRE_ONNXRUNTIME_ROOT}/LICENSE"
                "${SINDRE_ONNXRUNTIME_ROOT}/ThirdPartyNotices.txt"
                DESTINATION "${CMAKE_INSTALL_DATADIR}/sindrecpp/licenses/onnxruntime"
                OPTIONAL)
        endif()
    endif()
endif()

if(SINDRE_AI_TRT)
    set(SINDRE_AI_TRT_ENABLED ON)
    string(TOUPPER "${SINDRE_AI_TRT_RUNTIME}" sindre_ai_trt_runtime)
    if(NOT sindre_ai_trt_runtime STREQUAL "FULL" AND
       NOT sindre_ai_trt_runtime STREQUAL "DISPATCH")
        message(FATAL_ERROR
            "SINDRE_AI_TRT_RUNTIME must be FULL or DISPATCH")
    endif()
    find_package(CUDAToolkit ${SINDRE_THIRD_AI_CUDA_MIN_VERSION} REQUIRED)
    if(sindre_ai_trt_runtime STREQUAL "DISPATCH")
        set(sindre_trt_runtime_name nvinfer_dispatch)
    else()
        set(sindre_trt_runtime_name nvinfer)
    endif()
    if(sindre_ai_trt_runtime STREQUAL "DISPATCH" AND
       TARGET TensorRT::nvinfer_dispatch AND TARGET TensorRT::nvinfer_plugin)
        set(sindre_trt_targets TensorRT::nvinfer_dispatch TensorRT::nvinfer_plugin)
    elseif(sindre_ai_trt_runtime STREQUAL "FULL" AND
           TARGET TensorRT::nvinfer AND TARGET TensorRT::nvinfer_plugin AND
           TARGET TensorRT::nvonnxparser)
        set(sindre_trt_targets TensorRT::nvinfer TensorRT::nvinfer_plugin TensorRT::nvonnxparser)
    else()
        find_path(sindre_trt_include NvInfer.h
            HINTS "${SINDRE_TENSORRT_ROOT}" ENV TENSORRT_ROOT PATH_SUFFIXES include)
        if(NOT sindre_trt_include)
            message(FATAL_ERROR "Set SINDRE_TENSORRT_ROOT to TensorRT ${SINDRE_THIRD_AI_TENSORRT_MIN_VERSION} or newer")
        endif()
        set(sindre_trt_libraries ${sindre_trt_runtime_name} nvinfer_plugin)
        if(sindre_ai_trt_runtime STREQUAL "FULL")
            list(APPEND sindre_trt_libraries nvonnxparser)
        endif()
        foreach(library IN LISTS sindre_trt_libraries)
            find_library(sindre_${library}_library NAMES ${library} ${library}_10
                HINTS "${SINDRE_TENSORRT_ROOT}" ENV TENSORRT_ROOT PATH_SUFFIXES lib lib64)
            if(NOT sindre_${library}_library)
                message(FATAL_ERROR "TensorRT SDK missing ${library}")
            endif()
            add_library(sindre_${library} UNKNOWN IMPORTED GLOBAL)
            set_target_properties(sindre_${library} PROPERTIES
                IMPORTED_LOCATION "${sindre_${library}_library}"
                INTERFACE_INCLUDE_DIRECTORIES "${sindre_trt_include}")
            list(APPEND sindre_trt_targets sindre_${library})
        endforeach()
    endif()
    add_library(sindre_ai_tensorrt INTERFACE)
    set_target_properties(sindre_ai_tensorrt PROPERTIES EXPORT_NAME ai_tensorrt)
    target_link_libraries(sindre_ai_tensorrt INTERFACE
        $<BUILD_INTERFACE:${sindre_trt_targets};CUDA::cudart>
        $<INSTALL_INTERFACE:sindre::ai_tensorrt_native>)
    target_link_libraries(sindre_ai PUBLIC sindre_ai_tensorrt)
    install(TARGETS sindre_ai_tensorrt EXPORT sindrecppTargets)
    target_compile_definitions(sindre_ai PUBLIC SINDRE_AI_TRT=1)
    if(sindre_ai_trt_runtime STREQUAL "DISPATCH")
        target_compile_definitions(sindre_ai PRIVATE SINDRE_AI_TRT_DISPATCH=1)
    endif()
    list(APPEND sindre_ai_runtime_dirs
        "${SINDRE_TENSORRT_ROOT}/lib" "${CUDAToolkit_BIN_DIR}")
endif()

set(SINDRE_AI_ONNXRUNTIME_BUNDLED "${SINDRE_AI_ONNXRUNTIME_BUNDLED}" PARENT_SCOPE)
set(SINDRE_AI_TRT_ENABLED "${SINDRE_AI_TRT_ENABLED}" PARENT_SCOPE)
set(SINDRE_AI_TRT_RUNTIME_DIRS "${sindre_ai_runtime_dirs}" PARENT_SCOPE)

if(WIN32 AND sindre_ai_runtime_dirs)
    list(REMOVE_DUPLICATES sindre_ai_runtime_dirs)
    set(sindre_ai_path_modifications)
    foreach(runtime_dir IN LISTS sindre_ai_runtime_dirs)
        list(APPEND sindre_ai_path_modifications
            "PATH=path_list_prepend:${runtime_dir}")
    endforeach()
    set_property(TARGET sindre_ai PROPERTY SINDRE_RUNTIME_DIRS
        "${sindre_ai_runtime_dirs}")
endif()

if(SINDRE_BUILD_TESTS)
    add_executable(sindre_ai_execution_tests tests/execution_tests.cpp)
    target_link_libraries(sindre_ai_execution_tests PRIVATE sindre::ai)
    sindre_copy_target_runtime_dlls(sindre_ai_execution_tests)
    add_test(NAME sindre.ai.execution COMMAND sindre_ai_execution_tests)

    if(SINDRE_AI_ONNXRUNTIME)
        set(sindre_ai_fixture_dir "${CMAKE_CURRENT_SOURCE_DIR}/tests/models")
        foreach(model IN ITEMS identity.onnx identity_int32.onnx identity_float16.onnx
                              identity_int64.onnx identity_bool.onnx)
            if(NOT EXISTS "${sindre_ai_fixture_dir}/${model}")
                message(FATAL_ERROR "Missing AI test fixture: ${sindre_ai_fixture_dir}/${model}")
            endif()
        endforeach()
        add_executable(sindre_ai_tests tests/ai_tests.cpp)
        target_link_libraries(sindre_ai_tests PRIVATE sindre::ai)
        sindre_copy_target_runtime_dlls(sindre_ai_tests)
        if(WIN32 AND sindre_ai_runtime_dirs)
            sindre_copy_runtime_dirs(sindre_ai_tests ${sindre_ai_runtime_dirs})
        endif()
        add_test(NAME sindre.ai.onnxruntime COMMAND sindre_ai_tests
            "${sindre_ai_fixture_dir}/identity.onnx")
        add_executable(sindre_ai_typed_tests tests/typed_ai_tests.cpp)
        target_link_libraries(sindre_ai_typed_tests PRIVATE sindre::ai)
        sindre_copy_target_runtime_dlls(sindre_ai_typed_tests)
        if(WIN32 AND sindre_ai_runtime_dirs)
            sindre_copy_runtime_dirs(sindre_ai_typed_tests ${sindre_ai_runtime_dirs})
        endif()
        if(WIN32 AND sindre_ort_runtime)
            foreach(test_target IN ITEMS sindre_ai_tests sindre_ai_typed_tests)
                add_custom_command(TARGET ${test_target} POST_BUILD
                    COMMAND ${CMAKE_COMMAND} -E copy_if_different
                        "${sindre_ort_runtime}" "$<TARGET_FILE_DIR:${test_target}>")
            endforeach()
        endif()
        foreach(kind IN ITEMS int32 float16 int64 bool)
            add_test(NAME sindre.ai.typed.${kind} COMMAND sindre_ai_typed_tests
                "${sindre_ai_fixture_dir}/identity_${kind}.onnx" ${kind})
        endforeach()
        if(WIN32 AND sindre_ai_path_modifications)
            set_tests_properties(sindre.ai.onnxruntime
                sindre.ai.typed.int32 sindre.ai.typed.float16
                sindre.ai.typed.int64 sindre.ai.typed.bool PROPERTIES
                ENVIRONMENT_MODIFICATION
                "${sindre_ai_path_modifications}")
        endif()
    endif()

    if(SINDRE_AI_TRT AND SINDRE_BUILD_GPU_TESTS)
        add_executable(sindre_ai_trt_tests tests/trt_tests.cpp)
        target_link_libraries(sindre_ai_trt_tests PRIVATE sindre::ai)
        sindre_copy_target_runtime_dlls(sindre_ai_trt_tests)
        add_test(NAME sindre.ai.tensorrt COMMAND sindre_ai_trt_tests
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/models/identity.onnx")
        set_tests_properties(sindre.ai.tensorrt PROPERTIES TIMEOUT 120)
    endif()
endif()
