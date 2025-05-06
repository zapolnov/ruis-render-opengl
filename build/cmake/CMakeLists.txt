cmake_minimum_required(VERSION 3.30)

set(name ruis-render-opengl)
project(${name})

if(NOT IGAGIS_CMAKE_COMMON_INCLUDED)
    set(dir "${CMAKE_CURRENT_SOURCE_DIR}/../../tool-configs/cmake")
    if(NOT EXISTS "${dir}/common.cmake")
        if(DEFINED ENV{IGAGIS_TOOL_CONFIGS})
            set(url "$ENV{IGAGIS_TOOL_CONFIGS}")
        else()
            set(url "https://github.com/cppfw/tool-configs")
        endif()
        file(MAKE_DIRECTORY "${dir}")
        file(DOWNLOAD "${url}/raw/refs/heads/main/cmake/common.cmake" "${dir}/common.cmake")
    endif()
    include("${dir}/common.cmake")
endif()

set(srcs)
add_source_directory(srcs
    "${CMAKE_CURRENT_SOURCE_DIR}/../../src/ruis"
    PATTERNS "*.cpp" "*.hpp"
    RECURSIVE
)

declare_library(${name}
    SOURCES
        ${srcs}
    PUBLIC_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src"
    INSTALL_INCLUDE_DIRECTORIES
        "${CMAKE_CURRENT_SOURCE_DIR}/../../src/ruis"
    DEPENDENCIES
        ruis
    EXTERNAL_DEPENDENCIES
        GLEW
)
