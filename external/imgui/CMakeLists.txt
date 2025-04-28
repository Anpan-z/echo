# external/imgui/CMakeLists.txt


file(GLOB IMGUI_SOURCES
    *.cpp
    backends/imgui_impl_vulkan.cpp
    backends/imgui_impl_glfw.cpp
)

# Just create an object library instead of static/shared
add_library(imgui OBJECT ${IMGUI_SOURCES})

target_include_directories(imgui PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/backends
    ${Vulkan_INCLUDE_DIRS}
    glfw
)

# find_package(glfw3 REQUIRED)
target_link_libraries(imgui PUBLIC glfw)

