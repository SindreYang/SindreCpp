include_guard(GLOBAL)

# 编译器默认策略：统一警告、C++ 一致性、异常开关和 MSVC 运行时库设置。
# 这些选项通过 target 传播，避免修改宿主项目的全局编译参数。

function(sindre_apply_compiler_defaults target)
    if(NOT TARGET ${target})
        message(FATAL_ERROR "Cannot apply compiler defaults to unknown target '${target}'")
    endif()

    if(SINDRE_ENABLE_WARNINGS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang")
            if(MSVC)
                # clang-cl accepts the MSVC-compatible warning and conformance flags.
                target_compile_options(${target} INTERFACE
                    /W4 /permissive- /Zc:__cplusplus /utf-8 /bigobj)
            else()
                target_compile_options(${target} INTERFACE
                    -Wall -Wextra -Wpedantic)
            endif()
        elseif(MSVC)
            target_compile_options(${target} INTERFACE
                /W4 /permissive- /Zc:__cplusplus /utf-8 /bigobj /MP)
        elseif(CMAKE_CXX_COMPILER_ID STREQUAL "GNU")
            target_compile_options(${target} INTERFACE
                -Wall -Wextra -Wpedantic)
        endif()
    endif()

    if(SINDRE_WARNINGS_AS_ERRORS)
        if(CMAKE_CXX_COMPILER_ID MATCHES "Clang|GNU")
            target_compile_options(${target} INTERFACE -Werror)
        elseif(MSVC)
            target_compile_options(${target} INTERFACE /WX)
        endif()
    endif()

    # MSVC 默认不启用完整的 C++ 异常展开，而 General 的 Result 边界会
    # 在启用异常时捕获第三方库异常。显式启用 /EHsc，避免 C4530 警告并
    # 保证异常对象在跨库调用时可以正确析构。
    if(MSVC AND NOT SINDRE_NO_EXCEPTIONS)
        target_compile_options(${target} INTERFACE /EHsc)
    endif()

    if(MSVC AND SINDRE_MSVC_STATIC_RUNTIME)
        # General 的固定依赖全部使用非 Debug 静态 CRT。即使宿主选择 Debug，
        # 也必须保持 /MT，不能生成 /MTd 后再与固定 /MT 库混链。
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded")
    endif()
endfunction()
