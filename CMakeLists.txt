cmake_minimum_required(VERSION 3.0.0)

project(ControllerImage)

if(NOT TARGET SDL3::Headers OR NOT TARGET ${sdl3_target_name})
    find_package(SDL3 REQUIRED)
endif()

add_library(controllerimage SHARED
    src/controllerimage.c
)
target_include_directories(controllerimage PRIVATE
    src
    ${SDL3_INCLUDE_DIR}
)
set_target_properties(controllerimage PROPERTIES C_VISIBILITY_PRESET "hidden")
target_link_libraries(controllerimage ${SDL3_LIBRARIES})

add_executable(make-controllerimage-data src/make-controllerimage-data.c)
add_executable(demo-controllerimage src/demo-controllerimage.c)
target_link_libraries(demo-controllerimage controllerimage ${SDL3_LIBRARIES})


