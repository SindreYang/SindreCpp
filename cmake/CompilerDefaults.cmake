include_guard(GLOBAL)

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

    if(MSVC AND SINDRE_MSVC_STATIC_RUNTIME)
        set_property(TARGET ${target} PROPERTY
            MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
    endif()
endfunction()
