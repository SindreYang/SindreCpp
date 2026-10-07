cmake_minimum_required(VERSION 3.20)

# The examples are intentionally separate targets. This keeps each example's
# dependency surface visible and lets users configure an example directory on
# its own, while the root project can still build all enabled examples.
add_subdirectory(general_basics)

if(SINDRE_WITH_JSON)
    add_subdirectory(general_json_config)
else()
    message(STATUS "Skipping general_json_config: enable SINDRE_WITH_JSON=ON")
endif()

if(SINDRE_WITH_UTILS_3D)
    add_subdirectory(utils_3d_mesh)
else()
    message(STATUS "Skipping utils_3d_mesh: enable SINDRE_WITH_UTILS_3D=ON")
endif()
