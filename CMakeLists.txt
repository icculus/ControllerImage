cmake_minimum_required(VERSION 3.0.0)

project(ControllerImage)

find_package(SDL3 CONFIG REQUIRED)

add_library(controllerimage SHARED
    src/controllerimage.c
)
target_include_directories(controllerimage PRIVATE
    src
    ${SDL3_INCLUDE_DIR}
)
set_target_properties(controllerimage PROPERTIES C_VISIBILITY_PRESET "hidden")
target_link_libraries(controllerimage PRIVATE ${SDL3_LIBRARIES})

add_executable(make-controllerimage-data src/make-controllerimage-data.c)
add_executable(demo-controllerimage src/demo-controllerimage.c)
target_link_libraries(demo-controllerimage controllerimage ${SDL3_LIBRARIES})


