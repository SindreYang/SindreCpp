if(NOT DEFINED SINDRECPP_RUNTIME_DEST OR NOT DEFINED SINDRECPP_RUNTIME_FILES)
    return()
endif()

foreach(runtime_file IN LISTS SINDRECPP_RUNTIME_FILES)
    if(EXISTS "${runtime_file}")
        file(COPY "${runtime_file}" DESTINATION "${SINDRECPP_RUNTIME_DEST}")
    endif()
endforeach()
