cmake_minimum_required(VERSION 3.20)

project(SindreCpp VERSION 0.1.0 LANGUAGES CXX)

include(FetchContent)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    set(sindrecpp_build_extras_default ON)
else()
    set(sindrecpp_build_extras_default OFF)
endif()
option(SINDRECPP_BUILD_TESTS "Build SindreCpp tests" ${sindrecpp_build_extras_default})
option(SINDRECPP_BUILD_EXAMPLES "Build SindreCpp examples" ${sindrecpp_build_extras_default})

option(SINDRECPP_WITH_UTILS2D "Enable OpenCV image utilities" OFF)
option(SINDRECPP_WITH_AI "Enable ONNX Runtime inference" OFF)
option(SINDRECPP_AI_CUDA "Compile CUDA backend support (requires GPU ONNX Runtime at runtime)" ON)
option(SINDRECPP_AI_ONNXRUNTIME "Enable ONNX Runtime backend inside AI" ON)
option(SINDRECPP_AI_TRT "Enable independent TensorRT backend inside AI" OFF)
option(SINDRECPP_BUILD_GPU_TESTS "Run TensorRT conversion/inference tests on a real GPU" OFF)
set(SINDRECPP_TENSORRT_ROOT "" CACHE PATH "TensorRT 10.13.x SDK root")
set(SINDRECPP_ONNXRUNTIME_ROOT "" CACHE PATH "ONNX Runtime C/C++ SDK root")

option(SINDRECPP_WITH_GENERAL "Enable general-purpose utilities" ON)
option(SINDRECPP_WITH_POINTER "Enable the CsPointer integration" OFF)
option(SINDRECPP_WITH_STRING "Enable the CsString integration" OFF)
option(SINDRECPP_WITH_LOG "Enable the spdlog integration" OFF)
option(SINDRECPP_WITH_UTILS_GUI "Enable the Dear ImGui integration" OFF)
option(SINDRECPP_WITH_UTILS_PY "Enable the pybind11 integration" OFF)
option(SINDRECPP_WITH_HTTP "Enable the cpp-httplib integration" OFF)
option(SINDRECPP_HTTP_OPENSSL "Enable HTTPS support in cpp-httplib when OpenSSL is available" ON)
option(SINDRECPP_WITH_JSON "Enable the simdjson integration" OFF)
option(SINDRECPP_WITH_CLI "Enable the argparse integration" OFF)
option(SINDRECPP_WITH_UTILS3D "Enable VTK SindreMesh and geometry algorithms" OFF)
option(SINDRECPP_UTILS3D_SHOW "Enable standalone show_mesh rendering (requires an OpenGL context)" OFF)
option(SINDRECPP_UTILS3D_VTK_DATA "Enable VTK datasets, image processing and scientific visualization" OFF)
foreach(backend IN ITEMS MESHLIB CGAL OPEN3D IGL VCG)
    option(SINDRECPP_UTILS3D_${backend} "Enable optional ${backend} geometry algorithms" OFF)
endforeach()
set(SINDRECPP_VCG_ROOT "" CACHE PATH "VCGlib source root containing vcg/complex/complex.h")
set(SINDRECPP_IGL_ROOT "" CACHE PATH "libigl source root containing include/igl")
option(SINDRECPP_UTILS3D_NATIVE_ARCH "Optimize utils3d for the build machine CPU (reduces binary portability)" ON)
set(SINDRECPP_UTILS3D_BLAS_BACKEND "AUTO" CACHE STRING "utils3d BLAS backend: AUTO, EIGEN, or BLAS")
set_property(CACHE SINDRECPP_UTILS3D_BLAS_BACKEND PROPERTY STRINGS AUTO EIGEN BLAS)
option(SINDRECPP_BUILD_UTILS3D_BENCHMARKS "Build the optional utils3d vs BLAS benchmark" OFF)

if(NOT SINDRECPP_WITH_GENERAL)
    foreach(module IN ITEMS POINTER STRING LOG HTTP JSON CLI)
        if(SINDRECPP_WITH_${module})
            message(FATAL_ERROR "${module} requires SINDRECPP_WITH_GENERAL=ON")
        endif()
    endforeach()
endif()

# CsPointer and CsString have broken top-level CMake configure_file paths; only their sources are populated.
if(POLICY CMP0169)
    cmake_policy(SET CMP0169 OLD)
endif()

add_library(sindrecpp_base INTERFACE)
set_target_properties(sindrecpp_base PROPERTIES
    INTERFACE_COMPILE_FEATURES cxx_std_17
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
)

function(sindrecpp_add_module module)
    add_library(SindreCpp::${module} INTERFACE IMPORTED GLOBAL)
    string(TOUPPER "${module}" module_upper)
    set_target_properties(SindreCpp::${module} PROPERTIES
        INTERFACE_LINK_LIBRARIES "sindrecpp_base;${ARGN}"
        INTERFACE_COMPILE_DEFINITIONS SINDRECPP_WITH_${module_upper}=1
    )
endfunction()

# Apply upstream defaults only within the dependency's subdirectory scope.
# Existing parent-project variables and cache options always take precedence.
function(sindrecpp_make_available dependency)
    foreach(default IN LISTS ARGN)
        string(FIND "${default}" "=" separator)
        if(separator LESS 1)
            message(FATAL_ERROR "Invalid dependency default '${default}'; expected NAME=VALUE")
        endif()
        string(SUBSTRING "${default}" 0 ${separator} variable)
        math(EXPR value_start "${separator} + 1")
        string(SUBSTRING "${default}" ${value_start} -1 value)
        if(NOT DEFINED ${variable})
            set(${variable} "${value}")
        endif()
    endforeach()
    FetchContent_MakeAvailable(${dependency})
endfunction()

if(SINDRECPP_WITH_POINTER)
    FetchContent_Declare(cppointer
        GIT_REPOSITORY https://github.com/copperspice/cs_pointer.git
        GIT_TAG pointer-1.0.1
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(cppointer)
    if(NOT cppointer_POPULATED)
        FetchContent_Populate(cppointer)
    endif()
    add_library(sindrecpp_cs_pointer INTERFACE)
    target_compile_features(sindrecpp_cs_pointer INTERFACE cxx_std_17)
    target_include_directories(sindrecpp_cs_pointer SYSTEM INTERFACE ${cppointer_SOURCE_DIR}/src)
    sindrecpp_add_module(Pointer sindrecpp_cs_pointer)
endif()

if(SINDRECPP_WITH_STRING)
    FetchContent_Declare(csstring
        GIT_REPOSITORY https://github.com/copperspice/cs_string.git
        GIT_TAG string-1.4.1
        GIT_SHALLOW TRUE
    )
    FetchContent_GetProperties(csstring)
    if(NOT csstring_POPULATED)
        FetchContent_Populate(csstring)
    endif()
    add_library(sindrecpp_cs_string STATIC ${csstring_SOURCE_DIR}/src/cs_string.cpp)
    target_compile_features(sindrecpp_cs_string PUBLIC cxx_std_17)
    target_include_directories(sindrecpp_cs_string SYSTEM PUBLIC ${csstring_SOURCE_DIR}/src)
    sindrecpp_add_module(String sindrecpp_cs_string)
endif()

if(SINDRECPP_WITH_LOG)
    if(NOT TARGET spdlog::spdlog)
        find_package(spdlog CONFIG QUIET)
    endif()
    if(NOT TARGET spdlog::spdlog)
        FetchContent_Declare(spdlog
            GIT_REPOSITORY https://github.com/gabime/spdlog.git
            GIT_TAG v1.17.0
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(spdlog
            SPDLOG_BUILD_TESTS=OFF
            SPDLOG_BUILD_EXAMPLE=OFF
        )
    endif()
    sindrecpp_add_module(Log spdlog::spdlog)
endif()

if(SINDRECPP_WITH_UTILS_GUI)
    if(NOT TARGET imgui::imgui AND NOT TARGET imgui)
        find_package(imgui CONFIG QUIET)
    endif()
    if(TARGET imgui::imgui)
        sindrecpp_add_module(Utils_gui imgui::imgui)
    elseif(TARGET imgui)
        sindrecpp_add_module(Utils_gui imgui)
    else()
        FetchContent_Declare(imgui
            GIT_REPOSITORY https://github.com/ocornut/imgui.git
            GIT_TAG v1.92.9b
            GIT_SHALLOW TRUE
        )
        FetchContent_GetProperties(imgui)
        if(NOT imgui_POPULATED)
            FetchContent_Populate(imgui)
        endif()
        add_library(sindrecpp_imgui STATIC
            ${imgui_SOURCE_DIR}/imgui.cpp
            ${imgui_SOURCE_DIR}/imgui_draw.cpp
            ${imgui_SOURCE_DIR}/imgui_tables.cpp
            ${imgui_SOURCE_DIR}/imgui_widgets.cpp
        )
        target_include_directories(sindrecpp_imgui SYSTEM PUBLIC ${imgui_SOURCE_DIR})
        target_compile_features(sindrecpp_imgui PUBLIC cxx_std_17)
        sindrecpp_add_module(Utils_gui sindrecpp_imgui)
    endif()
endif()

if(SINDRECPP_WITH_UTILS_PY)
    find_package(Python3 REQUIRED COMPONENTS Interpreter Development)
    if(NOT TARGET pybind11::embed)
        find_package(pybind11 CONFIG QUIET)
    endif()
    if(NOT TARGET pybind11::embed)
        FetchContent_Declare(pybind11
            GIT_REPOSITORY https://github.com/pybind/pybind11.git
            GIT_TAG v3.1.0
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(pybind11 PYBIND11_TEST=OFF)
    endif()
    sindrecpp_add_module(Utils_py pybind11::embed)
    if(SINDRECPP_WITH_UTILS3D AND SINDRECPP_UTILS3D_MESHLIB)
        # MeshLib's dependency include root contains a patched pybind11. An
        # ordinary /usr/include entry cannot outrank that root, so stage ONLY
        # the independently selected pybind11 headers in a dedicated path.
        get_target_property(sindrecpp_pybind_paths pybind11::headers INTERFACE_INCLUDE_DIRECTORIES)
        find_path(sindrecpp_pybind_include pybind11/embed.h
            HINTS ${pybind11_INCLUDE_DIRS} ${sindrecpp_pybind_paths} ${pybind11_SOURCE_DIR}/include
            REQUIRED)
        set(sindrecpp_pybind_stage "${CMAKE_CURRENT_BINARY_DIR}/sindrecpp-python-headers")
        file(COPY "${sindrecpp_pybind_include}/pybind11" DESTINATION "${sindrecpp_pybind_stage}")
        # Imported target include directories become system paths. Use a
        # normal compiler include path here so the isolated headers win.
        set_property(TARGET SindreCpp::Utils_py APPEND PROPERTY INTERFACE_COMPILE_OPTIONS
            "$<$<CXX_COMPILER_ID:MSVC>:/I${sindrecpp_pybind_stage}>"
            "$<$<NOT:$<CXX_COMPILER_ID:MSVC>>:-I${sindrecpp_pybind_stage}>")
    endif()
endif()

if(SINDRECPP_WITH_HTTP)
    if(NOT TARGET httplib::httplib)
        find_package(httplib CONFIG QUIET)
    endif()
    if(NOT TARGET httplib::httplib)
        FetchContent_Declare(cpp_httplib
            GIT_REPOSITORY https://github.com/yhirose/cpp-httplib.git
            GIT_TAG v0.56.0
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(cpp_httplib
            HTTPLIB_TEST=OFF
            HTTPLIB_USE_OPENSSL_IF_AVAILABLE=${SINDRECPP_HTTP_OPENSSL}
        )
    endif()
    sindrecpp_add_module(Http httplib::httplib)
endif()

if(SINDRECPP_WITH_JSON)
    if(NOT TARGET simdjson::simdjson)
        find_package(simdjson CONFIG QUIET)
    endif()
    if(NOT TARGET simdjson::simdjson)
        FetchContent_Declare(simdjson
            GIT_REPOSITORY https://github.com/simdjson/simdjson.git
            GIT_TAG v4.6.11
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(simdjson SIMDJSON_DEVELOPER_MODE=OFF)
    endif()
    sindrecpp_add_module(Json simdjson::simdjson)
endif()

if(SINDRECPP_WITH_CLI)
    if(NOT TARGET argparse::argparse)
        find_package(argparse CONFIG QUIET)
    endif()
    if(NOT TARGET argparse::argparse)
        FetchContent_Declare(argparse
            GIT_REPOSITORY https://github.com/p-ranav/argparse.git
            GIT_TAG v3.2
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(argparse ARGPARSE_BUILD_TESTS=OFF)
    endif()
    sindrecpp_add_module(Cli argparse::argparse)
endif()

if(SINDRECPP_WITH_UTILS3D)
    set(sindrecpp_vtk_components CommonCore CommonDataModel CommonTransforms
        FiltersCore FiltersGeneral FiltersGeometry FiltersModeling IOGeometry IOPLY IOXML)
    if(SINDRECPP_UTILS3D_SHOW OR SINDRECPP_UTILS3D_VTK_DATA)
        enable_language(C) # Some installed VTK targets export MPI::MPI_C.
    endif()
    if(SINDRECPP_UTILS3D_SHOW)
        list(APPEND sindrecpp_vtk_components RenderingCore RenderingOpenGL2 RenderingFreeType
            RenderingAnnotation InteractionStyle IOImage)
    endif()
    if(SINDRECPP_UTILS3D_VTK_DATA)
        list(APPEND sindrecpp_vtk_components FiltersFlowPaths FiltersSources ImagingCore ImagingGeneral
            ImagingMath ImagingMorphological)
        if(SINDRECPP_UTILS3D_SHOW)
            list(APPEND sindrecpp_vtk_components RenderingVolumeOpenGL2 RenderingContextOpenGL2
                RenderingContext2D ChartsCore ViewsContext2D)
        endif()
    endif()
    find_package(VTK 9 REQUIRED COMPONENTS ${sindrecpp_vtk_components})
    if(NOT TARGET Eigen3::Eigen)
        find_package(Eigen3 CONFIG QUIET NO_MODULE)
    endif()
    if(NOT TARGET Eigen3::Eigen)
        FetchContent_Declare(eigen
            GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
            GIT_TAG 5.0.1
            GIT_SHALLOW TRUE
        )
        sindrecpp_make_available(eigen
            EIGEN_BUILD_DOC=OFF
            EIGEN_BUILD_PKGCONFIG=OFF
        )
    endif()

    string(TOUPPER "${SINDRECPP_UTILS3D_BLAS_BACKEND}" sindrecpp_utils3d_blas_backend)
    if(NOT sindrecpp_utils3d_blas_backend MATCHES "^(AUTO|EIGEN|BLAS)$")
        message(FATAL_ERROR "SINDRECPP_UTILS3D_BLAS_BACKEND must be AUTO, EIGEN, or BLAS")
    endif()
    if(NOT TARGET BLAS::BLAS)
        if(sindrecpp_utils3d_blas_backend STREQUAL "BLAS")
            find_package(BLAS REQUIRED)
        else()
            find_package(BLAS QUIET)
        endif()
    endif()

    set(sindrecpp_utils3d_use_blas OFF)
    if(sindrecpp_utils3d_blas_backend STREQUAL "BLAS")
        if(NOT TARGET BLAS::BLAS)
            message(FATAL_ERROR "BLAS backend requested, but no BLAS::BLAS target is available")
        endif()
        set(sindrecpp_utils3d_use_blas ON)
    elseif(sindrecpp_utils3d_blas_backend STREQUAL "AUTO" AND TARGET BLAS::BLAS)
        set(sindrecpp_utils3d_use_blas ON)
    endif()

    set(sindrecpp_utils3d_links Eigen3::Eigen ${VTK_LIBRARIES})
    if(sindrecpp_utils3d_use_blas)
        list(APPEND sindrecpp_utils3d_links BLAS::BLAS)
        message(STATUS "SindreCpp Eigen backend: external BLAS (BLA_VENDOR='${BLA_VENDOR}')")
    else()
        message(STATUS "SindreCpp Eigen backend: Eigen built-in kernels")
    endif()
    if(SINDRECPP_UTILS3D_NATIVE_ARCH)
        add_library(sindrecpp_utils3d_native_arch INTERFACE)
        if(MSVC AND (CMAKE_GENERATOR_PLATFORM MATCHES "^(x64|Win32)$" OR CMAKE_SYSTEM_PROCESSOR MATCHES "^(AMD64|amd64|x86_64|x64|i.86)$" OR CMAKE_CXX_COMPILER_ARCHITECTURE_ID MATCHES "^(x64|X86|Win32)$" OR MSVC_CXX_ARCHITECTURE_ID MATCHES "^(x64|X86|Win32)$"))
            target_compile_options(sindrecpp_utils3d_native_arch INTERFACE /arch:AVX2)
        elseif(MSVC)
            message(STATUS "MSVC target architecture has no SindreCpp native Eigen flag; using compiler defaults")
        elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang|AppleClang")
            target_compile_options(sindrecpp_utils3d_native_arch INTERFACE -march=native)
        else()
            message(FATAL_ERROR "SINDRECPP_UTILS3D_NATIVE_ARCH is unsupported by this compiler")
        endif()
        list(APPEND sindrecpp_utils3d_links sindrecpp_utils3d_native_arch)
        sindrecpp_add_module(Utils3d ${sindrecpp_utils3d_links})
    else()
        sindrecpp_add_module(Utils3d ${sindrecpp_utils3d_links})
    endif()
    if(sindrecpp_utils3d_use_blas)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS EIGEN_USE_BLAS=1)
    endif()
    if(SINDRECPP_UTILS3D_SHOW)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS SINDRECPP_UTILS3D_SHOW=1)
    endif()
    if(SINDRECPP_UTILS3D_VTK_DATA)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS SINDRECPP_UTILS3D_VTK_DATA=1)
    endif()
    if(SINDRECPP_UTILS3D_CGAL)
        find_package(CGAL REQUIRED)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_LINK_LIBRARIES CGAL::CGAL)
    endif()
    if(SINDRECPP_UTILS3D_OPEN3D)
        find_package(Open3D CONFIG REQUIRED)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_LINK_LIBRARIES Open3D::Open3D)
    endif()
    if(SINDRECPP_UTILS3D_MESHLIB)
        find_package(meshlib CONFIG REQUIRED)
        if(TARGET MeshLib::MRMesh)
            set(sindrecpp_meshlib_target MeshLib::MRMesh)
        elseif(TARGET MRMesh)
            set(sindrecpp_meshlib_target MRMesh)
        else()
            message(FATAL_ERROR "MRMesh SDK must export MeshLib::MRMesh or MRMesh")
        endif()
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_LINK_LIBRARIES ${sindrecpp_meshlib_target})
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES
            "${MESHLIB_INCLUDE_DIR}")
        # MRMesh exports its own dependency include paths. Do not expose the
        # entire SDK thirdparty root: it also contains MeshLib's patched
        # pybind11, which shadows Utils_py's independent pybind11 installation.
        # MeshLib 3.x public headers use C++20.
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_COMPILE_FEATURES cxx_std_20)
    endif()
    if(SINDRECPP_UTILS3D_IGL)
        find_path(sindrecpp_igl_include igl/harmonic.h HINTS ${SINDRECPP_IGL_ROOT}/include REQUIRED)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${sindrecpp_igl_include})
    endif()
    if(SINDRECPP_UTILS3D_VCG)
        find_path(sindrecpp_vcg_include vcg/complex/complex.h HINTS ${SINDRECPP_VCG_ROOT} REQUIRED)
        set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_INCLUDE_DIRECTORIES ${sindrecpp_vcg_include})
    endif()
    foreach(backend IN ITEMS MESHLIB CGAL OPEN3D IGL VCG)
        if(SINDRECPP_UTILS3D_${backend})
            set_property(TARGET SindreCpp::Utils3d APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS SINDRECPP_UTILS3D_${backend}=1)
        endif()
    endforeach()
endif()


if(SINDRECPP_WITH_UTILS2D)
    # Reuse an installed SDK; do not build OpenCV and unrelated dependencies from source.
    find_package(OpenCV 4 REQUIRED COMPONENTS core imgproc imgcodecs)
    sindrecpp_add_module(Utils2d ${OpenCV_LIBS})
    set_property(TARGET SindreCpp::Utils2d APPEND PROPERTY
        INTERFACE_INCLUDE_DIRECTORIES "${OpenCV_INCLUDE_DIRS}")
endif()

if(SINDRECPP_WITH_AI)
    find_package(Threads REQUIRED)
    sindrecpp_add_module(Ai Threads::Threads)
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
            message(FATAL_ERROR "Install ONNX Runtime C/C++ SDK (1.22+) and set SINDRECPP_ONNXRUNTIME_ROOT")
        endif()
        add_library(sindrecpp_onnxruntime UNKNOWN IMPORTED GLOBAL)
        set_target_properties(sindrecpp_onnxruntime PROPERTIES
            IMPORTED_LOCATION "${sindrecpp_ort_library}"
            INTERFACE_INCLUDE_DIRECTORIES "${sindrecpp_ort_include}")
        set(sindrecpp_ort_target sindrecpp_onnxruntime)
    endif()
    sindrecpp_add_module(OnnxRuntime ${sindrecpp_ort_target} Threads::Threads)
    set_property(TARGET SindreCpp::OnnxRuntime APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS SINDRECPP_AI_ONNXRUNTIME=1 SINDRECPP_WITH_AI=1)
    set_property(TARGET SindreCpp::Ai APPEND PROPERTY INTERFACE_LINK_LIBRARIES SindreCpp::OnnxRuntime)
    if(SINDRECPP_AI_CUDA)
        set_property(TARGET SindreCpp::OnnxRuntime APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS SINDRECPP_AI_CUDA=1)
    endif()
endif()


    if(SINDRECPP_AI_TRT)
        find_package(CUDAToolkit 12 REQUIRED)
        if(TARGET TensorRT::nvinfer AND TARGET TensorRT::nvinfer_plugin AND TARGET TensorRT::nvonnxparser)
            set(sindrecpp_trt_targets TensorRT::nvinfer TensorRT::nvinfer_plugin TensorRT::nvonnxparser)
        else()
            find_path(sindrecpp_trt_include NvInfer.h
                HINTS "${SINDRECPP_TENSORRT_ROOT}" ENV TENSORRT_ROOT PATH_SUFFIXES include)
            if(NOT sindrecpp_trt_include)
                message(FATAL_ERROR "Set SINDRECPP_TENSORRT_ROOT to a TensorRT 10.13.x SDK")
            endif()
            set(sindrecpp_trt_targets)
            foreach(library IN ITEMS nvinfer nvinfer_plugin nvonnxparser)
                find_library(sindrecpp_${library}_library NAMES ${library} ${library}_10
                    HINTS "${SINDRECPP_TENSORRT_ROOT}" ENV TENSORRT_ROOT PATH_SUFFIXES lib lib64)
                if(NOT sindrecpp_${library}_library)
                    message(FATAL_ERROR "TensorRT SDK missing ${library}; set SINDRECPP_TENSORRT_ROOT")
                endif()
                add_library(sindrecpp_${library} UNKNOWN IMPORTED GLOBAL)
                set_target_properties(sindrecpp_${library} PROPERTIES
                    IMPORTED_LOCATION "${sindrecpp_${library}_library}"
                    INTERFACE_INCLUDE_DIRECTORIES "${sindrecpp_trt_include}")
                list(APPEND sindrecpp_trt_targets sindrecpp_${library})
            endforeach()
        endif()
        sindrecpp_add_module(Trt ${sindrecpp_trt_targets} CUDA::cudart Threads::Threads)
        set_property(TARGET SindreCpp::Trt APPEND PROPERTY INTERFACE_COMPILE_DEFINITIONS SINDRECPP_AI_TRT=1 SINDRECPP_WITH_AI=1)
        set_property(TARGET SindreCpp::Ai APPEND PROPERTY INTERFACE_LINK_LIBRARIES SindreCpp::Trt)
    endif()
endif()

if(SINDRECPP_WITH_GENERAL)
    add_library(SindreCpp::General INTERFACE IMPORTED GLOBAL)
    set(sindrecpp_general_targets sindrecpp_base)
    foreach(module IN ITEMS Pointer String Log Http Json Cli)
        string(TOUPPER "${module}" module_upper)
        if(SINDRECPP_WITH_${module_upper})
            list(APPEND sindrecpp_general_targets SindreCpp::${module})
        endif()
    endforeach()
    set_target_properties(SindreCpp::General PROPERTIES
        INTERFACE_LINK_LIBRARIES "${sindrecpp_general_targets}"
        INTERFACE_COMPILE_DEFINITIONS SINDRECPP_WITH_GENERAL=1
    )
endif()

# One target carries every enabled module. Consumers can still link an individual
# module target when they want to keep their dependency surface small.
add_library(SindreCpp::SindreCpp INTERFACE IMPORTED GLOBAL)
set(sindrecpp_all_targets)
if(SINDRECPP_WITH_GENERAL)
    list(APPEND sindrecpp_all_targets SindreCpp::General)
endif()
if(SINDRECPP_WITH_UTILS3D)
    list(APPEND sindrecpp_all_targets SindreCpp::Utils3d)
endif()
if(SINDRECPP_WITH_UTILS_GUI)
    list(APPEND sindrecpp_all_targets SindreCpp::Utils_gui)
endif()
if(SINDRECPP_WITH_UTILS_PY)
    list(APPEND sindrecpp_all_targets SindreCpp::Utils_py)
endif()
if(SINDRECPP_WITH_UTILS2D)
    list(APPEND sindrecpp_all_targets SindreCpp::Utils2d)
endif()
if(SINDRECPP_WITH_AI)
    list(APPEND sindrecpp_all_targets SindreCpp::Ai)
endif()
set_target_properties(SindreCpp::SindreCpp PROPERTIES
    INTERFACE_LINK_LIBRARIES "${sindrecpp_all_targets}"
    INTERFACE_COMPILE_FEATURES cxx_std_17
    INTERFACE_INCLUDE_DIRECTORIES ${CMAKE_CURRENT_SOURCE_DIR}/include
)

if(SINDRECPP_BUILD_TESTS)
    enable_testing()
    if(SINDRECPP_WITH_GENERAL)
        add_executable(sindrecpp_general_tests tests/general_tests.cpp)
        target_link_libraries(sindrecpp_general_tests PRIVATE SindreCpp::General)
        add_test(NAME sindrecpp.general COMMAND sindrecpp_general_tests)
    endif()
    if(SINDRECPP_WITH_UTILS2D)
        add_executable(sindrecpp_utils2d_tests tests/utils2d_tests.cpp)
        target_link_libraries(sindrecpp_utils2d_tests PRIVATE SindreCpp::Utils2d)
        add_test(NAME sindrecpp.utils2d COMMAND sindrecpp_utils2d_tests)
    endif()
    if(SINDRECPP_WITH_AI)
        add_executable(sindrecpp_execution_tests tests/execution_tests.cpp)
        target_link_libraries(sindrecpp_execution_tests PRIVATE SindreCpp::Ai)
        add_test(NAME sindrecpp.execution COMMAND sindrecpp_execution_tests)
        if(SINDRECPP_AI_ONNXRUNTIME)
            add_executable(sindrecpp_ai_tests tests/ai_tests.cpp)
            target_link_libraries(sindrecpp_ai_tests PRIVATE SindreCpp::OnnxRuntime)
            add_test(NAME sindrecpp.ai COMMAND sindrecpp_ai_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/models/identity.onnx)
        endif()
        if(SINDRECPP_AI_TRT AND SINDRECPP_BUILD_GPU_TESTS)
            add_executable(sindrecpp_trt_tests tests/trt_tests.cpp)
            target_link_libraries(sindrecpp_trt_tests PRIVATE SindreCpp::Trt)
            add_test(NAME sindrecpp.trt COMMAND sindrecpp_trt_tests ${CMAKE_CURRENT_SOURCE_DIR}/tests/models/identity.onnx)
        endif()
    endif()
    if(SINDRECPP_WITH_UTILS3D)
        add_executable(sindrecpp_utils3d_tests tests/utils3d_tests.cpp)
        target_link_libraries(sindrecpp_utils3d_tests PRIVATE SindreCpp::Utils3d)
        add_test(NAME sindrecpp.utils3d COMMAND sindrecpp_utils3d_tests)
        add_executable(sindrecpp_mesh_tests tests/mesh_tests.cpp)
        target_link_libraries(sindrecpp_mesh_tests PRIVATE SindreCpp::Utils3d)
        add_test(NAME sindrecpp.mesh COMMAND sindrecpp_mesh_tests)
        if(SINDRECPP_UTILS3D_VTK_DATA)
            add_executable(sindrecpp_vtk_data_tests tests/vtk_data_tests.cpp)
            target_link_libraries(sindrecpp_vtk_data_tests PRIVATE SindreCpp::Utils3d)
            add_test(NAME sindrecpp.vtk_data COMMAND sindrecpp_vtk_data_tests)
        endif()
        if(SINDRECPP_UTILS3D_SHOW)
            add_executable(sindrecpp_show_mesh_tests tests/show_mesh_tests.cpp)
            target_link_libraries(sindrecpp_show_mesh_tests PRIVATE SindreCpp::Utils3d)
            add_test(NAME sindrecpp.show_mesh COMMAND sindrecpp_show_mesh_tests)
        endif()
        if(SINDRECPP_WITH_UTILS_PY)
            add_executable(sindrecpp_mesh_numpy_tests tests/mesh_numpy_tests.cpp)
            target_link_libraries(sindrecpp_mesh_numpy_tests PRIVATE SindreCpp::Utils3d SindreCpp::Utils_py)
            add_test(NAME sindrecpp.mesh_numpy COMMAND sindrecpp_mesh_numpy_tests)
        endif()
    endif()
endif()

if(SINDRECPP_BUILD_UTILS3D_BENCHMARKS)
    if(NOT SINDRECPP_WITH_UTILS3D)
        message(FATAL_ERROR "SINDRECPP_BUILD_UTILS3D_BENCHMARKS requires SINDRECPP_WITH_UTILS3D=ON")
    endif()
    if(NOT TARGET BLAS::BLAS)
        find_package(BLAS QUIET)
    endif()
    if(TARGET BLAS::BLAS)
        add_executable(sindrecpp_utils3d_benchmark tests/utils3d_benchmark.cpp)
        target_link_libraries(sindrecpp_utils3d_benchmark PRIVATE Eigen3::Eigen BLAS::BLAS)
        if(BLA_VENDOR)
            target_compile_definitions(sindrecpp_utils3d_benchmark PRIVATE SINDRECPP_BENCHMARK_BLAS_VENDOR="${BLA_VENDOR}")
        else()
            target_compile_definitions(sindrecpp_utils3d_benchmark PRIVATE SINDRECPP_BENCHMARK_BLAS_VENDOR="detected BLAS")
        endif()
        if(SINDRECPP_UTILS3D_NATIVE_ARCH)
            target_link_libraries(sindrecpp_utils3d_benchmark PRIVATE sindrecpp_utils3d_native_arch)
        endif()
    else()
        message(WARNING "SINDRECPP_BUILD_UTILS3D_BENCHMARKS requested, but BLAS was not found; benchmark target is skipped")
    endif()
endif()

if(SINDRECPP_BUILD_EXAMPLES)
    if(SINDRECPP_WITH_UTILS3D AND SINDRECPP_UTILS3D_SHOW)
        add_executable(sindrecpp_show_mesh examples/show_mesh.cpp)
        target_link_libraries(sindrecpp_show_mesh PRIVATE SindreCpp::Utils3d)
    endif()
    add_executable(sindrecpp_basic examples/basic_usage.cpp)
    target_link_libraries(sindrecpp_basic PRIVATE SindreCpp::SindreCpp)
    if(SINDRECPP_BUILD_TESTS)
        if(WIN32 AND SINDRECPP_WITH_UTILS_PY)
            add_test(NAME sindrecpp.modules COMMAND ${CMAKE_COMMAND} -E env
                "PATH=${Python3_RUNTIME_LIBRARY_DIRS};$ENV{PATH}" $<TARGET_FILE:sindrecpp_basic>)
        else()
            add_test(NAME sindrecpp.modules COMMAND sindrecpp_basic)
        endif()
    endif()
endif()
