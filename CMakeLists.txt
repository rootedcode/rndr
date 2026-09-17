cmake_minimum_required(VERSION 4.3.3)
set(CMAKE_EXPORT_COMPILE_COMMANDS true)
set(CMAKE_C_STANDARD 23)
set(CMAKE_C_STANDARD_REQUIRED true)
set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED true)

project(rndr
    VERSION 0.1.0
    LANGUAGES C CXX
)

find_package(glfw3 CONFIG REQUIRED)
find_package(Vulkan REQUIRED)
find_program(GLSLANG_VALIDATOR glslangValidator REQUIRED)

set(SHADER_DIR ${CMAKE_SOURCE_DIR}/shaders)
set(VERT_SHADER ${SHADER_DIR}/basic_triangle.vert)
set(FRAG_SHADER ${SHADER_DIR}/basic_triangle.frag)
set(VERT_SPV ${CMAKE_BINARY_DIR}/shaders/basic_triangle.vert.spv)
set(FRAG_SPV ${CMAKE_BINARY_DIR}/shaders/basic_triangle.frag.spv)

add_custom_command(
    OUTPUT ${VERT_SPV} ${FRAG_SPV}
    COMMAND ${CMAKE_COMMAND} -E make_directory ${CMAKE_BINARY_DIR}/shaders
    COMMAND ${GLSLANG_VALIDATOR} -V ${VERT_SHADER} -o ${VERT_SPV}
    COMMAND ${GLSLANG_VALIDATOR} -V ${FRAG_SHADER} -o ${FRAG_SPV}
    DEPENDS ${VERT_SHADER} ${FRAG_SHADER}
    COMMENT "Compiling Vulkan shaders"
    VERBATIM
)

add_custom_target(rndr_shaders DEPENDS ${VERT_SPV} ${FRAG_SPV})

include(FetchContent)
FetchContent_Declare(
    vk_bootstrap
    GIT_REPOSITORY https://github.com/charles-lunarg/vk-bootstrap
    GIT_TAG 9064ea7213ed160e9793eacc39ee68d957b55fad
)
FetchContent_MakeAvailable(vk_bootstrap)

add_executable(rndr)
target_sources(rndr
    PRIVATE
        src/rndr_main.cpp
        src/application.cpp
        src/multithreading.cpp
        src/window.cpp
        src/vulkan.cpp
    PRIVATE
        FILE_SET HEADERS
        BASE_DIRS include
        FILES
            include/rndr/application.hpp
            include/rndr/multithreading.hpp
            include/rndr/window.hpp
            include/rndr/vulkan.hpp
)

target_link_libraries(rndr
    PRIVATE
        glfw
        Vulkan::Vulkan
        vk-bootstrap
)

add_dependencies(rndr rndr_shaders)