if(NOT DEFINED SINDRE_RUNTIME_DEST OR NOT DEFINED SINDRE_RUNTIME_FILES)
    return()
endif()

foreach(runtime_file IN LISTS SINDRE_RUNTIME_FILES)
    if(EXISTS "${runtime_file}")
        file(COPY "${runtime_file}" DESTINATION "${SINDRE_RUNTIME_DEST}")
    endif()
endforeach()
