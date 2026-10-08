cmake_minimum_required(VERSION 3.20)

# The examples are intentionally separate targets. This keeps each example's
# dependency surface visible and lets users configure an example directory on
# its own, while the root project can still build all enabled examples.
add_subdirectory(general_basics)

add_subdirectory(general_json_config)

if(SINDRE_WITH_UTILS_3D)
    add_subdirectory(utils_3d_mesh)
else()
    message(STATUS "Skipping utils_3d_mesh: enable SINDRE_WITH_UTILS_3D=ON")
endif()

if(SINDRE_WITH_UTILS_2D)
    add_subdirectory(utils_2d_image)
else()
    message(STATUS "Skipping utils_2d_image: enable SINDRE_WITH_UTILS_2D=ON")
endif()

if(SINDRE_WITH_AI AND SINDRE_AI_TRT)
    add_subdirectory(ai_tensorrt_segmentation)
else()
    message(STATUS "Skipping ai_tensorrt_segmentation: enable SINDRE_WITH_AI=ON and SINDRE_AI_TRT=ON")
endif()
