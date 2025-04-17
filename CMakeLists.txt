cmake_minimum_required(VERSION 3.10)

project(ECHO)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

add_subdirectory(external/glfw-3.4)

find_package(Vulkan REQUIRED)
# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
# find_package(GLFW REQUIRED)


# include_directories(${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})

# 设置可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 设置库文件的输出目录（如果有）
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 设置归档文件的输出目录（如果有）
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

file(GLOB SOURCES
    src/*.cpp
)

message(STATUS "GLFW_INCLUDE_DIR: ${GLFW_INCLUDE_DIR}")
message(STATUS "GLFW_LIBRARY: ${GLFW_LIBRARY}")
message(STATUS "Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan_DIRS: ${Vulkan_LIBRARIES_DIRS}")
message(STATUS "Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
message(STATUS "GLFW_LIBRARIES: ${GLFW_LIBRARIES}")

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE ${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIR})

# 添加 GLM 的头文件路径
set(GLM_INCLUDE_DIR "external/glm-1.0.1-light") 
set(STB_INCLUDE_DIR "external/stb_master") 
set(Vulkan_LIBRARIES ${Vulkan_LIBRARIES} "${Vulkan_INCLUDE_DIRS}/../Lib/shaderc_shared.lib")
set(GLFW_LIBRARY glfw)

target_include_directories(${PROJECT_NAME} PRIVATE ${GLM_INCLUDE_DIR} ${STB_INCLUDE_DIR})

target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES} ${GLFW_LIBRARY})

# set(SHADER_FILES
#     shaders/shader.vert
#     shaders/shader.frag
# )

# foreach(SHADER ${SHADER_FILES})
#     get_filename_component(SHADER_NAME ${SHADER} NAME_WE)
#     add_custom_command(
#         OUTPUT ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv
#         COMMAND glslangValidator -V ${SHADER} -o ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv
#         DEPENDS ${SHADER}
#         COMMENT "Building ${SHADER_NAME}.spv"
#     )
#     list(APPEND SPIRV_FILES ${CMAKE_CURRENT_BINARY_DIR}/${SHADER_NAME}.spv)
# endforeach()

# add_custom_target(shaders DEPENDS ${SPIRV_FILES})

# add_dependencies(${PROJECT_NAME} shaders)