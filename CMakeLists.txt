cmake_minimum_required(VERSION 3.10)

project(ECHO)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

if(MSVC)
    message(STATUS "Using MSVC compiler")
    set(CMAKE_GENERATOR_TOOLSET "v143") # 指定 MSVC 工具集版本，例如 v143 对应 Visual Studio 2022
endif(MSVC)

find_package(Vulkan REQUIRED)

add_subdirectory(external/glfw-3.4)
add_subdirectory(external/imgui)

# set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_SOURCE_DIR}/cmake")
# find_package(GLFW REQUIRED)


# include_directories(${Vulkan_INCLUDE_DIRS} ${GLFW_INCLUDE_DIRS})

# 设置可执行文件的输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

# 设置库文件的输出目录（如果有）
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 设置归档文件的输出目录（如果有）
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 递归收集所有源文件（cpp），包含子目录
file(GLOB_RECURSE SOURCES CONFIGURE_DEPENDS
    src/*.cpp
)

# 递归收集所有头文件（h），包含子目录
file(GLOB_RECURSE HEADER_DIRS CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/*
)

# 从所有文件路径中提取唯一目录（防止路径重复）
set(INCLUDE_DIRS "")
foreach(path ${HEADER_DIRS})
    get_filename_component(dir ${path} DIRECTORY)
    list(APPEND INCLUDE_DIRS ${dir})
endforeach()
list(REMOVE_DUPLICATES INCLUDE_DIRS)

message(STATUS "GLFW_INCLUDE_DIR: ${GLFW_INCLUDE_DIR}")
message(STATUS "GLFW_LIBRARY: ${GLFW_LIBRARY}")
message(STATUS "Vulkan_INCLUDE_DIRS: ${Vulkan_INCLUDE_DIRS}")
message(STATUS "Vulkan_DIRS: ${Vulkan_LIBRARIES_DIRS}")
message(STATUS "Vulkan_LIBRARIES: ${Vulkan_LIBRARIES}")
message(STATUS "GLFW_LIBRARIES: ${GLFW_LIBRARIES}")

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# 添加 GLM 的头文件路径
set(GLM_INCLUDE_DIR "external/glm-1.0.1-light") 
set(STB_INCLUDE_DIR "external/stb_master") 

# 添加 include 路径（再加上外部依赖）
target_include_directories(${PROJECT_NAME} PRIVATE
    ${INCLUDE_DIRS}
    ${Vulkan_INCLUDE_DIRS}
    ${GLFW_INCLUDE_DIR}
    ${GLM_INCLUDE_DIR}
    ${STB_INCLUDE_DIR}
)

set(Vulkan_LIBRARIES ${Vulkan_LIBRARIES} "${Vulkan_INCLUDE_DIRS}/../Lib/shaderc_shared.lib")
set(GLFW_LIBRARY glfw)

target_link_libraries(${PROJECT_NAME} ${Vulkan_LIBRARIES} ${GLFW_LIBRARY} imgui)

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